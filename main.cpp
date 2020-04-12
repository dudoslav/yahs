#include <iostream>
#include "http.hpp"

int main() {
  auto index = http::get("/", [](const auto& req, auto& res){
      res.ok("Index");
      });

  auto about = http::get("/about", [](const auto& req, auto& res){
      res.ok("About");
      });

  auto server = http::make_server(8080, index, about);
  server.listen();

  return 0;
}
