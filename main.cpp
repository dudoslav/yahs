#include <iostream>
#include "http.hpp"

int main() {
  auto index = http::get("/", [](auto& conn, const auto& req){
      conn << "HTTP/1.1 200 OK\n\nIndex\n\n";
      });

  auto about = http::get("/about", [](auto& conn, const auto& req){
      conn << "HTTP/1.1 200 OK\n\nAbout\n\n";
      });

  auto server = http::make_server(8080, index, about);
  server.listen();
  return 0;
}
