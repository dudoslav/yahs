#include <iostream>
#include <http.hpp>

int main() {
  auto index = http::get("/", [](const auto& m, const auto& req, auto& conn){
      std::cout << req.path() << std::endl;
      conn << "HTTP/1.1 200 OK\n\n";
      });

  auto about = http::get("/about", [](const auto& m, const auto& req, auto& conn){
      std::cout << req.path() << std::endl;
      conn << "HTTP/1.1 200 OK\n\n";
      });

  auto other = http::get("/[a-z]+/?", [](const auto& m, const auto& req, auto& conn){
      std::cout << req.path() << std::endl;
      conn << "HTTP/1.1 200 OK\n\n";
      });

  auto server = http::make_server(8080, index, about, other);
  server.listen();

  return 0;
}
