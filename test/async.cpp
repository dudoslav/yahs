#include <iostream>
#include <memory>
#include <future>

#include <async.hpp>

static constexpr auto CORES = 4;

int main() {
  auto queue = async::Queue<std::unique_ptr<int>>{};

  auto tasks = std::array<std::future<void>, CORES>{};
  for (auto& task: tasks)
    task = std::async([&queue](){
        for (int i = 0; i < 100; ++i)
          queue.push(std::make_unique<int>(i));
        });

  for (auto& task: tasks)
    task.get();

  std::cout << queue.size() << std::endl;

  auto worker = [](auto ui){
    std::cout << std::this_thread::get_id() << " " << *ui << std::endl;
  };
  auto pool = async::Pool<std::unique_ptr<int>>{worker};
  for (int i = 0; i < 1000; ++i)
    pool.run(std::make_unique<int>(i));

  using namespace std::chrono_literals;
  std::this_thread::sleep_for(2s);

  return 0;
}
