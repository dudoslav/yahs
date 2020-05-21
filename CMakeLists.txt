cmake_minimum_required(VERSION 3.12)

project(YAHS VERSION 0.1.0)

include(GNUInstallDirs)

find_package(Threads REQUIRED)
add_subdirectory(deps)

add_library(yahs INTERFACE)
target_link_libraries(yahs INTERFACE Threads::Threads picohttpparser)
target_include_directories(yahs INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>  
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(yahs INTERFACE cxx_std_17)

# Test
enable_testing()
add_subdirectory(test)

# Install
install(TARGETS yahs
  EXPORT YAHS
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

include(CMakePackageConfigHelpers)
write_basic_package_version_file("YAHSConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/YAHSConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/YAHSConfig.cmake"
  INSTALL_DESTINATION
  ${CMAKE_INSTALL_DATAROOTDIR}/YAHS/cmake)

install(EXPORT YAHS
  FILE YAHSTargets.cmake
  NAMESPACE YAHS::
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/YAHS/cmake)

install(FILES "${PROJECT_BINARY_DIR}/YAHSConfig.cmake"
  "${PROJECT_BINARY_DIR}/YAHSConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/YAHS/cmake)

file(GLOB HEADER_FILES "include/*.hpp")
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include
  FILES_MATCHING PATTERN "*.hpp"
  )
