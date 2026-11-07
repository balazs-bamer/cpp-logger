// REVIEW.md bug 6: in direct mode AppInterfaceStd::lock() and unlock() did nothing, and Log only
// called them around each item anyway, so lines logged by several threads came out mixed, and
// the threads wrote the output stream at the same time. Now a line holds the lock from its first
// item to its end, at Log::end or in the helper's destructor. Every line must come out whole, an
// atomic buffer dump included. A value whose computation logs on the same thread must not
// deadlock, the lock is recursive.

#define LOG_TEST_MAX_TASKS 5u
#include "TestLog.h"

#include <array>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

constexpr uint32_t cgThreadCount = 4u;
constexpr uint32_t cgLinesPerThread = 200u;
char const * const cgThreadNames[cgThreadCount] = {"t0", "t1", "t2", "t3"};

int nestedLog() {
  Log::n() << "nested" << Log::end;
  return 1;
}

int main() {
  initLog();
  Log::registerCurrentTask("main");
  for(int32_t i = 0; i < (1 << cgAtomicBufferExponent); ++i) {
    Log::pushAtomic(i);
  }

  StartGate gate;
  std::vector<std::thread> threads;
  for(uint32_t thread = 0u; thread < cgThreadCount; ++thread) {
    threads.emplace_back([thread, &gate]{
      Log::registerCurrentTask(cgThreadNames[thread]);
      gate.wait();
      for(uint32_t i = 0u; i < cgLinesPerThread; ++i) {   // The header is part of the line.
        if(i % 2u == 0u) {
          Log::i() << "line" << thread << i << "a" << "b" << "c" << "end" << Log::end;
        }
        else {
          auto logger = Log::i() << "line" << thread;   // Ended by the destructor, which also releases the lock.
          logger << i << "a" << "b";
          logger << "c" << "end";
        }
      }
      Log::unregisterCurrentTask();
    });
  }
  gate.open();
  Log::sendAtomicBuffer();
  for(auto &thread : threads) {
    thread.join();
  }
  Log::n() << "outer" << nestedLog() << Log::end;
  Log::unregisterCurrentTask();
  Log::done();

  std::array<std::array<uint32_t, cgLinesPerThread>, cgThreadCount> seen{};
  uint32_t broken = 0u;
  uint32_t atomicDumps = 0u;
  bool nested = false;
  for(auto const &line : outputLines()) {
    char name[16];
    unsigned tick;
    unsigned thread;
    unsigned i;
    int consumed = -1;
    std::sscanf(line.c_str(), "%15s %u line %u %u a b c end%n", name, &tick, &thread, &i, &consumed);
    if(consumed > 0 && static_cast<size_t>(consumed) + 1u == line.size() && thread < cgThreadCount && i < cgLinesPerThread && cgThreadNames[thread] == std::string(name)) {
      ++seen[thread][i];
    }
    else if(line == "0 1 2 3 4 5 6 7 ") {
      ++atomicDumps;
    }
    else if(line.rfind("-=- ", 0) == 0) {   // Registration.
    }
    else if(line.rfind("outer nested", 0) == 0 || line == "1 ") {   // The nested line ends in the middle of the outer one.
      nested = true;
    }
    else {
      ++broken;
    }
  }
  uint32_t missing = 0u;
  for(auto const &perThread : seen) {
    for(auto const count : perThread) {
      missing += (count != 1u) ? 1u : 0u;
    }
  }
  CHECK(broken == 0u);
  CHECK(missing == 0u);
  CHECK(atomicDumps == 1u);
  CHECK(nested);
  return finish();
}
