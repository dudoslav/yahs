#pragma once

#include <csignal>

namespace tools {

class SignalGuard {
  const int _sig;

  static void handle(int _) {
    throw std::exception{};
  }

public:

  SignalGuard(int sig) : _sig(sig) { std::signal(_sig, &SignalGuard::handle); }
  ~SignalGuard() { std::signal(_sig, SIG_DFL); }
};

auto make_interrupt_guard() {
  return SignalGuard{SIGINT};
}

} // namespace tools
