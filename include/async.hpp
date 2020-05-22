#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <vector>
#include <functional>

namespace async {

template<typename T>
class Queue {
  std::queue<T> _queue;
  mutable std::mutex _mutex;

public:

  template<typename... Args>
  void push(Args&&... args) {
    const auto lock = std::lock_guard{_mutex};
    _queue.emplace(std::forward<Args>(args)...);
  }

  T pop() {
    const auto lock = std::lock_guard{_mutex};
    auto result = std::move(_queue.front());
    _queue.pop();
    return result;
  }

  bool empty() const {
    const auto lock = std::lock_guard{_mutex};
    return _queue.empty();
  }

  std::size_t size() const {
    const auto lock = std::lock_guard{_mutex};
    return _queue.size();
  }
};

template<typename... Args>
class Pool {
  std::vector<std::thread> _workers;
  Queue<std::tuple<Args...>> _jobs;
  std::mutex _mutex;
  std::condition_variable _cv;
  std::atomic<bool> _finished = false;

public:

  template<typename Fun>
  Pool(Fun&& fun, std::size_t threads = 4) {
    auto worker = [&](){
      do {
        auto ul = std::unique_lock{_mutex};
        _cv.wait(ul, [&](){ return !_jobs.empty() || _finished; });
        if (_jobs.empty()) continue;

        auto job = _jobs.pop();
        ul.unlock();
        std::apply(fun, std::move(job));
      } while (!_finished);
    };

    for (std::size_t i = 0; i < threads; ++i)
      _workers.emplace_back(worker);
  }

  ~Pool() {
    _finished = true;
    _cv.notify_all();
    for (auto& worker: _workers)
      worker.join();
  }

  void run(Args&&... args) {
    _jobs.push(std::move(args)...);
    _cv.notify_one();
  }

  void run(const Args&... args) {
    _jobs.push(args...);
    _cv.notify_one();
  }
};

} // namespace async
