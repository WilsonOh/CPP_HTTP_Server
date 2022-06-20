#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP

#include <atomic>
#include <csignal>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <string>

struct HttpResponse {
private:
  std::map<std::string, std::string> _headers;
  std::string _body;
  uint16_t _status_code;

public:
  HttpResponse();
  void text(const std::string &msg);
  std::string get_full_response();
  void set_header(const std::string &key, const std::string &value);
  void set_status_code(const int &status_code);
  void static_file(const std::string &path);
  void image(const std::string &path);
};

struct HttpRequest {
// private:
  std::map<std::string, std::string> _headers;
  std::string _body;
  std::string _method;
  std::string _route;

public:
  HttpRequest(std::string text);
};

class HttpServer {

  using routeFunc = std::function<void(const HttpRequest &, HttpResponse &)>;

  int _listenfd;
  int _connfd;
  static volatile sig_atomic_t _run;
  std::map<std::string, routeFunc> _routes;

  static void intHandler(int);

public:
  HttpServer();

  ~HttpServer();

  void listenAndRun(const std::uint16_t &port);

  void get(const std::string &route, routeFunc);
};

#endif // !HTTPSERVER_HPP
