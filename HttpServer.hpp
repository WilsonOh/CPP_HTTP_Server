#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP

#include <atomic>
#include <csignal>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <string>

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

struct HttpRequest {
  friend void handle_request_body(int connfd, HttpRequest &req);

private:
  std::map<std::string, std::string> _headers;
  std::string _body;
  std::string _method;
  std::string _route;

public:
  HttpRequest(std::string text);
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
