#include "HttpServer.hpp"
#include <cstring>

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

void HttpResponse::set_header(const std::string &key,
                              const std::string &value) {
  _headers.insert_or_assign(key, value);
}

void HttpResponse::text(const std::string &msg) {
  this->set_header("Content-Type", "text/plain");
  _body = msg;
}

void HttpResponse::static_file(const std::string &path) {
  _body = strutil::slurp(path);
}

void HttpResponse::image(const std::string &path) {
  this->set_header("Content-Type", "image/png");
  std::ifstream fin(path, std::ios::in | std::ios::binary);
  std::ostringstream oss;
  oss << fin.rdbuf();
  _body = oss.str();
  fin.close();
}

void HttpResponse::image(const std::string &path, const std::string &type) {
  this->set_header("Content-Type", "image/" + type);
  std::ifstream fin(path, std::ios::in | std::ios::binary);
  std::ostringstream oss;
  oss << fin.rdbuf();
  _body = oss.str();
  fin.close();
}

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

void HttpResponse::redirect(const std::string &new_location, const int &status_code) {
  this->set_status_code(status_code);
  this->set_header("Location", new_location);
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

/**********************HttpRequest END******************************/

// Have to re-declare static class variables in the source file
volatile sig_atomic_t HttpServer::_run;

void HttpServer::intHandler(int) { _run = 0; }

HttpServer::HttpServer() {
  _run = 1;
  _numListeners = 3;
  HttpResponse not_found_res;
  not_found_res.set_status_code(404);
  not_found_res.text("Wilson's Server: The requested page is not found");
  _notFoundResponse = not_found_res;
}

/**
 * Thing to note about the following route methods:
 *
 * The bracket operator [] for _routes is useful as
 * it creates an entry if the key doesn't exist.
 * This makes it very easy to add another HTTP
 * method.
 *
 * `insert_or_assign` is useful for the same reasons as above.
 */

void HttpServer::get(const std::string &route, routeFunc func) {
  if (!_static_directory_path.empty()) {
    throw std::invalid_argument(
        "Cannot define GET routes while in static directory serving mode");
  }
  _routes["GET"].insert_or_assign(route, func);
}

/**
 * Internal method that doesn't throw if there is already a
 * `_static_directory_path` defined.
 * This is used for setting up GET routes in `staticSetup`
 */
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

/**
 * For the following methods which return `HttpServer`, I
 * originally intended to simply modify the current instance
 * and return a reference to `this`.
 *
 * However, that resulted in some weird behaviour where `std::cout`
 * didn't print anything. That's why I'm creating a temporary `HttpServer`
 * object and returning a copy of it.
 */

HttpServer HttpServer::setNumListeners(int num_listeners) {
  HttpServer tmp = *this;
  tmp._numListeners = num_listeners;
  return tmp;
}

HttpServer HttpServer::set404Page(const std::string &path) {
  HttpServer tmp = *this;
  HttpResponse res;
  res.html(path);
  tmp._notFoundResponse = res;
  tmp._notFoundResponse.set_status_code(404);
  return tmp;
}

HttpServer HttpServer::set404Text(const std::string &message) {
  HttpServer tmp = *this;
  HttpResponse res;
  res.text(message);
  tmp._notFoundResponse = res;
  tmp._notFoundResponse.set_status_code(404);
  return tmp;
}

HttpServer HttpServer::set404Response(HttpResponse res) {
  HttpServer tmp = *this;
  tmp._notFoundResponse = res;
  tmp._notFoundResponse.set_status_code(404);
  return tmp;
}

HttpServer HttpServer::mount_static_directory(const std::string &directory_path,
                                              const std::string &mount_point) {
  HttpServer tmp = *this;
  tmp._static_directory_path = directory_path;
  if (tmp._static_directory_path.back() != '/') {
    tmp._static_directory_path += "/";
  }
  tmp._static_directory_mount_point = mount_point;
  return tmp;
}

/**
 * Returns a copy of a `sockaddr_in` with its
 * fields initialized.
 *
 * s_addr is initialized to `INADDR_ANY` which is basically
 * "0.0.0.0" (I think) and sin_port is initialized to `port`.
 *
 * @param port The port number
 * @return The sockaddr_in struct with its fields initialized
 */
static sockaddr_in get_sa(uint16_t port) {
  sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  return sa;
}

/**
 * The reason I'm using a try-catch instead of simply checking whether
 * "content-length" is a key in req.headers() is because for some reason
 * the latter method didn't work which caused `at` to throw a
 * `std::out_of_range` exception.
 *
 * This function reads the request body into `req` by using the "Content-Length"
 * header of the request.
 *
 * @param connfd The socket to be reading from
 * @param req The HttpRequest object
 */
void handle_request_body(int connfd, HttpRequest &req) {
  unsigned long size_to_read;
  try {
    size_to_read = std::stoul(req.headers().at("content-length"));
  } catch (std::out_of_range const &) {
    return;
  }
  // various methods of reading the request body into req._body
  
  // unique pointer method (buf gets deallocated for automatically
  // and doesn't read directly into the string's `data` so it's
  // safer)
  /* auto buf = std::make_unique<char []>(size_to_read + 1);
  memset(&buf[0], 0, size_to_read + 1);
  read(connfd, &buf[0], size_to_read);
  req._body.append(buf.get()); */

  // resize method (don't need to use a temp variable in between)
  // I'll settle on this one for now.
  // note: don't have to take the null-terminating character into
  // account when resizing the string
  //
  // could be dangerous to read directly into `req._body.data()`,
  // but since this is the only place we modify it, it should be ok
  req._body.resize(size_to_read);
  read(connfd, req._body.data(), size_to_read);

  // Temp string method
  /* std::string buf(size_to_read + 1, 0);
  read(connfd, buf.data(), size_to_read);
  req._body = buf; */
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
      // return after a succesful response
      return;
    }
  }
  // Only enter this block if none of the routes match
  if (verbose) {
    std::cout << fmt::format("Route {} doesn't match any mappings.",
                             request.route())
              << std::endl;
  }
  std::string reply = _notFoundResponse.get_full_response();
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
  // Set the entry point "index.html" to the route specified at
  // `_static_directory_mount_point`
  this->_get(_static_directory_mount_point,
             [=](const HttpRequest &req, HttpResponse &res) {
               res.html(root / "index.html");
             });
  // Recursively list all the files in the static directory
  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(_static_directory_path)) {
    std::string extension(entry.path().extension());
    // This skips all the directory entries
    if (!is_regular_file(entry.path())) {
      continue;
    }
    // Get the file path relatice to the root i.e. /static instead of
    // /build/static
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

  // Use select here so the server will not wait indefintely for a read
  // as it will return from this function after the time out
  int ret = select(connfd + 1, &fds, NULL, NULL, &tv);

  if (ret == 0) {
    std::cout << "reading from the client has timed out\n";
    return;
  } else if (ret == -1) {
    _cleanup();
    throw std::runtime_error(
        "an error has occured at select (for reading from the client)");
  } else if (FD_ISSET(connfd, &fds)) {
    // Read from the client up to the end of the headers
    while ((read(connfd, buf, 1) > 0)) {
      request_string.append(buf);
      if (strutil::contains(request_string, "\r\n\r\n")) {
        break;
      }
    }
  }
  // sanity check just in case, but shouldn't happen
  if (request_string.empty()) {
    std::cout << "Empty Request\n";
    return;
  }
  // parse and store the HTTP request headers and body in `request`
  HttpRequest request(request_string);
  handle_request_body(connfd, request);

  std::cout << fmt::format("Recieved {} request for route: {}",
                           request.method(), request.route())
            << std::endl;
  // handle the reply to the client based on the request recieved
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
    std::cerr << std::strerror(errno) << std::endl;
    throw std::runtime_error("error accepting connection");
  }
  if (verbose) {
    // use `INET_ADDRSTRLEN` as the max size of an internet address
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
    bind_status =
        bind(_listenfd, reinterpret_cast<sockaddr *>(&sa), sizeof(sockaddr));
    if (bind_status == -1) {
      std::cout << fmt::format(
          "Binding to port {} failed. Retry count: {}/{}\n", port,
          ++bind_retry_count, BIND_RETRY_COUNT);
      std::cerr << std::strerror(errno) << std::endl;
      std::cout << fmt::format("Waiting {}s before retrying...\n",
                               bind_retry_count);
      sleep(1 * bind_retry_count);
    }
  } while (bind_status == -1 && bind_retry_count < BIND_RETRY_COUNT);

  if (bind_status == -1) {
    std::cerr << std::strerror(errno) << std::endl;
    throw std::runtime_error(
        fmt::format("Unable to bind after {} tries", BIND_RETRY_COUNT));
  }
}

