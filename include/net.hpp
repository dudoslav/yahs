#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <async.hpp>

namespace net {

class Socket {
  int handle;

  friend class Connection;

  template<typename>
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
  Socket _socket;

  public:

  Connection(Socket&& socket): _socket(std::move(socket)) {}

  // Copy
  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  // Move
  Connection(Connection&& o): _socket(std::move(o._socket)) {}
  Connection& operator=(Connection&& o) {
    _socket = std::move(o._socket);
    return *this;
  }

  // Shutdown connection
  ~Connection() { shutdown(_socket, SHUT_RDWR); }

  static constexpr char eof() { return EOF; }

  /* Input */
  Connection& read(char* s, std::size_t count) {
    // if (!*this)
    //   *s = eof();
    if (::read(_socket, s, count) == -1)
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

  operator bool() {
    int count = 0;
    ioctl(_socket, FIONREAD, &count);
    return count;
  }

  /* Output */

  Connection& write(const char* s, std::size_t count) {
    // TODO: If wrote less than count write rest
    if (::write(_socket, s, count) == -1)
      throw std::system_error{errno, std::system_category()};
    return *this;
  }

  Connection& operator<<(const std::string& s) {
    write(s.c_str(), s.size());
    return *this;
  }

  Connection& operator<<(const std::string_view& sw) {
    write(sw.data(), sw.size());
    return *this;
  }

  Connection& operator<<(const char* s) {
    return operator<<(std::string_view{s});
  }

  Connection& operator<<(char c) {
    write(&c, 1);
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

template<typename Addr>
class Server {
  Socket _socket;
  Addr _addr;

  public:

  Server(Socket&& socket, Addr sockaddr):
    _socket(std::move(socket)), _addr(sockaddr) {
    if (bind(_socket, _addr, _addr.size()) < 0)
      throw std::system_error{errno, std::system_category()};
  }

  ~Server() { shutdown(_socket, SHUT_RDWR); }

  template<typename Fun, typename... Args>
  void listen(Fun&& fun, Args&&... args) {
    struct sockaddr_in caddr;
    socklen_t caddr_len = sizeof(caddr);

    ::listen(_socket, SOMAXCONN);

    auto pool = async::Pool<net::Connection, Args...>{std::forward<Fun>(fun)};
    for (;;) {
      pool.run(args..., Connection{accept(_socket,
              reinterpret_cast<struct sockaddr*>(&caddr),
              &caddr_len)});
    }
  }
};

auto make_tcp_socket() { return Socket(AF_INET, SOCK_STREAM, 0); }

auto make_tcp_server(short port) {
  return Server(make_tcp_socket(),
      SocketAddressIPv4::any(port));
}

} // namespace net
