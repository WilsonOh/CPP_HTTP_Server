#ifndef STRUTIL_HPP
#define STRUTIL_HPP

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace strutil {
inline std::vector<std::string> split(const std::string &s,
                                      const std::string &delimiter) {
  size_t pos_start = 0;
  size_t pos_end;
  size_t delim_len = delimiter.length();
  std::string token;
  std::vector<std::string> res;

  while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
    token = s.substr(pos_start, pos_end - pos_start);
    pos_start = pos_end + delim_len;
    res.emplace_back(token);
  }

  std::string last = s.substr(pos_start);
  if (!last.empty())
    res.emplace_back(last);
  return res;
}

inline std::string lowers(const std::string &s) {
  std::string tmp = s;
  std::transform(tmp.cbegin(), tmp.cend(), tmp.begin(),
                 [](const char &c) { return tolower(c); });
  return tmp;
}

inline std::string uppers(const std::string &s) {
  std::string tmp = s;
  std::transform(tmp.cbegin(), tmp.cend(), tmp.begin(),
                 [](const char &c) { return toupper(c); });
  return tmp;
}

inline std::string ltrim(const std::string &s) {
  std::string copy = s;
  copy.erase(copy.cbegin(),
             std::find_if(copy.cbegin(), copy.cend(),
                          [](const char &c) { return !std::isspace(c); }));
  return copy;
}

inline std::string rtrim(const std::string &s) {
  std::string copy = s;
  copy.erase(std::find_if(copy.crbegin(), copy.crend(),
                          [](const char &c) { return !std::isspace(c); })
                 .base(),
             copy.end());
  return copy;
}

inline std::string trim(const std::string &s) {
  std::string copy = s;
  copy = ltrim(copy);
  copy = rtrim(copy);
  return copy;
}

inline bool contains(const std::string haystack, const std::string needle) {
  return haystack.find(needle) != std::string::npos;
}

inline std::string slurp(const std::string &path) {
  std::ostringstream buf;
  std::ifstream input(path.c_str());
  if (!input) {
    throw std::runtime_error("file not found: " + path);
  }
  buf << input.rdbuf();
  return buf.str();
}

} // namespace strutil

#endif // STRUTIL_HPP
