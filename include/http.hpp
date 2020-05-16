#pragma once

#include <regex>
#include <stdexcept>
#include <algorithm>

#include <picohttpparser.h>

#include <net.hpp>

namespace http {

enum class Method { GET, POST }; // TODO: Fill

std::string_view to_string(Method method) {
  switch (method) {
    case Method::GET: return "GET";
    case Method::POST: return "POST";
  }
}

Method get_method(const std::string_view& method) {
  if (method == "GET") return Method::GET;
  if (method == "POST") return Method::POST;
  throw std::invalid_argument{"Unknown method"};
}

// Rework as immutable class with factories
class Request {
  const std::string _data;
  Method _method;
  std::string_view _path;
  // TODO: headers as unordered_map

  public:

  template<typename T>
  Request(T&& data) : _data(std::forward<T>(data)) {
    parse();
  }

  void parse() {
    const char* method;
    std::size_t method_len;
    const char* path;
    std::size_t path_len;
    int minor_version;
    phr_header headers[100];
    std::size_t headers_len;

    phr_parse_request(_data.c_str(), _data.size(),
        &method, &method_len,
        &path, &path_len,
        &minor_version,
        headers, &headers_len,
        0);

    _method = get_method({method, method_len});
    _path = {path, path_len};
  }

  auto path() const {
    return _path;
  }

  auto method() const {
    return _method;
  }
};

class Response {};

template<typename T>
using Matcher = std::tuple<Method, std::regex, T>;

template<typename T>
class Server {
  using AddressType = net::SocketAddressIPv4;
  using MatchersType = T;

  net::Server<AddressType> _server;
  MatchersType _matchers;

  template<typename M>
  bool match(const M& matcher, const Request& req, net::Connection& conn) {
    const auto& method = std::get<0>(matcher);

    if (req.method() != method) return false;

    const auto& regex = std::get<1>(matcher);
    auto m = std::cmatch{};
    const auto& path = req.path();
    auto matched = std::regex_match(std::begin(path), std::end(path), m, regex);

    if (!matched)
      return false;

    std::get<2>(matcher)(m, req, conn);
    return true;
  }

  void handle(net::Connection&& conn) {
    auto data = std::string{};
    while (net::getline(conn, data))
      data.push_back('\n');
    auto req = Request(std::move(data));

    auto matched = std::apply([&](const auto&... matchers){
        return (match(matchers, req, conn) || ...);
        }, _matchers);

    if (matched) return;

    // TODO: Refactor
    conn << "HTTP/1.1 404 Not Found\n\n";
  }

  Server(short port, MatchersType matchers):
    _server(net::make_tcp_server(port)),
    _matchers(matchers) {}

  public:

  template<typename... Args>
  Server(short port, Args... args):
    Server(port, std::tuple<Args...>(args...)) {}


  void listen() {
    _server.listen(&Server::handle, this);
  }
};

template<typename... Args>
auto make_server(short port, Args... args) {
  using MatchersType = std::tuple<Args...>;
  return Server<MatchersType>{port, std::forward<Args>(args)...};
}

template<typename Fun>
auto make_matcher(Method method, const std::string& path, Fun&& fun) {
  return Matcher<Fun>{method, path, std::forward<Fun>(fun)};
}

template<typename Fun>
auto get(const std::string& path, Fun&& fun) {
  return make_matcher(Method::GET, path, std::forward<Fun>(fun));
}

} // namespace http
