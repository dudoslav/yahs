#include <iostream>

#include <http.hpp>

int main() {
  auto index = http::get("/?", [](const auto& m, const auto& req, auto& conn){
    conn << http::response_ok() << http::endl;
  });

  auto app = http::Application{index};
  auto server = net::make_tcp_server(8080);
  server.listen(app);

  return 0;
}
