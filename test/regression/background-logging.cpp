// REVIEW.md bug 1 through Log: several tasks log concurrently in background mode, and Log::done()
// waits for the transmitter on AppInterfaceStd's semaphore, which used the same cross-thread lock
// as the queues and shared its wake-up with the atomic buffer send. Every line must arrive exactly
// once and the test must finish within the CTest timeout. The cross-thread unlock itself needs
// CPP_LOGGER_SANITIZER=thread.

#define LOG_TEST_MAX_TASKS 5u
#include "TestLog.h"

#include <array>
#include <cstdio>
#include <thread>
#include <vector>

constexpr uint32_t cgThreadCount = 4u;
constexpr uint32_t cgLinesPerThread = 200u;
char const * const cgThreadNames[cgThreadCount] = {"t0", "t1", "t2", "t3"};

int main() {
  initLog();
  Log::registerCurrentTask("main");
  StartGate gate;
  std::vector<std::thread> threads;
  for(uint32_t thread = 0u; thread < cgThreadCount; ++thread) {
    threads.emplace_back([thread, &gate]{
      Log::registerCurrentTask(cgThreadNames[thread]);
      gate.wait();
      for(uint32_t i = 0u; i < cgLinesPerThread; ++i) {
        if(i % 2u == 0u) {
          Log::n() << "line" << thread << i << Log::end;
        }
        else {
          Log::n() << "line" << thread << i;   // Ended by the helper's destructor.
        }
      }
      Log::unregisterCurrentTask();
    });
  }
  gate.open();
  for(auto &thread : threads) {
    thread.join();
  }
  Log::unregisterCurrentTask();
  Log::done();

  std::array<std::array<uint32_t, cgLinesPerThread>, cgThreadCount> seen{};
  uint32_t other = 0u;
  for(auto const &line : outputLines()) {
    unsigned thread;
    unsigned i;
    if(std::sscanf(line.c_str(), "line %u %u", &thread, &i) == 2 && thread < cgThreadCount && i < cgLinesPerThread) {
      ++seen[thread][i];
    }
    else if(line.rfind("-=- ", 0) != 0) {   // Not a registration line.
      ++other;
    }
    else { // nothing to do
    }
  }
  uint32_t missing = 0u;
  uint32_t repeated = 0u;
  for(auto const &perThread : seen) {
    for(auto const count : perThread) {
      missing += (count == 0u) ? 1u : 0u;
      repeated += (count > 1u) ? 1u : 0u;
    }
  }
  CHECK(missing == 0u);
  CHECK(repeated == 0u);
  CHECK(other == 0u);
  return finish();
}
