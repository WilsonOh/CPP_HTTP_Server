#include <arpa/inet.h>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "get_ip.hpp"
#include "strutil.hpp"
#include <string>

#define PORT 3000

static int listenfd;
static int connfd;

void intHandler(int) {
  std::cout << "\nInterrupted...Closing sockets\n";
  close(listenfd);
  close(connfd);
  exit(EXIT_SUCCESS);
}

std::unique_ptr<sockaddr_in> get_sa(uint16_t port) {
  auto sa = std::make_unique<struct sockaddr_in>();
  sa->sin_family = AF_INET;
  sa->sin_port = htons(port);
  sa->sin_addr.s_addr = htonl(INADDR_ANY);
  return sa;
}

int main(void) {
  signal(SIGINT, intHandler);
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    throw std::runtime_error("Failed to create socket");
  }
  auto sa = get_sa(PORT);
  if (bind(listenfd, reinterpret_cast<sockaddr *>(sa.get()),
           sizeof(sockaddr)) == -1) {
    std::cout << errno << '\n';
    throw std::runtime_error("Unable to bind");
  }
  if (listen(listenfd, 10) == -1) {
    throw std::runtime_error("unable to listen");
  }

  while (true) {
    connfd = accept(listenfd, nullptr, nullptr);
    char buf[2] = {0};
    std::string msg;
    while ((read(connfd, buf, 1) > 0)) {
      msg.append(buf);
      if (strutil::contains(msg, "\r\n\r\n")) {
        break;
      }
    }
    std::cout << msg;
    std::string reply("HTTP/1.1 200 OK\nContent-Type: application/json\r\n\r\n");
    reply.append(R"({ "name": "wilson" })");
    write(connfd, reply.c_str(), reply.length());
    close(connfd);
  }
}
