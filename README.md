# This is a simple HTTP server written in C++ for learning purposes.

## Features
* Very simple expressjs-like API, albeit with way fewer features
* Support for GET, POST, PUT, DELETE methods (should be trivial to add more if needed)
* Psuedo-asynchronous with the help of `select`, which also helps keep the resources used low
* Easily serve static directories (examples below)
* Easy to include in project

## This library will not work on windows!

## Dependencies
* [fmt library](https://fmt.dev/latest/index.html)
* C++ 17
* Unix-based OS (only tested on macOS and Linux)

## Documentation
The documentation for the API is in the _fully commented_ `HttpServer.hpp` header file.<br>
In the `Usage` section below I will go through some example usages.

## Usage
First off, a simple "hello world" example.<br>
To use the library, clone the repo into a directory.<br>
In this example we will just use the default, which is CPP_HTTP_Server.
```console
git clone https://github.com/WilsonOh/CPP_HTTP_Server.git
```
Include the library by providing the path to the `HttpServer.hpp` file as seen below.
```cpp
// main.cpp
#include "CPP_HTTP_Server/HttpServer.hpp"

int main(void) {
  auto svr = HttpServer();
  svr.get("/", [](const HttpRequest &req, HttpResponse &res) {
    res.text("Hello, World!");
  });
  svr.run();
}
```
To compile:
```console
g++ main.cpp CPP_HTTP_Server/HttpServer.cpp -lfmt -o main
```
### Explanation:
We first create an instance of `HttpServer` which initializes its fields to the default values.
#### HttpServer Default Values
* Number of listeners: 3
* Text displayed when the requested page is not found: "Wilson's Server: page request is not found"
###
After that, we define a **GET** route at the root path `/` by passing in "/" as the first argument.<br><br>
We then pass in a lambda which takes in a `const HttpRequest &` and `HttpResponse &` and has no return value.<br>
_**NOTE**:_ It is important to use the correct const qualifier and specify the parameters as references.<br><br>
Within the lambda, we have access the the `HttpRequest` and `HttpResponse` structs and all their methods, but for
this example we simply call `res.text()` with the message we want to send. <br>
This sets the `Content-Type` of the response header to `text/plain` and sends the message as the reponse body.

### Static Directory Hosting
Suppose we have a static directory with the following structure
```console
static
├── 404.html
├── favicon.ico
├── index.html
├── script.js
└── styles.css
```
We can serve the `static` directory and set the 404 page to `static/404.html` with 20 listeners with the following snippet:
```cpp
#include "CPP_HTTP_Server/HttpServer.hpp"

int main(void) {
  auto svr = HttpServer()
                 .setNumListeners(20)
                 .set404Page("./static/404.html")
                 .mount_static_directory("static");
  svr.run();
}
```
#### Explanation
`setNumListeners` and `set404Page` are self explanatory.<br>
`mount_static_directory` mounts the static directory on `/` by default if there is only one argument, but a second argument for the mount point can be passed in.<br>
`run` sets the socket to listen at port 3000 by default with no arguments, but a port number can be passed in if needed.
### *NOTE*: static directory hosting works with nested directories too!

### Another Example
```cpp
#include "CPP_HTTP_Server/HttpServer.hpp"

int main(void) {

  HttpResponse res;
  res.set_header("Foo", "Bar");
  res.html_string("<h1>PAGE NOT FOUND</h1>");

  auto svr = HttpServer()
                 .setNumListeners(20)
                 .set404Response(res)
                 .mount_static_directory("build", "/www");

  svr.post("/users", [](const HttpRequest &req, HttpResponse &res) {
    res.json(R"({ "foo": "bar" })");
  });

  svr.run(8080);
}
```
#### Explanation
Here, instead of setting the path to the 404 page, we can make our own `HttpResponse` object and fully define its behaviour and set it as the response for 404 requests.<br>
The static directory is mounted at `/www` instead of `/` here, and a POST route is defined for the `/users` route which returns a json in the response body.<br>
Lastly, the server is set to run at port 8000.

### Other methods
There are a lot of "convenience methods" for the `HttpResponse` object, such as `res.image` to send an image from the host directory or `res.downloadable` which sends a file as an attachment to the client.<br><br>
`res.redirect` can redirect the browser to a relative route or another URL, and
`res.set_header` can be used to add to the HTTP response headers or override any existing ones.<br><br>
Feel free to look through the header file for the full list of methods available!

## TODO
- [x] Get a basic server to start up
- [x] Create a class to enclose the server creation
- [x] Make setting headers easy
- [x] Create HTTP method functions, which takes in a route and lambda
- [x] Support (single) static file hosting
- [x] Support static directory hosting (for js and css files)
- [x] Support http methods other than go (using separate route maps for each method)
- [x] Support file hosting (for downloading)
- [x] Provide verbose debugging information
- [ ] Multithreading (maybe maybe not???)
- [ ] Support paramerized URLs

---
### *No support for encryption planned as of now*
