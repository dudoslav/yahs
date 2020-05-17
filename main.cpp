#include <iostream>
#include <fstream>
#include <filesystem>
#include <http.hpp>
#include <tools.hpp>

int main() {
  auto index = http::get("/", [](const auto& m, const auto& req, auto& conn){
      std::cout << req.path() << std::endl;
      conn << http::response_ok() << '\n';
      });

  auto about = http::get("/about", [](const auto& m, const auto& req, auto& conn){
      std::cout << req.path() << std::endl;
      conn << http::response_ok() << '\n';
      });

  auto other = http::get("/([a-z]+)/?", [](const auto& m, const auto& req, auto& conn){
      auto res = http::response_ok();
      auto file = std::ifstream{m[1]};
      auto size = std::filesystem::file_size(m[1].str());
      res.set_header("Content-Length", std::to_string(size));
      conn << res;

      auto line = std::string{};
      while (std::getline(file, line))
        conn << line << '\n';
      conn << '\n';
      });

  try {
    auto ig = tools::make_interrupt_guard();
    auto server = http::make_server(8080, index, about, other);
    server.listen();
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
