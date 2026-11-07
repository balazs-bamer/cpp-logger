// REVIEW.md bug 5: MessageCompact silently dropped values of types without an exact overload,
// such as long long or enums, and the converter, which direct mode calls, found them ambiguous.
// Now every integer type maps to the fixed width type of its size and signedness, and every enum
// to its underlying type, in all modes. On the Cortex-M target this also covers int and unsigned,
// because int32_t is long there.

#include "TestLog.h"

#include <cstddef>

enum Unscoped { cZero, cOne, cTwo };
enum class Scoped : uint8_t { cThree = 3u };
enum class Wide : int64_t { cMinusFour = -4 };

int main() {
  initLog();
  Log::registerCurrentTask("main");
  long const longValue = -5L;
  Log::n() << "long:" << longValue << 5UL
           << "long-long:" << -6LL << 6ULL
           << "int:" << -7 << 7u
           << "short:" << static_cast<short>(-8) << static_cast<unsigned short>(8)
           << "size_t:" << static_cast<size_t>(9)
           << "enum:" << cTwo << Scoped::cThree << Wide::cMinusFour
           << "end" << Log::end;
  // Compiled out by the error level, but must accept the same types.
  Log::i<nowtech::log::ErrorLevel::Info>() << -6LL << Scoped::cThree << Log::end;
  Log::unregisterCurrentTask();
  Log::done();

  auto const lines = outputLines();
#ifdef LOG_TEST_SHUTDOWN
  CHECK(lines.empty());
#else
  bool found = false;
  for(auto const &line : lines) {
    found = found || line == "long: -5 5 long-long: -6 6 int: -7 7 short: -8 8 size_t: 9 enum: 2 3 -4 end ";
  }
  CHECK(found);
  if(!found) {
    std::fprintf(stderr, "output:\n%s", gOutput.str().c_str());
  }
  else { // nothing to do
  }
#endif
  return finish();
}
