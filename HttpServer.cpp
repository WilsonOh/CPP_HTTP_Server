#include "HttpServer.hpp"

/**
 * Global boolean value to determine whether or not to print out verbose
 * debugging messages Compile with -DVERBOSE to set this to true
 */
#ifdef VERBOSE
static bool verbose = true;
#else
static bool verbose = false;
#endif

/**********************HttpRequest START******************************/

HttpRequest::HttpRequest(std::string raw_headers) {
  using namespace strutil;
  std::string::size_type n = raw_headers.find("\r\n\r\n");
  std::string header_string(raw_headers, 0, n);
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

/**********************HttpRequest END******************************/


/**********************HttpResponse START******************************/

/**
 * Struct which encapsulates the contents of a HttpResponse
 *
 * This constructor simply sets the default status code to be 200
 * and returns a HttpResponse object. The rest of the HttpResponse
 * components are set through other member functions.
 *
 * @return a HttpResponse object with its status code initialized to 200
 */
HttpResponse::HttpResponse() { _status_code = 200; }

/**
 * Helper function to get the corresponding description of
 * a status code.
 * TODO: There's definitely a better way to handle this as there's
 * a very limited selection of status codes here but this works for now.
 *
 * @param status_code the status code
 * @return the description corresponding to the given status code, or else
 * "OK" by default if the code if not included in the map.
 *
 */
static std::string get_status_msg(const uint8_t &status_code) {
  std::map<uint8_t, std::string> codes = {{200, "OK"},
                                          {301, "Moved Permanently"},
                                          {302, "Found"},
                                          {400, "Bad Request"},
                                          {404, "Not Found"}};
  if (codes.find(status_code) != codes.end()) {
    return codes.at(status_code);
  }
  return "OK";
}

/* The following methods are basically Setters and Getters
 * for HttpResponse
 */

void HttpResponse::set_status_code(const int &status_code) {
  _status_code = status_code;
}

/**
 * Take in a key value pair and set it as a HTTP header
 *
 * @param key the header key
 * @Param value the header value
 */
void HttpResponse::set_header(const std::string &key,
                              const std::string &value) {
  _headers.insert_or_assign(key, value);
}

/**
 * Sets the content type of the response to be
 * "text/plain" and sets the response body.
 *
 * @param msg message to be sent as plain text
 */
void HttpResponse::text(const std::string &msg) {
  this->set_header("Content-Type", "text/plain");
  _body = msg;
}


/**
 * Sets the response body to contain the contents
 * of the file specified at `path`.
 *
 * @param path path of the file to be sent
 */
void HttpResponse::static_file(const std::string &path) {
  _body = strutil::slurp(path);
}

/**
 * Send an image as a response
 * Sets the content-type to image/png by default
 *
 * @param path the path to the image
 */
void HttpResponse::image(const std::string &path) {
  this->set_header("Content-Type", "image/png");
  std::ifstream fin(path, std::ios::in | std::ios::binary);
  std::ostringstream oss;
  oss << fin.rdbuf();
  _body = oss.str();
  fin.close();
}

/**
 * Send an image as a response and set the
 * content type to image/`type`
 *
 * @param path the path to the image
 * @param type the type of image (png/x-icon/jpg etc.)
 */
void HttpResponse::image(const std::string &path, const std::string &type) {
  this->set_header("Content-Type", "image/" + type);
  std::ifstream fin(path, std::ios::in | std::ios::binary);
  std::ostringstream oss;
  oss << fin.rdbuf();
  _body = oss.str();
  fin.close();
}

/**
 * Send a raw html string as the response.
 * Content-Type will be set to "text/html".
 *
 * @param html_string 
 */
void HttpResponse::html_string(const std::string &msg) {
  this->set_header("Content-Type", "text/html");
  _body = msg;
}

void HttpResponse::html(const std::string &path) {
  this->set_header("Content-Type", "text/html");
  this->static_file(path);
}

void HttpResponse::json(const std::string &json_string) {
  this->set_header("Content-Type", "application/json");
  _body = json_string;
}

void HttpResponse::downloadable(const std::string &path,
                                const std::string &content_type) {
  std::string filename = std::filesystem::path(path).filename();
  this->set_header("Content-Type", content_type);
  this->set_header("Content-Disposition",
                   fmt::format(R"(attachment; filename="{}")", filename));
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
        "Cannot define GET routes while in static directory serving mode");
  }
  _routes["GET"].insert_or_assign(route, func);
}

void HttpServer::_get(const std::string &route, routeFunc func) {
  _routes["GET"].insert_or_assign(route, func);
}

void HttpServer::post(const std::string &route, routeFunc func) {
  _routes["POST"].insert_or_assign(route, func);
}

void HttpServer::del(const std::string &route, routeFunc func) {
  _routes["DELETE"].insert_or_assign(route, func);
}

void HttpServer::put(const std::string &route, routeFunc func) {
  _routes["PUT"].insert_or_assign(route, func);
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

static sockaddr_in get_sa(uint16_t port) {
  sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  return sa;
}

void handle_request_body(int connfd, HttpRequest &req) {
  unsigned long size_to_read;
  try {
    size_to_read = std::stoul(req.headers().at("content-length"));
  } catch (std::out_of_range const &) {
    return;
  }
  char *buf = new char[size_to_read + 1];
  memset(buf, 0, size_to_read + 1);
  read(connfd, buf, size_to_read);
  req._body.append(buf);
  delete[] buf;
}

void HttpServer::handle_reply(const HttpRequest &request, int connfd) const {
  if (_routes.find(request.method()) != _routes.end()) {
    auto route = _routes.at(request.method());
    if (route.find(request.route()) != route.end()) {
      if (verbose) {
        std::cout << fmt::format("Route {} matched for method {}!",
                                 request.route(), request.method())
                  << std::endl;
      }
      auto func = route.at(request.route());
      HttpResponse res;
      func(request, res);
      std::string reply = res.get_full_response();
      write(connfd, reply.c_str(), reply.length());
      close(connfd);
      return;
    }
  }
  if (verbose) {
    std::cout << fmt::format("Route {} doesn't match any mappings.",
                             request.route())
              << std::endl;
  }
  std::string reply = _notFoundPage.get_full_response();
  write(connfd, reply.c_str(), reply.length());
  close(connfd);
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
        content_type = "text/plain";
      }
      res.set_header("Content-Type", content_type);
      res.static_file(entry.path());
    });
  }
}

