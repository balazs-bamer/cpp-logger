#ifndef NOWTECH_LOG_QUEUE_STD_CIRCULAR
#define NOWTECH_LOG_QUEUE_STD_CIRCULAR

#include <array>
#include <cstddef>
#include <mutex>
#include <condition_variable>

namespace nowtech::log {

template<typename tMessage, typename tAppInterface, size_t tQueueSize>
class QueueStdCircular final {
public:
  using tMessage_ = tMessage;
  using tAppInterface_ = tAppInterface;
  using LogTime = typename tAppInterface::LogTime;

  static constexpr size_t csQueueSize = tQueueSize;

private:
  class FreeRtosQueue final {
    std::array<tMessage, tQueueSize> mQueue;
    size_t                           mNextWrite;
    size_t                           mNextRead;
    std::atomic<size_t>              mOccupied;
    std::mutex                       mMutexPush;
    std::mutex                       mMutexCv;
    std::condition_variable          mConditionVariable;

  public:
    /// First implementation, we assume we have plenty of memory.
    FreeRtosQueue() noexcept
      : mNextWrite(0u)
      , mNextRead(0u)
      , mOccupied(0u) {
    }

    ~FreeRtosQueue() noexcept = default;

    bool empty() const noexcept {
      return mOccupied == 0u;
    }

    void push(tMessage const &aMessage) noexcept {
      bool pushed;
      {
        std::lock_guard<std::mutex> lock(mMutexPush);
        pushed = mOccupied < tQueueSize;
        if(pushed) {
          mQueue[mNextWrite] = aMessage;
          mNextWrite = (mNextWrite + 1u) % tQueueSize;
          ++mOccupied;
        }
        else { // nothing to do
        }
      }
      if(pushed) {
        {
          // Passing through the mutex after incrementing mOccupied prevents a lost wakeup
          // between the consumer's check and its wait. Uncontended unless the consumer is waiting.
          std::lock_guard<std::mutex> lock(mMutexCv);
        }
        mConditionVariable.notify_one();
      }
      else { // nothing to do
      }
    }

    bool pop(tMessage &aMessage, LogTime const aPauseLength) noexcept {
      bool result = true;
      // Safe to read the slot without lock because there will be only one consumer.
      if(mOccupied == 0u) {
        // The lock must be local: a mutex may only be unlocked by the thread owning it,
        // and waiting unlocks it.
        std::unique_lock<std::mutex> lock(mMutexCv);
        result = mConditionVariable.wait_for(lock, std::chrono::milliseconds(aPauseLength), [this]{ return mOccupied > 0u; });
      }
      else { // nothing to do
      }
      if(result) {
        aMessage = mQueue[mNextRead];
        mNextRead = (mNextRead + 1u) % tQueueSize;
        --mOccupied;
      }
      else { // nothing to do
      }
      return result;
    }
  };

  inline static FreeRtosQueue sQueue;

  QueueStdCircular() = delete;

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
