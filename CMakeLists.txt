cmake_minimum_required(VERSION 3.17)
project(cpp_logger LANGUAGES CXX)

# Header-only library. Link cpp-logger::cpp-logger for the include directories and C++17. The STL app
# interface needs Threads::Threads and QueueStdBoost needs the Boost headers, link those as required.
# No build type is forced. The size measurements in README.md use -DCMAKE_BUILD_TYPE=MinSizeRel (-Os).
add_library(cpp_logger INTERFACE)
add_library(cpp-logger::cpp-logger ALIAS cpp_logger)
target_include_directories(cpp_logger INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/cpp-memory-manager)
target_compile_features(cpp_logger INTERFACE cxx_std_17)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(CPP_LOGGER_TOP_LEVEL ON)
else()
  set(CPP_LOGGER_TOP_LEVEL OFF)
endif()
option(CPP_LOGGER_BUILD_EXAMPLES "Build the demo programs in test/" ${CPP_LOGGER_TOP_LEVEL})
option(CPP_LOGGER_BUILD_TESTS "Build the regression tests in test/regression" ${CPP_LOGGER_TOP_LEVEL})
option(CPP_LOGGER_BUILD_BENCHMARKS "Build the benchmarks in benchmark/, downloads picobench unless CPP_LOGGER_PICOBENCH_DIR is set" ${CPP_LOGGER_TOP_LEVEL})

if(CPP_LOGGER_BUILD_EXAMPLES OR CPP_LOGGER_BUILD_TESTS OR CPP_LOGGER_BUILD_BENCHMARKS)
  find_package(Threads REQUIRED)
  find_package(Boost CONFIG REQUIRED)
  # Warnings for this project's own programs only, not imposed on users of the library.
  add_library(cpp_logger_warnings INTERFACE)
  target_compile_options(cpp_logger_warnings INTERFACE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>)
endif()

# The demos that build on a desktop. The test-sizes-freertosminimal-* ones need an STM32 project with
# 32 bit pointers, test/regression compiles test-freertosminimal.cpp against stubs.
if(CPP_LOGGER_BUILD_EXAMPLES)
  foreach(demo test-stdostream test-stdthreadostream-boost test-stdthreadostream-circular
               test-sizes-atomic-stdostream-boost test-sizes-atomic-stdostream-circular
               test-sizes-stdthreadostream-boost test-sizes-stdthreadostream-circular)
    add_executable(${demo} test/${demo}.cpp)
    target_link_libraries(${demo} PRIVATE cpp_logger cpp_logger_warnings Threads::Threads Boost::headers)
  endforeach()
endif()

if(CPP_LOGGER_BUILD_BENCHMARKS)
  set(CPP_LOGGER_PICOBENCH_DIR "" CACHE PATH "Directory containing picobench/picobench.hpp, empty to download it")
  if(NOT CPP_LOGGER_PICOBENCH_DIR)
    set(picobenchHeader ${CMAKE_CURRENT_BINARY_DIR}/picobench/include/picobench/picobench.hpp)
    if(NOT EXISTS ${picobenchHeader})
      file(DOWNLOAD https://raw.githubusercontent.com/iboB/picobench/v2.01/include/picobench/picobench.hpp ${picobenchHeader}
        EXPECTED_HASH SHA256=fc623452dfab5133388734f95dd46b9ac93ad10728cdffe1eba0948d6d9874d7
        STATUS picobenchStatus)
      list(GET picobenchStatus 0 picobenchError)
      if(picobenchError)
        file(REMOVE ${picobenchHeader})
        message(WARNING "Downloading picobench failed, not building the benchmarks: ${picobenchStatus}")
      endif()
    endif()
    if(EXISTS ${picobenchHeader})
      set(picobenchInclude ${CMAKE_CURRENT_BINARY_DIR}/picobench/include)
    endif()
  else()
    set(picobenchInclude ${CPP_LOGGER_PICOBENCH_DIR})
  endif()
  if(picobenchInclude)
    foreach(benchmark picobench-direct picobench-msg-compact-boost picobench-msg-compact-circular
                      picobench-msg-variant-boost picobench-msg-variant-circular)
      add_executable(${benchmark} benchmark/${benchmark}.cpp)
      target_include_directories(${benchmark} SYSTEM PRIVATE ${picobenchInclude})
      target_link_libraries(${benchmark} PRIVATE cpp_logger cpp_logger_warnings Threads::Threads Boost::headers)
    endforeach()
  endif()
endif()

if(CPP_LOGGER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test/regression)
endif()
