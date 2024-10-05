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

#define BIND_RETRY_COUNT 5
#define DEFAULT_PORT 3000

/**
 * struct which encapulates the contents of a HttpResponse
 *
 * The struct comprises of 3 components:
 * 1. headers, which is a map of HTTP response headers in a string key-value
 * pair
 * 2. body, which contains the contents of the HTTP response
 * 3. status code, which is the HTTP status code to send to the client in the
 * response
 */
struct HttpResponse {
private:
  std::map<std::string, std::string> _headers;
  std::string _body;
  uint16_t _status_code;

public:
  /**
   * This constructor simply sets the default status code to be 200
   * and returns a HttpResponse object. The rest of the HttpResponse
   * components are set through other member functions.
   *
   * @return a HttpResponse object with its status code initialized to 200
   */
  HttpResponse();

  /**
   * Return the full formatted HTTP response, including all the headers and
   * body.
   * This method is to be called after setting all headers and status code and
   * body as it simply combines all those components together in a single
   * formatted string.
   *
   * @return The fully formatted HTTP response as a string
   */
  std::string get_full_response() const;

  /**
   * Return the headers of the HTTP response in a formatted string.
   * This method is basically identical to `get_full_response()`, just
   * without the body.
   *
   * @return The formatted HTTP response headers as a string
   */
  std::string get_headers() const;

  /**
   * Add a HTTP response header in the form of a key-value pair.
   * This method overrides previous entries if identical keys are
   * inserted.
   *
   * @param key The key of the HTTP header
   * @param value The corresponding value of the HTTP header
   */
  void set_header(const std::string &key, const std::string &value);

  /**
   * Set the status code of the HTTP response.
   * If not set, the status code defaults to 200.
   *
   * @param status_code The HTTP response status code
   */
  void set_status_code(const int &status_code);

  /**
   * Set the Content-Type of the HTTP response to "text/plain",
   * and the response body to `msg`.
   *
   * @param msg The msg to be set as the HTTP response body
   */
  void text(const std::string &msg);

  /**
   * Set the body of the HTTP response to the contents of the
   * file at `path`.
   * This method does not set the Content-Type of the HTTP response,
   * so the user has to specify the Content-Type using `set_header`.
   *
   * @param path The path to the file
   */
  void static_file(const std::string &path);

  /**
   * Send an image as a response
   * Sets the content-type to image/png by default
   *
   * @param path the path to the image
   */
  void image(const std::string &path);

  /**
   * Send an image as a response and set the
   * content type to image/`type`
   *
   * @param path the path to the image
   * @param type the type of image (png/x-icon/jpg etc.)
   */
  void image(const std::string &path, const std::string &type);

  /**
   * Set the Content-Type of the HTTP response to
   * "text/html" and set the body of the response
   * to the contents of the HTML file at `path`.
   *
   * @param path The path to the HTML file
   */
  void html(const std::string &path);

  /**
   * Set the Content-Type of the HTTP response to
   * "text/html" and set the body of the response
   * to `html_string`.
   *
   * @param html_string The raw HTML string
   */
  void html_string(const std::string &html_string);

  /**
   * Set the Content-Type of the HTTP response to
   * "application/json" and set the body of the response
   * to `json_string`.
   *
   * @param json_string The raw JSON string
   */
  void json(const std::string &json_string);

  /**
   * Set the Content-Type of the HTTP response to `content_type` and
   * the Content-Dispostion to "attachment".
   * This allows the file at `path` to be marked as an attachment and
   * will be automatically downloaded to the client's machine.
   *
   * @param path The path to the file
   * @param content_type the content type of the file
   */
  void downloadable(const std::string &path,
                    const std::string &content_type = "text/html");

  void redirect(const std::string &new_location, const int &status_code = 301);
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

  /**
   * Nice little typedef to represent the lambda used when defining
   * a route.
   */
  using routeFunc = std::function<void(const HttpRequest &, HttpResponse &)>;

  /**
   * The server file descriptor made into an instance variable so
   * that it can be closed whenever needed.
   */
  int _listenfd;

  /**
   * The number of backlog listeners allowed in the server before the
   * connection gets dropped.
   */
  int _numListeners;

  /**
   * The path to a static directory for hosting.
   * If this string is not empty, the server will host the
   * files in this directory.
   */
  std::string _static_directory_path;

  /**
   * The URI for the hosted static directory
   */
  std::string _static_directory_mount_point;

  /**
   * The HTTP response to be sent to the client when a requested
   * page is not defined in the server.
   */
  HttpResponse _notFoundResponse;

  /**
   * An atomic int to determine whether the server should continue to
   * run. This variable is set to 1 during construction, and set to 0
   * when a SIGINT is catched or when an exception is thrown.
   */
  static volatile sig_atomic_t _run;

  /**
   * Map of HTTP methods to their respective route maps.
   * Each route map consists of a route and their corresponding
   * lambda.
   */
  std::unordered_map<std::string, std::map<std::string, routeFunc>> _routes;

public:
  /**
   * Constructor which initializes some fields of the
   * server to its defaults.
   *
   * Default Values:
   * _num_listeners = 3
   * _notFoundResponse = HttpResponse().status_code(404).text("Wilson's Server:
   * The requested page is not found")
   */
  HttpServer();

