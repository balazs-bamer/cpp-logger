# Regression tests for the bugs fixed from REVIEW.md. Each test is an executable that returns
# non-zero on failure, run them with ctest. Some bugs only show up under a sanitizer, configure
# with -DCPP_LOGGER_SANITIZER=thread or -DCPP_LOGGER_SANITIZER=address,undefined to use one.

find_package(Threads REQUIRED)
include(CheckTypeSize)
include(CheckCXXCompilerFlag)
check_type_size("long double" CPP_LOGGER_LONG_DOUBLE_SIZE LANGUAGE CXX)

set(CPP_LOGGER_SANITIZER "" CACHE STRING "Sanitizers for the regression tests, e.g. thread or address,undefined")
if(CPP_LOGGER_SANITIZER)
  add_compile_options(-fsanitize=${CPP_LOGGER_SANITIZER} -fno-sanitize-recover=all -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=${CPP_LOGGER_SANITIZER})
endif()

function(add_regression_test aName aSource)
  add_executable(${aName} ${aSource})
  target_compile_definitions(${aName} PRIVATE ${ARGN})
  target_link_libraries(${aName} PRIVATE Threads::Threads)
  add_test(NAME ${aName} COMMAND ${aName})
  set_tests_properties(${aName} PROPERTIES
    TIMEOUT 60   # A hang is the failure mode of several of these bugs.
    ENVIRONMENT "TSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/tsan.supp")
endfunction()

add_regression_test(queues queues.cpp)
add_regression_test(background-logging-circular background-logging.cpp)
add_regression_test(background-logging-boost background-logging.cpp LOG_TEST_QUEUE_BOOST)
add_regression_test(atomic-send-circular atomic-send.cpp)
add_regression_test(atomic-send-boost atomic-send.cpp LOG_TEST_QUEUE_BOOST)
add_regression_test(atomic-send-direct atomic-send.cpp LOG_TEST_DIRECT)
add_regression_test(include-all include-all.cpp)
add_regression_test(registration-background registration.cpp)
add_regression_test(registration-direct registration.cpp LOG_TEST_DIRECT)
add_regression_test(null-strings-compact null-strings.cpp)
add_regression_test(null-strings-variant null-strings.cpp LOG_TEST_MESSAGE_VARIANT)
add_regression_test(null-strings-direct null-strings.cpp LOG_TEST_DIRECT)
add_regression_test(std-string-compact std-string.cpp)
add_regression_test(std-string-variant std-string.cpp LOG_TEST_MESSAGE_VARIANT)
add_regression_test(std-string-direct std-string.cpp LOG_TEST_DIRECT)
add_regression_test(std-string-shutdown std-string.cpp LOG_TEST_SHUTDOWN)
add_regression_test(types-compact types.cpp)
add_regression_test(types-variant types.cpp LOG_TEST_MESSAGE_VARIANT)
add_regression_test(types-direct types.cpp LOG_TEST_DIRECT)
add_regression_test(types-shutdown types.cpp LOG_TEST_SHUTDOWN)
add_regression_test(direct-threads direct-threads.cpp LOG_TEST_DIRECT)
add_regression_test(raii-compact raii.cpp)
add_regression_test(raii-variant raii.cpp LOG_TEST_MESSAGE_VARIANT)
add_regression_test(raii-direct raii.cpp LOG_TEST_DIRECT)
add_regression_test(raii-shutdown raii.cpp LOG_TEST_SHUTDOWN)
add_regression_test(sender-errors-background sender-errors.cpp)
add_regression_test(sender-errors-background-exceptions sender-errors.cpp LOG_TEST_STREAM_EXCEPTIONS)
add_regression_test(sender-errors-direct sender-errors.cpp LOG_TEST_DIRECT)
add_regression_test(sender-errors-direct-exceptions sender-errors.cpp LOG_TEST_DIRECT LOG_TEST_STREAM_EXCEPTIONS)

