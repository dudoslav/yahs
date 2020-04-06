#include <iostream>
#include <sstream>
#include <string_view>
#include <regex>
#include <stdexcept>
#include <future>
#include <functional>
#include <algorithm>
#include <vector>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

namespace net {

class Socket {
  int handle;

  friend class Connection;

  template<typename, typename>
  friend class Server;

  operator int() { return handle; }

  public:

  // Construtor from linux socket function
  Socket(int domain, int type, int protocol):
    Socket(socket(domain, type, protocol)) {}

  // Constructor from linux handle
  Socket(int handle): handle(handle) {
    if (handle < 0)
      throw std::system_error{errno, std::system_category()};
  }

  ~Socket() { if (handle != -1) close(handle); }

  // Copy
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  // Move
  Socket(Socket&& o): handle(o.handle) { o.handle = -1; };
  Socket& operator=(Socket&& o) {
    handle = o.handle;
    o.handle = -1;
    return *this;
  }
};

class Connection {
  Socket socket;

  public:

  Connection(Socket&& sock): socket(std::move(sock)) {}

  // Copy
  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  // Move
  Connection(Connection&& o): socket(std::move(o.socket)) {}
  Connection& operator=(Connection&& o) {
    socket = std::move(o.socket);
    return *this;
  }

  // Shutdown connection
  ~Connection() { shutdown(socket, SHUT_RDWR); }

  static constexpr char eof() { return EOF; }

  /* Input */
  Connection& read(char* s, std::size_t count) {
    // if (!*this)
    //   *s = eof();
    if (::read(socket, s, count) == -1)
      throw std::system_error{errno, std::system_category()};
    return *this;
  }

  Connection& get(char& c) {
    read(&c, 1);
    return *this;
  }

  char get() {
    char c = eof();
    get(c);
    return c;
  }

  Connection& operator>>(std::string& s) {
    return *this;
  }

  operator bool() {
    int count = 0;
    ioctl(socket, FIONREAD, &count);
    return count;
  }

  /* Output */

  Connection& write(const char* s, std::size_t count) {
    // TODO: If wrote less than count write rest
    if (::write(socket, s, count) == -1)
      throw std::system_error{errno, std::system_category()};
    return *this;
  }

  Connection& operator<<(const std::string& s) {
    write(s.c_str(), s.size());
    return *this;
  }
};

/* Connection read functions */

Connection& getline(Connection& conn, std::string& s, char delim) {
  for (char c = conn.get(); c != delim && c != conn.eof(); conn.get(c)) {
    s.push_back(c);
  }
  return conn;
}

Connection& getline(Connection& conn, std::string& s) {
  return getline(conn, s, '\n');
}

class SocketAddressIPv4 {
  struct sockaddr_in saddr;

  public:

  SocketAddressIPv4(int addr, short port) {
    saddr.sin_addr.s_addr = htonl(addr);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
  }

  socklen_t size() const { return sizeof(saddr); }
  operator struct sockaddr*()
    { return reinterpret_cast<struct sockaddr*>(&saddr); }
  operator const struct sockaddr*() const
    { return reinterpret_cast<const struct sockaddr*>(&saddr); }

  static SocketAddressIPv4 any(short port)
    { return {INADDR_ANY, port}; };
};

template<typename Addr, typename Fun>
class Server {
  Socket socket;
  Fun handler;
  Addr addr;

  public:

  Server(Socket&& sock, Addr sockaddr, Fun&& fun):
    socket(std::move(sock)), addr(sockaddr), handler(std::move(fun)) {
    if (bind(socket, sockaddr, sockaddr.size()) < 0)
      throw std::system_error{errno, std::system_category()};
  }

  void listen() {
    struct sockaddr_in caddr;
    socklen_t caddr_len = sizeof(caddr);

    ::listen(socket, SOMAXCONN);

    for (;;) {
      std::thread(handler,
          Connection{accept(socket.handle,
              reinterpret_cast<struct sockaddr*>(&caddr),
              &caddr_len)}).detach();
    }
  }
};

auto make_tcp_socket() { return Socket(AF_INET, SOCK_STREAM, 0); }

template<typename Fun>
auto make_tcp_server(short port, Fun&& fun) {
  return Server(make_tcp_socket(),
      SocketAddressIPv4::any(port),
      std::move(fun));
}

} // namespace net

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

template<typename... Args>
auto make_server(short port, Args&&... args) {
  auto matchers = std::vector<Matcher<std::function<std::string()>>>{};
  ((matchers.emplace_back(args)) , ...);

  return net::make_tcp_server(port, [matchers = std::move(matchers)](auto&& client){
      auto req_data = std::string{};
      while (net::getline(client, req_data))
        req_data.push_back('\n');

      auto req = Request{std::move(req_data)};
      auto search = std::find_if(matchers.begin(), matchers.end(), [&req](const auto& matcher){
          return std::get<0>(matcher) == req.method() &&
            std::regex_match(req.path().begin(), req.path().end(), std::get<1>(matcher));
          });
      if (search == matchers.end()) {
        client << "HTTP/1.1 404 Not Found\n";
      } else {
        client << "HTTP/1.1 200 OK\n\n" << std::get<2>(*search)();
      }
      });
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

int main() {
  auto index = http::get("/", [](){
        return "HelloWorld\n";
      });

  auto server = http::make_server(8080, index);
  server.listen();
  return 0;
}
