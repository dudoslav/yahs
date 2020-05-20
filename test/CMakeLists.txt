cmake_minimum_required(VERSION 3.10)

add_executable(test_async async.cpp)
target_link_libraries(test_async PRIVATE yahs)

add_executable(test_http http.cpp)
target_link_libraries(test_http PRIVATE yahs)
