#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "HttpServer.hpp"
#include "get_ip.hpp"
#include "strutil.hpp"
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#define RETRY_COUNT 5

#ifdef VERBOSE
static bool verbose = true;
#else
static bool verbose = false;
#endif

//--------------------HttpResponse-------------------------------

HttpRequest::HttpRequest(std::string text) {
  using namespace strutil;
  std::string::size_type n = text.find("\r\n\r\n");
  std::string header_string(text, 0, n);
  std::vector<std::string> headers = split(header_string, "\r\n");
  auto it = headers.begin();
  std::string status_line(*it);
  std::vector<std::string> status_line_components = split(status_line, " ");
  _method = status_line_components[0];
  _route = status_line_components[1];
  it++;
  for (; it != headers.end(); ++it) {
    auto tmp = strutil::split(*it, ": ");
    _headers.insert_or_assign(lowers(trim(tmp[0])), lowers(trim(tmp[1])));
  }
}

std::string HttpRequest::body() const { return _body; }
std::string HttpRequest::method() const { return _method; }
std::string HttpRequest::route() const { return _route; }
std::map<std::string, std::string> HttpRequest::headers() const {
  return _headers;
}

//---------------------------------------------------------------

//--------------------HttpResponse-------------------------------

static std::string get_status_msg(const uint8_t &status_code) {
  std::map<uint8_t, std::string> codes = {{200, "OK"},
                                          {301, "Moved Permanently"},
                                          {302, "Found"},
                                          {400, "Bad Request"},
                                          {404, "Not Found"}};
  return codes.at(status_code);
}

HttpResponse::HttpResponse() { _status_code = 200; }

void HttpResponse::text(const std::string &msg) {
  this->set_header("Content-Type", "text/plain");
  _body = msg;
}

void HttpResponse::static_file(const std::string &path) {
  _body = strutil::slurp(path);
}

/**
 * Send an image as a response
 * Sets the content-type to image/png by default
 *
 * @param path the path to the image
 *
 */
void HttpResponse::image(const std::string &path) {
  this->set_header("Content-Type", "image/png");
  std::ifstream fin(path, std::ios::in | std::ios::binary);
  std::ostringstream oss;
  oss << fin.rdbuf();
  _body = oss.str();
}

/**
 * Send an image as a response
 * Sets the content-type to image/png by default
 *
 * @param path the path to the image
 * @param type the type of image (png/x-icon/jpg etc.)
 *
 */
void HttpResponse::image(const std::string &path, const std::string &type) {
  this->set_header("Content-Type", "image/" + type);
  std::ifstream fin(path, std::ios::in | std::ios::binary);
  std::ostringstream oss;
  oss << fin.rdbuf();
  _body = oss.str();
}

void HttpResponse::html_string(const std::string &html_string) {
  this->set_header("Content-Type", "text/html");
  _body = html_string;
}

void HttpResponse::html(const std::string &path) {
  this->set_header("Content-Type", "text/html");
  this->static_file(path);
}

std::string HttpResponse::get_headers() const {
  std::string res(fmt::format("HTTP/1.1 {} {}\r\n", _status_code,
                              get_status_msg(_status_code)));
  for (const auto &[k, v] : _headers) {
    res.append(fmt::format("{}: {}\r\n", k, v));
  }
  if (!_body.empty()) {
    res.append(fmt::format("Content-Length: {}", _body.length()));
  }
  res.append("\r\n\r\n");
  return res;
}

std::string HttpResponse::get_full_response() const {
  return this->get_headers() + _body;
}

void HttpResponse::set_status_code(const int &status_code) {
  _status_code = status_code;
}

void HttpResponse::set_header(const std::string &key,
                              const std::string &value) {
  _headers.insert_or_assign(key, value);
}

//---------------------------------------------------------------

// Have to re-declare static class variables in the source file
volatile sig_atomic_t HttpServer::_run;

void HttpServer::intHandler(int) { _run = 0; }

HttpServer::HttpServer() {
  _run = 1;
  _numListeners = 3;
  HttpResponse not_found_res;
  not_found_res.set_status_code(404);
  not_found_res.text("Wilson's Server: page request is not found");
  _notFoundPage = not_found_res;
}

