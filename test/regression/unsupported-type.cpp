// REVIEW.md bug 5: values the logger can't handle were silently dropped, printed as bool or
// copied past the end of the message payload. They must not compile now, in any mode. Each
// build logs one of them, chosen by a LOG_TEST_UNSUPPORTED_* definition, and CMake expects the
// build to fail with the logger's static_assert message.

#include "TestLog.h"

#include <string_view>

int main() {
  initLog();
  Log::registerCurrentTask("main");
  int const value = 0;
#if defined(LOG_TEST_UNSUPPORTED_POINTER)
  Log::n() << &value << Log::end;
#elif defined(LOG_TEST_UNSUPPORTED_FILTERED_POINTER)
  Log::i<nowtech::log::ErrorLevel::Info>() << &value << Log::end;   // Compiled out by the error level.
#elif defined(LOG_TEST_UNSUPPORTED_WIDE_CHAR)
  Log::n() << L'x' << Log::end;
#elif defined(LOG_TEST_UNSUPPORTED_STRING_VIEW)
  Log::n() << std::string_view("view") << Log::end;
#elif defined(LOG_TEST_UNSUPPORTED_FLOAT)
  Log::n() << 1.5f << Log::end;
#elif defined(LOG_TEST_UNSUPPORTED_LONG_DOUBLE)
  Log::n() << 1.5L << Log::end;
#else
#error "Choose the unsupported value with a LOG_TEST_UNSUPPORTED_* definition."
#endif
  Log::unregisterCurrentTask();
  Log::done();
  return value;
}
