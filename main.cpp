#include <iostream>
#include <sstream>
#include <stdexcept>
#include <future>
#include <functional>
#include <unordered_map>

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
  operator struct sockaddr*() { return reinterpret_cast<struct sockaddr*>(&saddr); }
  operator const struct sockaddr*() const { return reinterpret_cast<const struct sockaddr*>(&saddr); }

  static SocketAddressIPv4 any(short port) { return {INADDR_ANY, port}; };
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

enum class Version { V11 };
enum class Method { GET, POST };

struct Request {
  Version version;
  Method method;
  std::string path;
  std::unordered_map<std::string, std::string> headers;

  class Builder;
};

class Request::Builder {
  Request request;

  Method parse_method(const std::string& s) {
    if (s == "GET") return request.method = Method::GET;
    if (s == "POST") return request.method = Method::POST;
    throw std::invalid_argument{"Cannot parse method: " + s};
  }

  Version parse_version(const std::string& s) {
    if (s == "HTTP/1.1") return request.version = Version::V11;
    throw std::invalid_argument{"Cannot parse version: " + s};
  }

  public:

  Builder& request_line(const std::string& line) {
    auto iss = std::istringstream{line};
    std::string word;

    iss >> word;
    parse_method(word);

    iss >> request.path;

    iss >> word;
    parse_version(word);

    return *this;
  }

  Builder& header_line(const std::string& line) {
    auto iss = std::istringstream{line};
    return *this;
  }

  Request build() {
    return request;
  }
};

template<typename T>
using Matcher = std::tuple<Method, std::string, T>;

template<typename... Args>
void on_connect(Args... args, net::Connection&& client) {
  auto req_builder = Request::Builder{};
  auto line = std::string{};
  net::getline(client, line);
  req_builder.request_line(line);
  auto req = req_builder.build();

  client << "HTTP/1.1 200 OK\n\nHelloWorld\n";
}

template<typename... Args>
auto make_server(short port, Args&&... args) {
  auto matchers = std::unordered_map<std::string, std::function<std::string()>>{};
  ((matchers[std::get<1>(args)] = std::get<2>(args)) , ...);

  return net::make_tcp_server(port, [matchers = std::move(matchers)](auto&& client){
      client << matchers.find("/")->second();
      });
}

template<typename Fun>
auto get(const std::string& path, Fun&& fun) {
  return Matcher<Fun>{Method::GET, path, std::forward<Fun>(fun)};
}

} // namespace http

int main() {
  auto index = http::get("/", [](){
        return "HTTP/1.1 200 OK\n\nHelloWorld\n";
      });

  auto server = http::make_server(8080, index);
  server.listen();
  return 0;
}