void HttpServer::get(const std::string &route, routeFunc func) {
  if (!_static_directory_path.empty()) {
    throw std::invalid_argument(
        "Cannot define routes while in static directory serving mode");
  }
  _routes.insert_or_assign(route, func);
}

void HttpServer::_get(const std::string &route, routeFunc func) {
  _routes.insert_or_assign(route, func);
}

HttpServer HttpServer::setNumListeners(int num_listeners) {
  HttpServer tmp = *this;
  tmp._numListeners = num_listeners;
  return tmp;
}

HttpServer HttpServer::set404Page(const std::string &path) {
  HttpServer tmp = *this;
  HttpResponse res;
  res.html(path);
  tmp._notFoundPage = res;
  tmp._notFoundPage.set_status_code(404);
  return tmp;
}

HttpServer HttpServer::set404Text(const std::string &message) {
  HttpServer tmp = *this;
  HttpResponse res;
  res.text(message);
  tmp._notFoundPage = res;
  tmp._notFoundPage.set_status_code(404);
  return tmp;
}

HttpServer HttpServer::set404Response(HttpResponse res) {
  HttpServer tmp = *this;
  tmp._notFoundPage = res;
  tmp._notFoundPage.set_status_code(404);
  return tmp;
}

HttpServer HttpServer::static_directory_path(const std::string &path) {
  HttpServer tmp = *this;
  tmp._static_directory_path = path;
  if (tmp._static_directory_path.back() != '/') {
    tmp._static_directory_path += "/";
  }
  return tmp;
}

static std::unique_ptr<sockaddr_in> get_sa(uint16_t port) {
  auto sa = std::make_unique<struct sockaddr_in>();
  sa->sin_family = AF_INET;
  sa->sin_port = htons(port);
  sa->sin_addr.s_addr = htonl(INADDR_ANY);
  return sa;
}

static void handle_request_body(int connfd, HttpRequest &req) {
  unsigned long size_to_read;
  try {
    size_to_read = std::stoul(req.headers().at("content-length"));
  } catch (std::out_of_range const &) {
    return;
  }
  char *buf = new char[size_to_read + 1];
  memset(buf, 0, size_to_read + 1);
  read(connfd, buf, size_to_read);
  req.body().append(buf);
  std::cout << "recieved: " << buf << '\n';
  delete[] buf;
}

void HttpServer::handle_requests(const HttpRequest &request) const {
  if (_routes.find(request.route()) != _routes.end()) {
    if (verbose) {
      std::cout << fmt::format("Route {} matched!", request.route())
                << std::endl;
    }
    auto func = _routes.at(request.route());
    HttpResponse res;
    func(request, res);
    std::string reply = res.get_full_response();
    write(_connfd, reply.c_str(), reply.length());
    close(_connfd);
  } else {
    if (verbose) {
      std::cout << fmt::format("Route {} doesn't match any mappings.",
                               request.route())
                << std::endl;
    }
    std::string reply = _notFoundPage.get_full_response();
    write(_connfd, reply.c_str(), reply.length());
    close(_connfd);
  }
}

void HttpServer::staticSetup() {
  auto root = std::filesystem::path(_static_directory_path);
  if (!is_directory(root)) {
    throw std::invalid_argument(
        "static directory path has to point to a directory");
  }
  if (root.empty()) {
    throw std::invalid_argument("given directory is empty");
  }
  if (!std::ifstream(root / "index.html").good()) {
    throw std::invalid_argument(
        "index.html does not exist in the root directory of the static folder");
  }
  this->_get("/", [=](const HttpRequest &req, HttpResponse &res) {
    res.html(root / "index.html");
  });
  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(_static_directory_path)) {
    std::string extension(entry.path().extension());
    if (!is_regular_file(entry.path())) {
      continue;
    }
    std::string file_name = std::filesystem::relative(entry.path(), root);
    this->_get("/" + file_name, [=](const HttpRequest &req, HttpResponse &res) {
      std::string content_type;
      if (extension == ".css") {
        content_type = "text/css";
      } else if (extension == ".js") {
        content_type = "text/javascript";
      } else if (extension == ".html") {
        content_type = "text/html";
      } else if (extension == ".ico") {
        res.image(entry.path(), "x-icon");
        return;
      } else if (extension == ".svg") {
        res.image(entry.path(), "svg+xml");
        return;
      } else if (extension == ".txt") {
        content_type = "text/plain";
      } else if (extension == ".json" || extension == ".map") {
        content_type = "application/json";
      } else if (extension == ".png") {
        res.image(entry.path());
        return;
      } else {
        throw std::runtime_error(fmt::format(
            "staticSetup: static file extension {} not supported", extension));
      }
      res.set_header("Content-Type", content_type);
      res.static_file(entry.path());
    });
  }
}

