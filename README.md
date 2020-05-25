# yahs
Yet Another HTTP Server

YAHS is actually not a server, but a framework for making web apps.
It was inspired by Express.js. The request handling
process is multithreaded. This project is still under development.

On my machine with 4 cores (8 threads) I can get around 12k requests/sec.
Tests were done using ApacheBench `ab -n 10000 -c 100 localhost:8080`.

# Usage

The basic server can look as follows "main.cpp":

```
#include <yahs/http.hpp>

auto main() -> int {
  auto index = http::get("/?", [](const auto& m, const auto& req, auto& conn){
      conn << http::response_ok() << http::endl;
      });
  
  auto app = http::Application{index};
  auto server = net::make_tcp_server(8080);
  server.listen(app);

  return 0;
}

```

# Installation

This project uses CMake, so the installation process should be straight forward:
```
mkdir build && cd build
cmake ..
make
sudo make install
```
