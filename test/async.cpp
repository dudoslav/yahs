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
  auto providers = std::vector<std::future<void>>{};
  for (auto i : {0, 1, 2, 3, 4, 5, 6, 7})
    providers.emplace_back(std::async([&, i](){
        for (int j = 0; j < 4; ++j)
          pool.run(std::make_unique<int>(i*4+j));
        }));

  for (auto& p: providers)
    p.get();

  using namespace std::chrono_literals;
  std::this_thread::sleep_for(2s);

  return 0;
}
