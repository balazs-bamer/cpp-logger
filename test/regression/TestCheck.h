#ifndef NOWTECH_LOG_TEST_CHECK
#define NOWTECH_LOG_TEST_CHECK

#include <atomic>
#include <cstdio>
#include <thread>

// Minimal assertion support for the regression tests: each test is an executable,
// CHECK records failures and finish() turns them into the exit code CTest reads.

inline int gFailures = 0;

#define CHECK(aCondition) \
  do { \
    if(!(aCondition)) { \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #aCondition); \
      ++gFailures; \
    } \
  } while(false)

/// Lets started threads run their racing part together. Without it a thread may finish before
/// the next one starts, especially on a loaded machine, and the race never happens.
class StartGate final {
  std::atomic<bool> mOpen = false;

public:
  void wait() const noexcept {
    while(!mOpen) {
      std::this_thread::yield();
    }
  }

  void open() noexcept {
    mOpen = true;
  }
};

inline int finish() {
  if(gFailures == 0) {
    std::printf("passed\n");
  }
  else {
    std::printf("%d check(s) failed\n", gFailures);
  }
  return gFailures == 0 ? 0 : 1;
}

#endif
