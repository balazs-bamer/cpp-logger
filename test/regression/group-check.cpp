// REVIEW.md agreed redesign point 3: a debug check reports a task opening a log group while another
// one of it is still open, which corrupts both in queued mode. It goes to tAppInterface::error as
// cOverlappingGroups, which AppInterfaceStd counts. The check is on unless NDEBUG is defined, and
// NOWTECH_LOG_CHECK_GROUPS overrides that. Builds with LOG_TEST_CHECK_OFF expect no reports.

#define LOG_TEST_MAX_TASKS 3u
#include "TestLog.h"

#include <thread>
#include <utility>

#ifdef LOG_TEST_CHECK_OFF
constexpr uint32_t cgReport = 0u;
#else
constexpr uint32_t cgReport = 1u;
#endif

uint32_t overlaps() {
  return LogAppInterface::getErrorCount(nowtech::log::Exception::cOverlappingGroups);
}

int nestedLog() {
  Log::n() << "nested";
  return 1;
}

int main() {
  initLog();
  Log::registerCurrentTask("main");

  // Groups one after the other, ended either way, and moving a helper are fine.
  Log::n() << "a" << Log::end;
  Log::n() << "b";
  {
    auto logger = Log::n() << "c";
    logger << 1;
  }
  {
    auto logger = Log::n() << "d";
    auto other = std::move(logger);
    other << 2;
  }
  CHECK(overlaps() == 0u);

  {   // Another group starts while a named helper is open.
    auto logger = Log::n() << "open";
    Log::n() << "second";
    logger << "still";
  }
  CHECK(overlaps() == cgReport);

  Log::n() << "outer" << nestedLog();   // Computing a value logs.
  CHECK(overlaps() == 2u * cgReport);

  {   // Open groups are counted: the second inner one is reported too, although the first one ended.
    auto logger = Log::n() << "outer";
    Log::n() << "inner";
    Log::n() << "inner";
  }
  CHECK(overlaps() == 4u * cgReport);

  Log::n() << "clean" << Log::end;   // Nothing is open any more.
  CHECK(overlaps() == 4u * cgReport);

#ifndef LOG_TEST_DIRECT   // In direct mode the open helper holds the output lock, the other task would wait for it.
  {   // Each task has its own groups.
    auto logger = Log::n() << "main";
    std::thread([]{
      Log::registerCurrentTask("other");
      Log::n() << "other";
      Log::unregisterCurrentTask();
    }).join();
  }
  CHECK(overlaps() == 4u * cgReport);
#endif

  Log::unregisterCurrentTask();
  Log::done();
  return finish();
}
