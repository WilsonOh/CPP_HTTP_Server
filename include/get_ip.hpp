#ifndef GET_IP_HPP
#define GET_IP_HPP

#include <netdb.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <string>
#include <iostream>

/**
 * Return the ip address in dots and numbers notation of a host given its
 * hostname. The scheme (HTTP/HTTPS) must be left out.
 *
 * @param[in] hostname The hostname of the host server
 * @return An optional string containing the ip address of the host server,
 * or a nullopt if the hostname is invalid
 */
inline std::string get_ipaddr(const std::string &hostname) {
  struct addrinfo hints = {0};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  struct addrinfo *res;
  int err_code;
  if ((err_code = getaddrinfo(hostname.c_str(), NULL, &hints, &res)) != 0) {
    std::cerr << gai_strerror(err_code) << std::endl;
    throw std::invalid_argument("get_ipaddr: invalid hostname");
  }
  char buf[100] = {0};
  void *ptr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
  inet_ntop(AF_INET, ptr, buf, 100);
  freeaddrinfo(res);
  return buf;
}

#endif // !GET_IP_HPP
