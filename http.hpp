#pragma once

#include <sstream>
#include <string_view>
#include <regex>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <vector>

#include "net.hpp"

namespace http {

enum class Version { V11 }; // TODO: Fill
enum class Method { GET, POST }; // TODO: Fill

struct Request {
  const std::string data;

  Method _method;
  std::string_view _path;
  Version _version;
  std::unordered_map<std::string_view, std::string_view> _headers;

  Method parse_method(const std::string_view& method_view) {
    if (method_view == "GET") return _method = Method::GET;

    throw std::invalid_argument{std::string{"Unknown method: "}
      .append(method_view.begin(), method_view.end())};
  }

  Version parse_version(const std::string_view& version_view) {
    if (version_view == "HTTP/1.1") return _version = Version::V11;

    throw std::invalid_argument{std::string{"Unknown version: "}
      .append(version_view.begin(), version_view.end())};
  }

  void parse() {
    auto pos = data.find_first_of(' ');
    parse_method({data.c_str(), pos});

    auto spos = data.find_first_of(' ', pos + 1);
    _path = {&data[pos + 1], spos - pos - 1};

    pos = spos;
    spos = data.find_first_of('\n', pos);
    parse_version({&data[pos + 1], spos - pos - 2});

    //TODO: Parse headers
  }

  public:

  Request(std::string&& data): data(data) { parse(); }

  Method method() const { return _method; }
  Version version() const { return _version; }
  const std::string_view& path() const { return _path; }
};

template<typename T>
using Matcher = std::tuple<Method, std::regex, T>;

template<typename T>
class Server {
  using AddressType = net::SocketAddressIPv4;
  using MatchersType = T;

  net::Server<AddressType> _server;
  MatchersType _matchers;

  template<typename Matcher>
  bool match(const std::string_view& path,
      const Matcher& matcher,
      net::Connection& conn,
      const Request& req) {
    if (std::regex_match(std::begin(path), std::end(path), std::get<1>(matcher))) {
      // TODO: Match method
      std::get<2>(matcher)(conn, req);
      return true;
    }
    return false;
  }

  bool match(net::Connection& conn, const Request& req) {
    return [&]<std::size_t... p>(std::index_sequence<p...>){
      return (match(req.path(), std::get<p>(_matchers), conn, req) || ...);
    }(std::make_index_sequence<std::tuple_size<MatchersType>::value>{});
  }

  void handle(net::Connection&& conn) {
    auto req_data = std::string{};
    while (net::getline(conn, req_data))
      req_data.push_back('\n');

    auto req = Request{std::move(req_data)};
    if (!match(conn, req))
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
