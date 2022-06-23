#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP

/******************NETWORKING INCLUDES**************************/

/* inet_pton/inet_ntop for converting between presentation format
   of IP addresses */
#include <arpa/inet.h>

/* fcntl for setting socket options, such as setting a socket to be
   non-blocking */
#include <fcntl.h>

/* getaddrinfo for getting a server's IP address from its hostname,
   along with other useful information about the server */
#include <netdb.h>

/* sockaddr_in struct for defining a internet address */
#include <netinet/in.h>

/* provides type definitions for a lot of interfaces */
#include <sys/types.h>

/* socket function for creating a socket */
#include <sys/socket.h>

/* select function for pseudo async behaviour */
#include <sys/select.h>

/* read and write functions for communication between sockets */
#include <unistd.h>

/*************************OTHER INCLUDES**************************/

/* format function for easy string interpolation */
#include <fmt/format.h>

/* path manipulaion functions and classes */
#include <filesystem>

/* obviously strings are very important for this */
#include <string>

/* errno global variable for debugging */
#include <errno.h>

/* std::runtime_error and std::invalid_argument exceptions
   to throw when something goes wrong */
#include <stdexcept>

/* catch SIGINT and other signals and clean up resources
   nicely. NOTE: use sigaction over signal whenever possible */
#include <signal.h>

/* handy little string manipulation library */
#include "strutil.hpp"

/* getipaddr function for easy hostname to IP address conversion */
#include "get_ip.hpp"

/* provides the sig_atomic_t type so that the program can exit gracefully when
 * interrupted */
#include <atomic>

/* for specific int types such as uint16_t */
#include <cstdint>

/* nice way of defining lambdas as a type to be used in the routes map */
#include <functional>

/* cout and ifstream for obvious reasons */
#include <iostream>

/* map to store routing information in HttpServer */
#include <map>
/*************************INCLUDES END**************************/

#define RETRY_COUNT 5
#define DEFAULT_PORT 3000

struct HttpResponse {
private:
  std::map<std::string, std::string> _headers;
  std::string _body;
  uint16_t _status_code;

public:
  HttpResponse();
  std::string get_full_response() const;
  std::string get_headers() const;
  void set_header(const std::string &key, const std::string &value);
  void set_status_code(const int &status_code);
  void text(const std::string &msg);
  void static_file(const std::string &path);
  void image(const std::string &path);
  void image(const std::string &path, const std::string &type);
  void html(const std::string &path);
  void html_string(const std::string &html_string);
  void json(const std::string &json_string);
  void downloadable(const std::string &path,
                    const std::string &content_type = "text/html");
};

/**
 * A struct to encapsulate the contents of a HttpRequest
 */
struct HttpRequest {
  /**
   * Friend function which reads the body of a HTTP request into
   * the struct, if there's any.
   */
  friend void handle_request_body(int connfd, HttpRequest &req);

private:
  std::map<std::string, std::string> _headers;
  std::string _body;
  std::string _method;
  std::string _route;

public:
  /**
   * This constructor takes in raw HTTP request headers and parses it
   * into components comprising of:
   * 1. The HTTP method
   * 2. The URI requested
   * 3. The HTTP headers
   *
   * @param raw_headers the raw HTTP request headers
   * @return A HttpRequest object containing the parsed information
   */
  HttpRequest(std::string text);

  /* Getters for each component of the HttpRequest */
  std::map<std::string, std::string> headers() const;
  std::string body() const;
  std::string method() const;
  std::string route() const;
};

class HttpServer {

  using routeFunc = std::function<void(const HttpRequest &, HttpResponse &)>;

  int _listenfd;
  int _numListeners;
  std::string _static_directory_path;
  HttpResponse _notFoundPage;
  static volatile sig_atomic_t _run;
  std::map<std::string, std::map<std::string, routeFunc>> _routes;

public:
  HttpServer();

  void run(const std::uint16_t &port = DEFAULT_PORT);

  void get(const std::string &route, routeFunc);
  void post(const std::string &route, routeFunc);
  void del(const std::string &route, routeFunc);
  void put(const std::string &route, routeFunc);

  HttpServer setNumListeners(int num_listeners);

  HttpServer set404Page(const std::string &path);

  HttpServer set404Text(const std::string &message);

  HttpServer set404Response(HttpResponse res);

  HttpServer static_directory_path(const std::string &path);

private:
  static void intHandler(int);

  int accept_connection();

  void setup_interrupts();

  int create_socket();

  void try_listen(const int &port);

  void try_bind(const int &port);

  void handle_reply(const HttpRequest &request, int connfd) const;

  void handle_connections(int connfd);

  void staticSetup();

  void _get(const std::string &route, routeFunc);

  void _cleanup();
};

#endif // !HTTPSERVER_HPP
