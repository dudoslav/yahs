cmake_minimum_required(VERSION 3.12)

add_library(picohttpparser STATIC picohttpparser/picohttpparser.c)
target_include_directories(picohttpparser PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/picohttpparser>  
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

install(TARGETS picohttpparser DESTINATION lib EXPORT YAHS)
install(FILES picohttpparser/picohttpparser.h DESTINATION include)
