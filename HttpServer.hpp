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
  std::string get_full_response() const;
  std::string get_headers() const;
  void set_header(const std::string &key, const std::string &value);
  void set_status_code(const int &status_code);
  void text(const std::string &msg);
  void static_file(const std::string &path);
  void image(const std::string &path);
  void image(const std::string &path, const std::string &type);
  void html(const std::string &html_string);
};

struct HttpRequest {
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
  int _connfd;
  int _numListeners;
  std::string _static_directory_path;
  std::string _static_root;
  HttpResponse _notFoundPage;
  static volatile sig_atomic_t _run;
  std::map<std::string, routeFunc> _routes;

public:
  HttpServer();

  void listenAndRun(const std::uint16_t &port);

  void get(const std::string &route, routeFunc);

  HttpServer setNumListeners(int num_listeners);

  HttpServer set404Page(const HttpResponse res);

  HttpServer static_directory_path(const std::string &path, const std::string &static_root);

private:
  static void intHandler(int);

  void handleNonStatic(const HttpRequest &request) const;

  void handleStatic(const HttpRequest &request) const;

  void staticSetup();
};

#endif // !HTTPSERVER_HPP