void HttpServer::handle_connections(int connfd) {
  char buf[2] = {0};
  std::string request_string;
  fd_set fds;
  timeval tv;
  tv.tv_sec = 5;
  tv.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(connfd, &fds);

  int ret = select(connfd + 1, &fds, NULL, NULL, &tv);

  if (ret == 0) {
    std::cout << "reading from the client has timed out\n";
    return;
  } else if (ret == -1) {
    _cleanup();
    throw std::runtime_error(
        "an error has occured at select (for reading from the client)");
  } else if (FD_ISSET(connfd, &fds)) {
    while ((read(connfd, buf, 1) > 0)) {
      request_string.append(buf);
      if (strutil::contains(request_string, "\r\n\r\n")) {
        break;
      }
    }
  }
  if (request_string.empty()) {
    std::cout << "Empty Request\n";
    return;
  }
  HttpRequest request(request_string);
  handle_request_body(connfd, request);

  std::cout << fmt::format("Recieved {} request for route: {}",
                           request.method(), request.route())
            << std::endl;
  handle_reply(request, connfd);
}

void HttpServer::_cleanup() {
  close(_listenfd);
  std::cout << "\nall sockets closed. Exiting now..." << std::endl;
}

int HttpServer::accept_connection() {
  // create a sockaddr struct to store client information
  sockaddr_in client_sa = {0}; // zero out the struct
  socklen_t client_sa_len =
      sizeof(sockaddr); // must initialize value to size of struct

  // accept is now non-blocking as _listenfd is set to be non-blocking
  int connfd = accept(_listenfd, reinterpret_cast<sockaddr *>(&client_sa),
                      &client_sa_len);
  if (connfd == -1) {
    _cleanup();
    throw std::runtime_error("error accepting connection");
  }
  if (verbose) {
    char client_address[INET_ADDRSTRLEN] = {0};
    inet_ntop(client_sa.sin_family, &client_sa.sin_addr, client_address,
              INET_ADDRSTRLEN);
    std::cout << fmt::format("Recieved connection from ip address: {}",
                             client_address)
              << std::endl;
  }
  return connfd;
}

void HttpServer::try_bind(const int &port) {
  int bind_retry_count = 0;
  int bind_status;
  sockaddr_in sa = get_sa(port);
  do {
    bind_status = bind(_listenfd, (sockaddr *)&sa, sizeof(sockaddr));
    if (bind_status == -1) {
      std::cout << fmt::format(
          "Binding to port {} failed. Retry count: {}/{}\n", port,
          ++bind_retry_count, RETRY_COUNT);
      std::cout << "Error: " << strerror(errno) << '\n';
      std::cout << fmt::format("Waiting {}s before retrying...\n",
                               bind_retry_count);
      sleep(1 * bind_retry_count);
    }
  } while (bind_status == -1 && bind_retry_count < RETRY_COUNT);

  if (bind_status == -1) {
    std::cout << std::strerror(errno) << '\n';
    throw std::runtime_error(
        fmt::format("Unable to bind after {} tries", RETRY_COUNT));
  }
}

void HttpServer::try_listen(const int &port) {
  if (listen(_listenfd, _numListeners) == -1) {
    throw std::runtime_error("unable to listen");
  }
  std::cout << fmt::format("Now listening at port: {} with {} listeners\n",
                           port, _numListeners);
}

void HttpServer::setup_interrupts() {
  struct sigaction sigAction;
  sigAction.sa_flags = 0;
  sigAction.sa_handler = intHandler;
  sigaction(SIGINT, &sigAction, NULL);
}

int HttpServer::create_socket() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  // Get the socket's current flags
  int flags = fcntl(fd, F_GETFL, 0);
  // Set the socket to be non-blocking
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  // Set the socket to be reusable instantly; Violates TCP/IP protocol?
  int iSetOption = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&iSetOption,
             sizeof(iSetOption));
  if (fd == -1) {
    throw std::runtime_error("Failed to create socket");
  }
  return fd;
}

void HttpServer::run(const std::uint16_t &port) {
  if (!_static_directory_path.empty()) {
    staticSetup();
  }
  setup_interrupts();

  _listenfd = create_socket();
  try_bind(port);
  try_listen(port);

  fd_set current_sockets;
  fd_set ready_sockets;
  FD_ZERO(&current_sockets);
  FD_SET(_listenfd, &current_sockets);

  while (_run) {
    ready_sockets = current_sockets;

    if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) == -1) {
      _cleanup();
      throw std::runtime_error(
          "an error occured at select (for accepting connections)");
    }
    for (int fd = 0; fd < FD_SETSIZE; ++fd) {
      if (FD_ISSET(fd, &ready_sockets)) {
        if (fd == _listenfd) {
          int connfd = accept_connection();
          FD_SET(connfd, &current_sockets);
        } else {
          handle_connections(fd);
          FD_CLR(fd, &current_sockets);
        }
      }
    }
  }
  _cleanup();
}
