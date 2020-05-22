cmake_minimum_required(VERSION 3.12)

add_library(picohttpparser STATIC picohttpparser/picohttpparser.c)
target_include_directories(picohttpparser PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/picohttpparser>  
  $<INSTALL_INTERFACE:include>)

install(TARGETS picohttpparser DESTINATION lib EXPORT YAHSTargets)
install(FILES picohttpparser/picohttpparser.h DESTINATION include/yahs)