void HttpServer::listenAndRun(const std::uint16_t &port) {
  struct sigaction sigAction;
  sigAction.sa_flags = 0;
  sigAction.sa_handler = intHandler;
  sigaction(SIGINT, &sigAction, NULL);
  _listenfd = socket(AF_INET, SOCK_STREAM, 0);
  int flags = fcntl(_listenfd, F_GETFL); // Get the socket's current flags
  fcntl(_listenfd, F_SETFL,
        flags | O_NONBLOCK); // Set the socket to be non-blocking
  // Set the socket to be reusable instantly; Violates TCP/IP protocol?
  /* int iSetOption = 1;
  setsockopt(_listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&iSetOption,
             sizeof(iSetOption)); */
  if (_listenfd == -1) {
    throw std::runtime_error("Failed to create socket");
  }
  auto sa = get_sa(port);

  int bind_retry_count = 0;
  int bind_status;
  do {
    bind_status = bind(_listenfd, reinterpret_cast<sockaddr *>(sa.get()),
                       sizeof(sockaddr));
    if (bind_status == -1) {
      std::cout << fmt::format(
          "Binding to port {} failed. Retry count: {}/{}\n", port,
          ++bind_retry_count, RETRY_COUNT);
      std::cout << fmt::format("Waiting {}s before retrying...\n",
                               bind_retry_count);
      sleep(1 * bind_retry_count);
    }
  } while (bind_status == -1 && bind_retry_count < RETRY_COUNT);

  if (bind_status == -1) {
    std::cout << std::strerror(errno) << '\n';
    throw std::runtime_error("Unable to bind");
  }

  if (listen(_listenfd, _numListeners) == -1) {
    throw std::runtime_error("unable to listen");
  }

  std::string msg = fmt::format("Now listening at port: {} with {} listeners\n",
                                port, _numListeners);
  std::cout << msg;

  while (_run) {
    // create a sockaddr struct to store client information
    sockaddr_in client_sa = {0}; // zero out the struct
    socklen_t client_sa_len =
        sizeof(sockaddr); // must initialize value to size of struct

    // accept is now non-blocking as _listenfd is set to be non-blocking
    _connfd = accept(_listenfd, reinterpret_cast<sockaddr *>(&client_sa),
                     &client_sa_len);
    if (_connfd == -1) {
      if (errno == EWOULDBLOCK) {
        usleep(500);
      } else {
        close(_connfd);
        close(_listenfd);
        std::cout << "\nsockets closed. Exiting..." << std::endl;
        throw std::runtime_error("error accepting connection");
      }
    } else if (verbose) {
      char client_address[INET_ADDRSTRLEN] = {0};
      inet_ntop(client_sa.sin_family, &client_sa.sin_addr, client_address,
                INET_ADDRSTRLEN);
      std::cout << fmt::format("Recieved connection from ip address: {}",
                               client_address)
                << std::endl;
    }

    char buf[2] = {0};
    std::string request_string;
    while (_run && (read(_connfd, buf, 1) > 0)) {
      request_string.append(buf);
      if (strutil::contains(request_string, "\r\n\r\n")) {
        break;
      }
    }
    if (request_string.empty())
      continue;
    HttpRequest request(request_string);
    handle_request_body(_connfd, request);

    std::cout << fmt::format("Recieved request for route: {}", request.route())
              << std::endl;
    if (!_static_directory_path.empty()) {
      staticSetup();
    }
    handle_requests(request);
  }
  close(_connfd);
  close(_listenfd);
  std::cout << "\nsockets closed. Exiting..." << std::endl;
}
