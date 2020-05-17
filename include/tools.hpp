#pragma once

#include <csignal>
#include <stdexcept>
#include <string>

namespace tools {

class SignalGuard {
  const int _sig;

  static void handle(int sig) {
    throw std::runtime_error{std::to_string(sig)};
  }

public:

  SignalGuard(int sig) : _sig(sig) { std::signal(_sig, &SignalGuard::handle); }
  ~SignalGuard() { std::signal(_sig, SIG_DFL); }
};

auto make_interrupt_guard() {
  return SignalGuard{SIGINT};
}

} // namespace tools