void HttpServer::try_listen(const int &port) {
  if (listen(_listenfd, _numListeners) == -1) {
    std::cerr << strerror(errno) << std::endl;
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
  // setup static directory first so that any errors can be caught early
  if (!_static_directory_path.empty()) {
    staticSetup();
  }
  setup_interrupts();

  _listenfd = create_socket();
  try_bind(port);
  try_listen(port);

  // credits to Jacob Sorber for the `select` method
  fd_set current_sockets;
  fd_set ready_sockets;
  FD_ZERO(&current_sockets);
  FD_SET(_listenfd, &current_sockets);

  while (_run) {
    // reinitialize `ready_sockets` every loop as `select` is destructive
    ready_sockets = current_sockets;

    if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) == -1) {
      _cleanup();
      throw std::runtime_error(
          "an error occured at select (for accepting connections)");
    }
    for (int fd = 0; fd < FD_SETSIZE; ++fd) {
      if (FD_ISSET(fd, &ready_sockets)) {
        if (fd == _listenfd) {
          // if _listenfd is ready, that means there is a 
          // incoming connection, so we add it to the fd set
          int connfd = accept_connection();
          FD_SET(connfd, &current_sockets);
        } else {
          // if it's not _listenfd, then it must be a client
          // socket which is ready. This means we should read
          // from the client socket, so we call `handle_connections`
          // with the fd
          handle_connections(fd);
          FD_CLR(fd, &current_sockets);
        }
      }
    }
  }
  // clean up when SIGINT is called and _run becomes 0,
  // breaking the while loop
  _cleanup();
}