  /**
   * Start up the server and run
   *
   * This method is to be called at the very end after all the settings and
   * routes are defined. If `port` is not specified, the server will run on the
   * default port of 3000
   *
   * @param port The port to run the server on
   */
  void run(const std::uint16_t &port = DEFAULT_PORT);

  /**
   * Define a route for GET requests
   *
   * The method signature of the lambda to be passed in is
   * void f(const HttpRequest &, HttpResponse &res)
   *
   * @param route The URI route
   * @param f The lambda which defines what the route does.
   *
   */
  void get(const std::string &route, routeFunc f);

  /**
   * Define a route for POST requests
   *
   * The method signature of the lambda to be passed in is
   * void f(const HttpRequest &, HttpResponse &res)
   *
   * @param route The URI route
   * @param f The lambda which defines what the route does.
   *
   */
  void post(const std::string &route, routeFunc f);

  /**
   * Define a route for DELETE requests
   *
   * The method signature of the lambda to be passed in is
   * void f(const HttpRequest &, HttpResponse &res)
   *
   * @param route The URI route
   * @param f The lambda which defines what the route does.
   *
   */
  void del(const std::string &route, routeFunc f);

  /**
   * Define a route for PUT requests
   *
   * The method signature of the lambda to be passed in is
   * void f(const HttpRequest &, HttpResponse &res)
   *
   * @param route The URI route
   * @param f The lambda which defines what the route does.
   *
   */
  void put(const std::string &route, routeFunc f);

  /**
   * Sets the number of listeners allowed in the server
   *
   * @param num_listeners The number of listeners
   */
  HttpServer setNumListeners(int num_listeners);

  /**
   * Set the body of `_notFoundResponse` to the
   * contents of the file at `path`.
   *
   * @param path The path to the file
   */
  HttpServer set404Page(const std::string &path);

  /**
   * Set the body of `_notFoundResponse` to `message`.
   *
   * @param message The message to be displayed as plain text
   */
  HttpServer set404Text(const std::string &message);

  /**
   * Set `_notFoundResponse` to `res`.
   * This method is useful if you want to define custom
   * behaviour for the 404 response.
   *
   * @param res The HttpResponse to be sent
   */
  HttpServer set404Response(HttpResponse res);

  /**
   * Mounts a static directory at `directory_path` to `mount_point`.
   * If `mount_point` is not specified, it will be defaulted to "/".
   *
   * @param directory_path The path to the static directory
   * @param mount_point The route the static directory will be mounted on
   */
  HttpServer mount_static_directory(const std::string &directory_path,
                                    const std::string &mount_point = "/");

private:
  /**
   * Handler function for Interrupts
   * Basically sets _run to 0
   */
  static void intHandler(int);

  /**
   * Light wrapper around the `accept` function which reads the
   * client information into a `sockaddr` and prints out its
   * IP address.
   * Additionally does error checking which prints the error message
   *
   * @throw std::runtime_exception if `accept` returns -1
   */
  int accept_connection();

  /**
   * Setup SIGINT handler using `sigaction`
   */
  void setup_interrupts();

  /**
   * Create a listener socket and set it to be non-blocking
   * and resuable using `setsockopt` and `fcntl`.
   *
   * @throw std::runtime_exception if `socket` returns -1
   */
  int create_socket();

  /**
   * Light wrapper around the `listen` function which sets _listenfd to
   * listen at `port`.
   * Additionally does error checking which prints the error message
   *
   * @throw std::runtime_exception if `listen` returns -1
   */
  void try_listen(const int &port);

  /**
   * Light wrapper around the `bind` function which binds _listenfd to
   * `port` and prints out debugging information
   * This method retries `BIND_RETRY_COUNT` number of times before
   * throwing a runtime exception
   *
   * Additionally does error checking which prints the error message
   *
   * @throw std::runtime_exception if `bind` returns -1
   */
  void try_bind(const int &port);

  /**
   * This methods takes in a HttpRequest and checks whether
   * the URI the requested is defined in the server.
   * If the URI requested is not defined, it will respond with
   * _notFoundResponse.
   *
   * @param request The incoming HTTP request from the client
   * @param connfd The file descriptor of the client
   *
   */
  void handle_reply(const HttpRequest &request, int connfd);

  /**
   * Reads from `connfd` using `select`.
   * The timeout specified for `select` is 5 seconds. If `select`
   * timeouts, the function will simply return and the next coonnection
   * will be handled.
   *
   * If the connection is valid, the function will read the the entire
   * HTTP request and pass the parsed request into `handle_reply`.
   *
   * @param connfd The file descriptor to be read from
   */
  void handle_connections(int connfd);

  /**
   * Set up the GET routes for files in `_static_directory_path` if specified.
   */
  void staticSetup();

  /**
   * Identical to `get`, but used internally in `staticSetup` as defining
   * additional GET routes is not allowed when `_static_directory_path` is
   * specified.
   */
  void _get(const std::string &route, routeFunc);

  /**
   * Simply `close`s the `_listenfd` socket
   */
  void _cleanup();
};

#endif // !HTTPSERVER_HPP
