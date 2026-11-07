#ifndef NOWTECH_LOG_QUEUE_STD_BOOST
#define NOWTECH_LOG_QUEUE_STD_BOOST

#include <cstddef>
#include <mutex>
#include <condition_variable>
#include <boost/lockfree/queue.hpp>

namespace nowtech::log {

template<typename tMessage, typename tAppInterface, size_t tQueueSize>
class QueueStdBoost final {
public:
  using tMessage_ = tMessage;
  using tAppInterface_ = tAppInterface;
  using LogTime = typename tAppInterface::LogTime;

  static constexpr size_t csQueueSize = tQueueSize;

private:
  class BlockingBoostQueue final {
    boost::lockfree::queue<tMessage, boost::lockfree::capacity<tQueueSize>> mQueue;
    bool                             mNotified = false;   // Guarded by mMutex.
    std::mutex                       mMutex;
    std::condition_variable          mConditionVariable;

  public:
    /// First implementation, we assume we have plenty of memory.
    BlockingBoostQueue() noexcept = default;

    ~BlockingBoostQueue() noexcept = default;

    bool empty() const noexcept {
      return mQueue.empty();
    }

    void push(tMessage const &aMessage) noexcept {
      bool success = mQueue.bounded_push(aMessage);
      if(success) {
        {
          // Setting the flag under the mutex prevents a lost wakeup between the consumer's check and its wait.
          std::lock_guard<std::mutex> lock(mMutex);
          mNotified = true;
        }
        mConditionVariable.notify_one();
      }
      else { // nothing to do
      }
    }

    bool pop(tMessage &aMessage, LogTime const aPauseLength) noexcept {
      // Safe to call empty because there will be only one consumer.
      if(mQueue.empty()) {
        // The lock must be local: a mutex may only be unlocked by the thread owning it,
        // and waiting unlocks it.
        std::unique_lock<std::mutex> lock(mMutex);
        mConditionVariable.wait_for(lock, std::chrono::milliseconds(aPauseLength), [this]{ return mNotified; });
        mNotified = false;
      }
      else { // nothing to do
      }
      return mQueue.pop(aMessage);  // False on timeout, or on a stale flag from an already popped message.
    }
  };

  inline static BlockingBoostQueue sQueue;

  QueueStdBoost() = delete;

public:
  static void init() { // nothing to do
  }

  static void done() {  // nothing to do
  }

  static bool empty() noexcept {
    return sQueue.empty();
  }

  static void push(tMessage const &aMessage) noexcept {
    sQueue.push(aMessage);
  }

  static bool pop(tMessage &aMessage, LogTime const aPauseLength) noexcept {
    return sQueue.pop(aMessage, aPauseLength);
  }
};

}

#endif
