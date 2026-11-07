// REVIEW.md bug 1: the std queues kept their wait mutex locked by the thread that ran static
// initialisation, and the transmitter thread unlocked it while waiting. The circular queue's
// unsynchronised wake-up flag could also go stale, so pop() took an item from an empty queue,
// returning old items again and wrapping the item count around.
// Both queues are driven directly by several producers and one consumer, which runs on its own
// thread like the transmitter. The counting checks catch the stale-flag pops in most runs, the
// cross-thread unlock needs CPP_LOGGER_SANITIZER=thread.

#include "TestCheck.h"
#include "LogAppInterfaceStd.h"
#include "LogQueueStdBoost.h"
#include "LogQueueStdCircular.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

struct Item final {   // Trivially copyable, as boost::lockfree::queue requires.
  uint32_t mProducer;
  uint32_t mSequence;
};

constexpr uint32_t cgRounds = 10u;   // The stale flag race is rare, give it several chances.
constexpr uint32_t cgProducerCount = 4u;
constexpr uint32_t cgItemsPerProducer = 10000u;
constexpr uint32_t cgItemCount = cgProducerCount * cgItemsPerProducer;
constexpr size_t cgQueueSize = cgItemCount;   // push() drops when full, so make room for everything.

using AppInterface = nowtech::log::AppInterfaceStd<1u, false, 10u>;

template<typename tQueue>
void testRound() {
  StartGate gate;
  std::vector<std::thread> producers;
  for(uint32_t producer = 0u; producer < cgProducerCount; ++producer) {
    producers.emplace_back([producer, &gate]{
      gate.wait();
      for(uint32_t i = 0u; i < cgItemsPerProducer; ++i) {
        tQueue::push(Item{producer, i});
      }
    });
  }

  uint32_t received = 0u;
  uint32_t unexpected = 0u;   // Out of order, repeated or garbage.
  bool emptyAfterwards = false;
  std::thread consumer([&]{
    // pop() may return false early on the boost queue (stale flag), so stop on a deadline, not on a false.
    std::array<uint32_t, cgProducerCount> next{};
    Item item;
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while(received < cgItemCount && std::chrono::steady_clock::now() < deadline) {
      if(tQueue::pop(item, 10u)) {
        if(item.mProducer < cgProducerCount && item.mSequence == next[item.mProducer]) {
          ++next[item.mProducer];
        }
        else {
          ++unexpected;
        }
        ++received;
      }
      else { // nothing to do
      }
    }
    // Nothing left, a stale wake-up must not pop from an empty queue.
    emptyAfterwards = !tQueue::pop(item, 10u) && tQueue::empty();
  });
  gate.open();
  for(auto &producer : producers) {
    producer.join();
  }
  consumer.join();

  CHECK(received == cgItemCount);
  CHECK(unexpected == 0u);
  CHECK(emptyAfterwards);
}

template<typename tQueue>
void testQueue(char const * const aName) {
  std::printf("%s\n", aName);
  Item item;
  CHECK(!tQueue::pop(item, 1u));   // Times out when empty.
  CHECK(tQueue::empty());
  int const failuresBefore = gFailures;
  for(uint32_t round = 0u; round < cgRounds && gFailures == failuresBefore; ++round) {
    testRound<tQueue>();
  }
}

int main() {
  testQueue<nowtech::log::QueueStdCircular<Item, AppInterface, cgQueueSize>>("QueueStdCircular");
  testQueue<nowtech::log::QueueStdBoost<Item, AppInterface, cgQueueSize>>("QueueStdBoost");
  return finish();
}
