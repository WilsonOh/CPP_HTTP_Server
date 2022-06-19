#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <fmt/core.h>
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
#include <string>
#include <vector>

//--------------------HttpResponse-------------------------------

HttpRequest::HttpRequest(std::string text) {
  using namespace strutil;
  std::string::size_type n = text.find("\r\n\r\n");
  std::string header_string(text, 0, n);
  std::vector<std::string> headers = split(header_string, "\r\n");
  auto it = headers.begin();
  std::string status_line(*it);
  std::vector<std::string> status_line_components =
      split(status_line, " ");
  _method = status_line_components[0];
  _route = status_line_components[1];
  it++;
  for (; it != headers.end(); ++it) {
    auto tmp = strutil::split(*it, ": ");
    _headers.insert_or_assign(lowers(trim(tmp[0])), lowers(trim(tmp[1])));
  }
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

void HttpResponse::text(const std::string &msg) { _body = msg; }

void HttpResponse::static_file(const std::string &path) {
  _body = strutil::slurp(path);
}

std::string HttpResponse::get_full_response() {
  std::string res(fmt::format("HTTP/1.1 {} {}\r\n", _status_code,
                              get_status_msg(_status_code)));
  for (const auto &[k, v] : _headers) {
    res.append(fmt::format("{}: {}\r\n", k, v));
  }
  if (!_body.empty()) {
    res.append(fmt::format("Content-Length: {}", _body.length()));
  }
  res.append("\r\n\r\n");
  res.append(_body);
  return res;
}

void HttpResponse::set_status_code(const int &status_code) { _status_code = status_code; }

void HttpResponse::set_header(const std::string &key,
                              const std::string &value) {
  _headers.insert_or_assign(key, value);
}

//---------------------------------------------------------------

// Have to re-declare static class variables in the source file
volatile sig_atomic_t HttpServer::_run;

void HttpServer::intHandler(int) { _run = 0; }

HttpServer::HttpServer() { _run = 1; }

HttpServer::~HttpServer() {
  close(_connfd);
  close(_listenfd);
  std::cout << "\nsockets closed. Exiting..." << std::endl;
}

void HttpServer::get(const std::string &route, routeFunc func) {
  _routes.insert_or_assign(route, func);
}

static std::unique_ptr<sockaddr_in> get_sa(uint16_t port) {
  auto sa = std::make_unique<struct sockaddr_in>();
  sa->sin_family = AF_INET;
  sa->sin_port = htons(port);
  sa->sin_addr.s_addr = htonl(INADDR_ANY);
  return sa;
}

static void handle_request_body(int connfd, HttpRequest &req) {
  if (req._headers.find("content-length") == req._headers.end()) return;
  unsigned long size_to_read = std::stoul(req._headers.at("content-length"));
  char *buf = new char[size_to_read + 1];
  memset(buf, 0, size_to_read + 1);
  read(connfd, buf, size_to_read);
  req._body.append(buf);
  delete [] buf;
}

void HttpServer::listenAndRun(const std::uint16_t &port) {
  signal(SIGINT, HttpServer::intHandler);
  signal(SIGABRT, HttpServer::intHandler);
  _listenfd = socket(AF_INET, SOCK_STREAM, 0);
  int flags = fcntl(_listenfd, F_GETFL); // Get the socket's current flags
  fcntl(_listenfd, F_SETFL,
        flags | O_NONBLOCK); // Set the socket to be non-blocking
  if (_listenfd == -1) {
    throw std::runtime_error("Failed to create socket");
  }
  auto sa = get_sa(port);
  if (bind(_listenfd, reinterpret_cast<sockaddr *>(sa.get()),
           sizeof(sockaddr)) == -1) {
    std::cout << errno << '\n';
    throw std::runtime_error("Unable to bind");
  }
  if (listen(_listenfd, 10) == -1) {
    throw std::runtime_error("unable to listen");
  }

  std::cout << "Now listening at port: " << port << '\n';

  while (_run) {
    // accept is now non-blocking as _listenfd is set to be non-blocking
    _connfd = accept(_listenfd, nullptr, nullptr);
    if (_connfd == -1) {
      if (errno == EWOULDBLOCK) {
        sleep(1);
      } else {
        throw std::runtime_error("error accepting connection");
      }
    }
    char buf[2] = {0};
    std::string request_string;
    // NOTE: This only handles the headers,
    // body handling is not yet implemented.
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
    if (_routes.find(request._route) != _routes.end()) {
      auto func = _routes.at(request._route);
      HttpResponse res;
      func(request, res);
      std::string reply = res.get_full_response();
      write(_connfd, reply.c_str(), reply.length());
      close(_connfd);
    } else {
      HttpResponse not_found_res;
      not_found_res.set_status_code(404);
      not_found_res.text("Wilson's Server: page request is not found");
      std::string reply = not_found_res.get_full_response();
      write(_connfd, reply.c_str(), reply.length());
      close(_connfd);
    }
  }
}
