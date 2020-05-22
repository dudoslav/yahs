cmake_minimum_required(VERSION 3.12)

project(YAHS VERSION 0.1.0)

find_package(Threads REQUIRED)
add_subdirectory(deps)

add_library(YAHS INTERFACE)
target_link_libraries(YAHS INTERFACE Threads::Threads picohttpparser)
target_include_directories(YAHS INTERFACE
  $<BUILD_INTERFACE:${YAHS_SOURCE_DIR}/include>  
  $<INSTALL_INTERFACE:include>
)

target_compile_features(YAHS INTERFACE cxx_std_17)

# Test
enable_testing()
add_subdirectory(test)

# Install
install(TARGETS YAHS
  EXPORT YAHSTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  INCLUDES DESTINATION include
)

install(EXPORT YAHSTargets
  FILE YAHSTargets.cmake
  NAMESPACE YAHS::
  DESTINATION lib/cmake/YAHS
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file("YAHSConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES "YAHSConfig.cmake" "${CMAKE_BINARY_DIR}/YAHSConfigVersion.cmake"
  DESTINATION lib/cmake/YAHS
)

install(DIRECTORY include/ DESTINATION include/yahs
  FILES_MATCHING PATTERN "*.hpp"
)