# Code the logger must reject. Each test builds a target from the source, and passes when the build
# fails with the given part of the logger's static_assert message.
function(add_compile_fail_test aName aSource aExpectedMessage)
  add_executable(${aName} EXCLUDE_FROM_ALL ${aSource})
  target_compile_definitions(${aName} PRIVATE ${ARGN})
  target_link_libraries(${aName} PRIVATE Threads::Threads)
  add_test(NAME ${aName} COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${aName})
  set_tests_properties(${aName} PROPERTIES
    PASS_REGULAR_EXPRESSION "${aExpectedMessage}"
    RESOURCE_LOCK build-tree)   # Parallel builds in one tree can step on each other.
endfunction()

add_compile_fail_test(unsupported-pointer-background unsupported-type.cpp "can't be logged" LOG_TEST_UNSUPPORTED_POINTER)
add_compile_fail_test(unsupported-pointer-direct unsupported-type.cpp "can't be logged" LOG_TEST_UNSUPPORTED_POINTER LOG_TEST_DIRECT)
add_compile_fail_test(unsupported-pointer-shutdown unsupported-type.cpp "can't be logged" LOG_TEST_UNSUPPORTED_POINTER LOG_TEST_SHUTDOWN)
add_compile_fail_test(unsupported-pointer-filtered unsupported-type.cpp "can't be logged" LOG_TEST_UNSUPPORTED_FILTERED_POINTER)
add_compile_fail_test(unsupported-wide-char unsupported-type.cpp "can't be logged" LOG_TEST_UNSUPPORTED_WIDE_CHAR LOG_TEST_DIRECT)
add_compile_fail_test(unsupported-string-view unsupported-type.cpp "can't be logged" LOG_TEST_UNSUPPORTED_STRING_VIEW)
add_compile_fail_test(unsupported-float-background unsupported-type.cpp "Floating point logging is turned off" LOG_TEST_UNSUPPORTED_FLOAT LOG_TEST_NO_FLOAT)
add_compile_fail_test(unsupported-float-direct unsupported-type.cpp "Floating point logging is turned off" LOG_TEST_UNSUPPORTED_FLOAT LOG_TEST_NO_FLOAT LOG_TEST_DIRECT)
if(CPP_LOGGER_LONG_DOUBLE_SIZE GREATER 8)   # Longer than the 8 byte test payload.
  add_compile_fail_test(unsupported-long-double-background unsupported-type.cpp "doesn't fit the message payload" LOG_TEST_UNSUPPORTED_LONG_DOUBLE)
endif()
add_compile_fail_test(throwing-sender throwing-sender.cpp "Senders must not throw")

# FreeRTOS parts, against the declaration-only headers in freertos-stub.
add_regression_test(freertos-registration freertos-registration.cpp)
target_include_directories(freertos-registration PRIVATE freertos-stub)

add_library(freertos-log-direct OBJECT freertos-log.cpp)   # Compile only.
target_include_directories(freertos-log-direct PRIVATE freertos-stub)
add_library(freertos-log-background OBJECT freertos-log.cpp)
target_compile_definitions(freertos-log-background PRIVATE LOG_TEST_BACKGROUND)
target_include_directories(freertos-log-background PRIVATE freertos-stub)

# test/test-freertosminimal.cpp logs long double values. They fit its 8 byte message payload
# only where long double has 8 bytes, as on the Cortex-M target.
check_cxx_compiler_flag(-mlong-double-64 CPP_LOGGER_HAVE_LONG_DOUBLE_64)
if(CPP_LOGGER_LONG_DOUBLE_SIZE EQUAL 8 OR CPP_LOGGER_HAVE_LONG_DOUBLE_64)
  add_library(freertos-demo OBJECT ../test-freertosminimal.cpp)   # Compile only.
  target_include_directories(freertos-demo PRIVATE freertos-stub)
  if(NOT CPP_LOGGER_LONG_DOUBLE_SIZE EQUAL 8)
    target_compile_options(freertos-demo PRIVATE -mlong-double-64)
  endif()
else()
  message(STATUS "Not compiling test/test-freertosminimal.cpp, it needs an 8 byte long double")
endif()
