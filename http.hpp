#pragma once

#include <sstream>
#include <string_view>
#include <regex>
#include <stdexcept>
#include <algorithm>

#include "net.hpp"

namespace http {

enum class Version { V11 }; // TODO: Fill
enum class Method { GET, POST }; // TODO: Fill

static constexpr std::string_view STATUS_200 = "200 OK";
static constexpr std::string_view STATUS_404 = "404 Not Found";

static constexpr std::string_view HEADER_CONTENT_LENGTH = "Content-Length";

std::string_view to_string(Method method) {
  switch (method) {
    case Method::GET: return "GET";
    case Method::POST: return "GET";
  }
}

std::string_view to_string(Version version) {
  switch (version) {
    case Version::V11: return "HTTP/1.1";
  }
}

class Request {
  const std::string data;

  Method _method;
  std::string_view _path;
  Version _version;
  std::unordered_map<std::string_view, std::string_view> _headers;

  Method parse_method(const std::string_view& method_view) {
    if (method_view == "GET") return _method = Method::GET;
    if (method_view == "POST") return _method = Method::POST;

    throw std::invalid_argument{std::string{"Unknown method: "}
      .append(method_view.begin(), method_view.end())};
  }

  Version parse_version(const std::string_view& version_view) {
    if (version_view == "HTTP/1.1") return _version = Version::V11;

    throw std::invalid_argument{std::string{"Unknown version: "}
      .append(version_view.begin(), version_view.end())};
  }

  void parse() {
    // This section is a mess and should be rewritten
    auto pos = data.find_first_of(' ');
    parse_method({data.c_str(), pos});

    auto spos = data.find_first_of(' ', pos + 1);
    _path = {&data[pos + 1], spos - pos - 1};

    pos = spos;
    spos = data.find_first_of('\n', pos);
    parse_version({&data[pos + 1], spos - pos - 2});

    auto tpos = spos;
    for (;;) {
      pos = tpos + 1;
      spos = data.find_first_of(':', pos);
      if (spos == data.npos) return;
      tpos = data.find_first_of('\n', spos + 1);
      _headers.emplace(std::string_view{&data[pos], spos - pos},
          std::string_view{&data[spos + 2], tpos - spos - 3});
    }
  }

  public:

  Request(std::string&& data): data(data) { parse(); }

  Method method() const { return _method; }
  Version version() const { return _version; }
  const std::string_view& path() const { return _path; }
  const auto& headers() const { return _headers; }
};

class Response {
  Version _version = Version::V11;
  std::string_view _status = STATUS_404;
  std::unordered_map<std::string, std::string> _headers;

  std::string _body;

  public:

  Version version() const { return _version; }
  const std::string_view& status() const { return _status; }
  const std::string& body() const { return _body; }
  const auto& headers() const { return _headers; }

  template<typename T>
  void ok(T&& body) {
    _status = STATUS_200;
    _headers.emplace(HEADER_CONTENT_LENGTH, std::to_string(std::size(body)));
    _body = std::forward<T>(body);
  }
};

net::Connection& operator<<(net::Connection& conn, const Response& res) {
  conn << to_string(res.version()) << ' ' << res.status() << '\n';
  for (const auto& h: res.headers())
    conn << h.first << ": " << h.second << '\n';
  conn << '\n';
  conn << res.body() << '\n';
  return conn;
}

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
      const Request& req,
      Response& res) {
    if (std::get<0>(matcher) != req.method())
      return false;

    if (std::regex_match(std::begin(path), std::end(path), std::get<1>(matcher))) {
      std::get<2>(matcher)(req, res);
      return true;
    }

    return false;
  }

  bool match(const Request& req, Response& res) {
    return [&]<std::size_t... p>(std::index_sequence<p...>){
      return (match(req.path(), std::get<p>(_matchers), req, res) || ...);
    }(std::make_index_sequence<std::tuple_size<MatchersType>::value>{});
  }

  void handle(net::Connection&& conn) {
    auto req_data = std::string{};
    while (net::getline(conn, req_data))
      req_data.push_back('\n');

    auto req = Request{std::move(req_data)};
    auto res = Response{};
    match(req, res);
    conn << res;
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
