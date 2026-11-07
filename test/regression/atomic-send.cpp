// Log::sendAtomicBuffer() handshake, fixed after REVIEW.md bug 1. Concurrent callers shared one
// request flag and one wake-up, so in background mode all but one of them waited forever, and in
// direct mode they overwrote each other in the sender buffer. Each call must now produce exactly
// one intact line, and the test must finish within the CTest timeout.
// The back-to-back sends cover the transmitter clearing a request only after waking its caller,
// which lost the caller's next request. That needs the caller to preempt the transmitter, as a
// higher priority FreeRTOS task does, so on a desktop OS it is only caught by chance.

#include "TestLog.h"

#include <string>
#include <thread>
#include <vector>

constexpr uint32_t cgThreadCount = 4u;
#ifdef LOG_TEST_DIRECT
constexpr uint32_t cgSendsPerThread = 1000u;   // Quick, so many are needed to keep the threads overlapping on a loaded machine.
#else
constexpr uint32_t cgSendsPerThread = 50u;     // Each waits for the transmitter to be idle, up to cgRefreshPeriod.
#endif
constexpr uint32_t cgBackToBackSends = 100u;

int main() {
  initLog();
  for(int32_t i = 0; i < (1 << cgAtomicBufferExponent); ++i) {
    Log::pushAtomic(i);
  }

  StartGate gate;
  std::vector<std::thread> threads;
  for(uint32_t thread = 0u; thread < cgThreadCount; ++thread) {
    threads.emplace_back([&gate]{
      gate.wait();
      for(uint32_t i = 0u; i < cgSendsPerThread; ++i) {
        Log::sendAtomicBuffer();
      }
    });
  }
  gate.open();
  for(auto &thread : threads) {
    thread.join();
  }
  for(uint32_t i = 0u; i < cgBackToBackSends; ++i) {
    Log::sendAtomicBuffer();
  }
  Log::done();

  std::string const expected = "0 1 2 3 4 5 6 7 ";
  auto const lines = outputLines();
  uint32_t corrupted = 0u;
  for(auto const &line : lines) {
    corrupted += (line != expected) ? 1u : 0u;
  }
  CHECK(lines.size() == cgThreadCount * cgSendsPerThread + cgBackToBackSends);
  CHECK(corrupted == 0u);
  return finish();
}
