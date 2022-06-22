# This is a simple HTTP server written in C++ for learning purposes.

## Features
* Very simple expressjs-like API, albeit with way fewer features
* Support for GET, POST, PUT, DELETE methods (should be trivial to add more if needed)
* Psuedo-asynchronous with the help of `select`, which also helps keep the resources used low
* Easily serve static directories (examples below)
* Easy to include in project

## Usage
First off, a simple "hello world" example.<br>
To use the library, clone the repo into a directory.<br>
In this example we will just use the default, which is CPP_HTTP_Server.
```console
git clone https://github.com/WilsonOh/CPP_HTTP_Server.git
```
Include the library by providing the path to the `HttpServer.hpp` file as seen below.
```cpp
#include "CPP_HTTP_Server/HttpServer.hpp"

int main(void) {
  auto svr = HttpServer();
  svr.get("/", [](const HttpRequest &req, HttpResponse &res) {
    res.text("Hello, World!");
  });
  svr.run();
}
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

### Important methods to note

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
