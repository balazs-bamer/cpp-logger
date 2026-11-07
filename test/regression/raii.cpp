// REVIEW.md agreed redesign point 1, RAII group termination: a group ends at Log::end, or when its
// chain helper is destroyed, a temporary at the end of its full expression and a named helper at the
// end of its scope. Helpers are move-only, moving hands the group over. Each case below logs one or
// more lines, some over several statements, and the captured output must equal the expected text.

#include "TestLog.h"

#include <string>
#include <type_traits>
#include <utility>

using Helper = decltype(Log::n());
using FilteredHelper = decltype(Log::n<nowtech::log::ErrorLevel::Info>());   // Compiled out by the error level.
static_assert(!std::is_copy_constructible_v<Helper> && !std::is_copy_assignable_v<Helper>);
static_assert(std::is_nothrow_move_constructible_v<Helper> && std::is_nothrow_destructible_v<Helper>);
static_assert(std::is_same_v<decltype(Log::n() << 1), Helper &&>);   // So auto logger = Log::n() << 1; moves.
static_assert(std::is_same_v<decltype(std::declval<Helper &>() << 1), Helper &>);
static_assert(!std::is_copy_constructible_v<FilteredHelper> && std::is_trivially_destructible_v<FilteredHelper>);

template<typename tLogger>
void addItems(tLogger &aLogger) {
  aLogger << "added" << 7;
}

template<typename tLogger>
void takeOver(tLogger aLogger) {   // Owns the group from here on.
  aLogger << "taken";
}

int main() {
  gLogFormatConfig.tickFormat.invalidate();   // The Log::i() header is then just the task name.
  initLog();
  Log::registerCurrentTask("main");

  Log::n() << "end" << 1 << Log::end;
  Log::n() << "temporary" << 2;   // Ends at the semicolon.
  Log::i() << "header" << 3;

  {   // Several statements, ended by Log::end. The destructor must not end it again.
    auto logger = Log::n() << "block";
    logger << "with" << 4;
    logger << "end";
    logger << Log::end;
    Log::n() << "after" << "end";
  }

  {   // Several statements, ended at the end of the scope.
    auto logger = Log::n();
    logger << "scope";
    for(int i = 0; i < 3; ++i) {
      logger << i;
    }
    addItems(logger);
  }

  {   // Moving hands the group over, only the new owner ends it.
    auto logger = Log::n() << "moved";
    auto other = std::move(logger);
    other << 5;
    logger << "lost";   // The moved-from helper owns nothing, so this is dropped.
  }

  {   // A function taking the helper by value ends the group when its parameter is destroyed.
    auto logger = Log::n() << "function";
    takeOver(std::move(logger));
    Log::n() << "next";
  }

  {
    auto filtered = Log::n<nowtech::log::ErrorLevel::Info>() << "filtered";
    filtered << "out";
  }
  Log::f(Log::n(), "fold", 6);
  Log::unregisterCurrentTask();
  Log::done();

#ifdef LOG_TEST_SHUTDOWN
  std::string const expected;
#else
  std::string const expected =
    "-=- Registered task: main 1 \n"
    "end 1 \n"
    "temporary 2 \n"
    "main header 3 \n"
    "block with 4 end \n"
    "after end \n"
    "scope 0 1 2 added 7 \n"
    "moved 5 \n"
    "function taken \n"
    "next \n"
    "fold 6 \n"
    "-=- Unregistered task: 1 \n";
#endif
  CHECK(gOutput.str() == expected);
  if(gOutput.str() != expected) {
    std::fprintf(stderr, "output:\n%s---\nexpected:\n%s---\n", gOutput.str().c_str(), expected.c_str());
  }
  else { // nothing to do
  }
  return finish();
}
