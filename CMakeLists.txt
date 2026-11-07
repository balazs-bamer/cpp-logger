cmake_minimum_required(VERSION 3.17)
project(cpp_logger)

set(CMAKE_CXX_STANDARD 17)

include_directories(cpp-memory-manager)
include_directories(src)

add_compile_options(-Os -Wall -Wextra -Wpedantic)
add_link_options(-lpthread)

add_executable(test-stdthreadostream-boost test/test-stdthreadostream-boost.cpp)
add_executable(test-stdthreadostream-circular test/test-stdthreadostream-circular.cpp)
add_executable(test-sizes-atomic-stdostream-boost test/test-sizes-atomic-stdostream-boost.cpp)
add_executable(test-sizes-stdthreadostream-boost test/test-sizes-stdthreadostream-boost.cpp)
add_executable(test-sizes-stdthreadostream-circular test/test-sizes-stdthreadostream-circular.cpp)

enable_testing()
add_subdirectory(test/regression)
