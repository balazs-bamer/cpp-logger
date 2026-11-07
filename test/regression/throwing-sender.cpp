// REVIEW.md bug 7: a sender's send() runs in noexcept code, so Log must reject a sender whose send()
// may throw. CMake expects building this to fail with Log's static_assert message.

#include "TestLog.h"

#include <utility>

class ThrowingSender final {
public:
  using tAppInterface_   = LogAppInterface;
  using tConverter_      = LogConverter;
  using ConversionResult = typename LogConverter::ConversionResult;
  using Iterator         = typename LogConverter::Iterator;

  static constexpr bool csVoid = false;

  static void init() noexcept {
  }

  static void done() noexcept {
  }

  static void send(char const * const, char const * const) {   // Not noexcept.
  }

  static auto getBuffer() noexcept {
    return std::pair<Iterator, Iterator>(nullptr, nullptr);
  }
};

using ThrowingLog = nowtech::log::Log<LogQueue, ThrowingSender, LogAtomicBuffer, LogConfig>;

int main() {
  ThrowingLog::init(gLogFormatConfig);
  ThrowingLog::done();
  return 0;
}
