cmake_minimum_required(VERSION 3.10)

add_library(picohttpparser STATIC picohttpparser/picohttpparser.c)
target_include_directories(picohttpparser PUBLIC picohttpparser)
