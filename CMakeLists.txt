cmake_minimum_required(VERSION 3.10)

# set the project name
project(YAHS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
add_executable(server main.cpp)
target_link_libraries(server Threads::Threads)