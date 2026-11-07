// REVIEW.md bug 2 and the task registration fixes after it:
// - Log::registerCurrentTask() passed nullptr as the name, which AppInterfaceStd copied into a
//   std::string and the registration log line dereferenced.
// - The unnamed overload was noexcept, so running out of task IDs, which throws in
//   AppInterfaceStd, called std::terminate even inside try/catch.
// - AppInterfaceStd gave a second ID to a task registering twice, and never freed the first.
// - Unregistering a task that was not registered indexed arrays with the invalid task ID.
//   That writes out of bounds without crashing, so only CPP_LOGGER_SANITIZER=address,undefined catches it.

#include "TestLog.h"

#include <atomic>
#include <exception>
#include <string>
#include <thread>

static_assert(!noexcept(Log::registerCurrentTask()));

void testUnnamedRegistration() {
  std::thread([]{
    Log::registerCurrentTask();
    Log::i() << "hello" << Log::end;
    Log::unregisterCurrentTask();
  }).join();
}

void testOutOfTaskIds() {
  // main and the holder use up both IDs.
  std::atomic<bool> holding = false;
  std::atomic<bool> release = false;
  std::thread holder([&]{
    Log::registerCurrentTask("holder");
    holding = true;
    while(!release) {
      std::this_thread::yield();
    }
    Log::unregisterCurrentTask();
  });
  while(!holding) {
    std::this_thread::yield();
  }
  bool caught = false;
  std::thread([&]{
    try {
      Log::registerCurrentTask();
    }
    catch(std::exception const &) {
      caught = true;
    }
  }).join();
  release = true;
  holder.join();
  CHECK(caught);
}

void testDoubleRegistration() {
  bool sameId = false;
  std::thread([&]{
    Log::registerCurrentTask("twice");
    auto const first = Log::getCurrentTaskId();
    Log::registerCurrentTask("twice");
    sameId = (first == Log::getCurrentTaskId());
    Log::unregisterCurrentTask();
  }).join();
  CHECK(sameId);
  bool registered = false;   // The only free ID must be free again.
  std::thread([&]{
    Log::registerCurrentTask("after");
    registered = (Log::getCurrentTaskId() != LogAppInterface::csInvalidTaskId);
    Log::unregisterCurrentTask();
  }).join();
  CHECK(registered);
}

void testUnregisterWithoutRegistration() {
  std::thread([]{
    Log::unregisterCurrentTask();
  }).join();
  std::thread([]{
    Log::registerCurrentTask("once");
    Log::unregisterCurrentTask();
    Log::unregisterCurrentTask();
  }).join();
}

int main() {
  initLog();
  Log::registerCurrentTask("main");
  try {
    testUnnamedRegistration();
    testOutOfTaskIds();
    testDoubleRegistration();
    testUnregisterWithoutRegistration();
  }
  catch(std::exception const &e) {
    std::fprintf(stderr, "unexpected exception: %s\n", e.what());
    ++gFailures;
  }
  Log::unregisterCurrentTask();
  Log::done();

  bool unnamedRegistered = false;
  bool unnamedLogged = false;   // The header is the task name, then the tick count.
  for(auto const &line : outputLines()) {
    unnamedRegistered = unnamedRegistered || line.rfind("-=- Registered task: UNKNOWN ", 0) == 0;
    unnamedLogged = unnamedLogged || (line.rfind("UNKNOWN ", 0) == 0 && line.find(" hello ") != std::string::npos);
  }
  CHECK(unnamedRegistered);
  CHECK(unnamedLogged);
  return finish();
}
