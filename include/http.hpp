#pragma once

#include <regex>
#include <stdexcept>
#include <algorithm>

#include <picohttpparser.h>

#include <net.hpp>

namespace http {

enum class Method { GET, POST }; // TODO: Fill
enum class Version { V11 };

std::string_view to_string(Method method) {
  switch (method) {
    case Method::GET: return "GET";
    case Method::POST: return "POST";
  }
}

std::string_view to_string(Version version) {
  switch (version) {
    case Version::V11: return "HTTP/1.1";
  }
}

Method get_method(const std::string_view& method) {
  if (method == "GET") return Method::GET;
  if (method == "POST") return Method::POST;
  throw std::invalid_argument{"Unknown method"};
}

/* HTTP ENDL */
static constexpr auto ENDL = "\r\n";
class Endl {};
constexpr auto endl = Endl{};

net::Connection& operator<<(net::Connection& conn, Endl _) {
  conn << ENDL;
  return conn;
}

// Rework as immutable class with factories
class Request {
  using Headers = std::unordered_map<std::string_view, std::string_view>;
  const std::string _data;
  Method _method;
  std::string_view _path;
  Headers _headers;
  std::string_view _body;

  public:

  template<typename T>
  Request(T&& data) : _data(std::forward<T>(data)) { parse(); }

  void parse() {
    const char* method;
    std::size_t method_len;
    const char* path;
    std::size_t path_len;
    int minor_version;
    struct phr_header headers[100];
    std::size_t headers_len = 100;

    int head = phr_parse_request(_data.c_str(), _data.size(),
        &method, &method_len,
        &path, &path_len,
        &minor_version,
        headers, &headers_len,
        0);

    if (head < 0)
      throw std::invalid_argument{"Malformed request head: " + _data};

    for (int i = 0; i != headers_len; ++i)
      _headers.emplace(
          std::string_view{headers[i].name, headers[i].name_len},
          std::string_view{headers[i].value, headers[i].value_len});

    _method = get_method({method, method_len});
    _path = {path, path_len};
    _body = {_data.data() + head, _data.size() - head};
  }

  auto& path() const {
    return _path;
  }

  auto& method() const {
    return _method;
  }

  auto& body() const {
    return _body;
  }
};

using Status = std::pair<unsigned, std::string_view>;

Status status_ok() {
  return {200, "OK"};
}

Status status_not_found() {
  return {404, "Not Found"};
}

class Response {
  using Headers = std::unordered_map<std::string, std::string>;

  Version _version;
  Status _status;
  Headers _headers;
public:
  Response(Version version, Status status):
    _version(version), _status(status) {}

  auto& version() const {
    return _version;
  }

  auto& status() const {
    return _status;
  }

  template<typename T, typename U>
  void set_header(T&& k, U&& v) {
    _headers.emplace(std::forward<T>(k), std::forward<U>(v));
  }

  friend net::Connection& operator<<(net::Connection& conn, const Response& res) {
    auto status = res._status;
    conn << to_string(res._version)
      << ' ' << std::to_string(status.first)
      << ' ' << status.second << endl;

    for (const auto& h: res._headers)
      conn << h.first << ": " << h.second << endl;

    conn << endl;
    return conn;
  }
};

auto response_ok() {
  return Response{Version::V11, status_ok()};
}

auto response_not_found() {
  return Response{Version::V11, status_not_found()};
}

template<typename T>
using Matcher = std::tuple<Method, std::regex, T>;

template<typename... Matchers>
class Application {
  std::tuple<Matchers...> _matchers;

public:

  Application(Matchers... matchers):
    _matchers(std::forward<Matchers>(matchers)...) {}

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

  void operator()(net::Connection&& conn) {
    auto data = std::string{};
    while (net::getline(conn, data, '\n'))
      data.push_back('\n');
    data.push_back('\n');
    try {
      auto req = Request(std::move(data));

      auto matched = std::apply([&](const auto&... matchers){
          return (match(matchers, req, conn) || ...);
          }, _matchers);

      if (matched) return;

      // TODO: Allow change
      conn << response_not_found();
    } catch (std::invalid_argument e) {
      std::cerr << e.what() << std::endl;
    }
  }
};

template<typename Fun>
auto make_matcher(Method method, const std::string& path, Fun&& fun) {
  return Matcher<Fun>{method, path, std::forward<Fun>(fun)};
}

template<typename Fun>
auto get(const std::string& path, Fun&& fun) {
  return make_matcher(Method::GET, path, std::forward<Fun>(fun));
}

template<typename Fun>
auto post(const std::string& path, Fun&& fun) {
  return make_matcher(Method::POST, path, std::forward<Fun>(fun));
}

} // namespace http
