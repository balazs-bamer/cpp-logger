#ifndef NOWTECH_LOG_APP_INTERFACE_STD
#define NOWTECH_LOG_APP_INTERFACE_STD

#include "Log.h"
#include <ios>
#include <mutex>
#include <chrono>
#include <thread>
#include <array>
#include <algorithm>
#include <condition_variable>

namespace nowtech::log {

template<TaskId tMaxTaskCount, bool tLogFromIsr, size_t tTaskShutdownPollPeriod>
class AppInterfaceStd final {
public:
  using LogTime = uint32_t;
  static constexpr TaskId csMaxTaskCount      = tMaxTaskCount; // Exported just to let the Log de checks.
  static constexpr TaskId csInvalidTaskId     = std::numeric_limits<TaskId>::max();
  static constexpr TaskId csIsrTaskId         = std::numeric_limits<TaskId>::min();
  static constexpr TaskId csFirstNormalTaskId = csIsrTaskId + 1u;
  static constexpr bool   csConstantTaskNames = false;

  class Occupier final {
  public:
    Occupier() = default;

    void* occupy(size_t const aSize) noexcept {
      return new std::byte[aSize];
    }

    void release(void* const aPointer) noexcept {
      delete[](static_cast<std::byte*>(aPointer));
    }
    
    void badAlloc() {
      throw std::bad_alloc();
    }
  };

private:
  /// Binary semaphore: a notify() arriving before wait() is kept, not lost.
  class Semaphore final {
  private:
    bool                    mNotified = false;   // Guarded by mMutex.
    std::mutex              mMutex;
    std::condition_variable mConditionVariable;

  public:
    void wait() noexcept {
      // The lock must be local: a mutex may only be unlocked by the thread owning it,
      // and waiting unlocks it.
      std::unique_lock<std::mutex> lock(mMutex);
      mConditionVariable.wait(lock, [this] { return mNotified; });
      mNotified = false;
    }

    void notify() noexcept {
      {
        std::lock_guard<std::mutex> lock(mMutex);
        mNotified = true;
      }
      mConditionVariable.notify_one();
    }
  };

  inline static Semaphore sTransmitterFinished;
  inline static Semaphore sAtomicBufferSent;
  inline static std::mutex sAtomicBufferSendMutex;

  inline static constexpr char csErrorMessages[static_cast<size_t>(Exception::cCount)][40] = {
    "cOutOfTaskIdsOrDoubleRegistration", "cOutOfTopics", "cSenderError", "cOverlappingGroups"
  };
  inline static constexpr char   csError[]            = "Error: ";
  inline static constexpr char   csFatalError[]       = "Fatal: ";
  inline static constexpr char   csUnknownTaskName[]  = "UNKNOWN";
  inline static constexpr char   csIsrTaskName[]      = "ISR";

  inline static thread_local TaskId shTaskId = csInvalidTaskId;
  inline static thread_local std::string shTaskName;
  inline static std::array<bool, csMaxTaskCount + 1u> sFreeTaskIds;
  inline static std::mutex sRegistrationMutex;
  inline static std::recursive_mutex sOutputMutex;
  inline static std::array<std::atomic<uint32_t>, static_cast<size_t>(Exception::cCount)> sErrorCounts;
  inline static std::thread *sTransmitterThread;
  
  AppInterfaceStd() = delete;

public:
  static void init() {
    sFreeTaskIds.fill(true);
  }

  static void init(void(*aFunction)(void)) {
    init();
    sTransmitterThread = new std::thread(aFunction);
  }

  static void done() {
    if(sTransmitterThread != nullptr) {
      sTransmitterThread->join();
      delete sTransmitterThread;
    }
    else { // nothing to do
    }
  }

  /// We assume it won't get called during registering.
  static TaskId getCurrentTaskId() noexcept {
    return shTaskId;
  }

  static TaskId registerCurrentTask(char const * const aTaskName) {
    std::lock_guard<std::mutex> lock (sRegistrationMutex);
    TaskId result;
    if(shTaskId != csInvalidTaskId) {   // Already registered, keep the ID and the name.
      result = shTaskId;
    }
    else {
      auto it = std::find(sFreeTaskIds.begin() + csFirstNormalTaskId, sFreeTaskIds.end(), true);
      if(it != sFreeTaskIds.end()) {
        shTaskId = result = static_cast<TaskId>(it - sFreeTaskIds.begin());
        shTaskName = (aTaskName != nullptr) ? aTaskName : csUnknownTaskName;
        *it = false;
      }
      else {
        result = csInvalidTaskId;
      }
    }
    return result;
  }

  static TaskId unregisterCurrentTask() {
    std::lock_guard<std::mutex> lock (sRegistrationMutex);
    if(shTaskId != csInvalidTaskId) {
      sFreeTaskIds[shTaskId] = true;
      shTaskId = csInvalidTaskId;
    }
    else { // nothing to do
    }
    return csInvalidTaskId;
  }

  // Caller will copy contents from returned pointer immediately.
  static char const * getTaskName(TaskId const) noexcept {
    return shTaskName.c_str();
  }

  static LogTime getLogTime() noexcept {
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
  }

  static void finish() noexcept {
    sTransmitterFinished.notify();
  }

  static void waitForFinished() noexcept {
    sTransmitterFinished.wait();
  }

  static void sleepWhileWaitingForTaskShutdown() noexcept {
    std::this_thread::sleep_for(std::chrono::milliseconds(tTaskShutdownPollPeriod));
  }

  /// Direct mode holds this from the first item of a line to Log::end, so lines don't mix.
  /// Recursive, because computing a logged value may log on the same thread.
  static void lock() noexcept {
    sOutputMutex.lock();
  }

  static void unlock() noexcept {
    sOutputMutex.unlock();
  }

  static void atomicBufferSendBegin() noexcept {
    sAtomicBufferSendMutex.lock();
  }

  static void atomicBufferSendWait() noexcept {
    sAtomicBufferSent.wait();
  }

  static void atomicBufferSendEnd() noexcept {
    sAtomicBufferSendMutex.unlock();
  }

  static void atomicBufferSendFinished() noexcept {
    sAtomicBufferSent.notify();
  }

  /// Counts instead of throwing: senders call it from noexcept code like the transmitter.
  static void error(Exception const aError) noexcept {
    ++sErrorCounts[static_cast<size_t>(aError)];
  }

  /// How many times error() was called with aError, for example cSenderError for failed writes.
  static uint32_t getErrorCount(Exception const aError) noexcept {
    return sErrorCounts[static_cast<size_t>(aError)];
  }

  static void fatalError(Exception const aError) {
    throw std::ios_base::failure(csErrorMessages[static_cast<size_t>(aError)]);
  }

  template<typename tClass, typename ...tParameters>
  static tClass* _new(tParameters... aParameters) {
    return ::new tClass(aParameters...);
  }

  template<typename tClass>
  static tClass* _newArray(uint32_t const aCount) {
    return ::new tClass[aCount];
  }

  template<typename tClass>
  static void _delete(tClass* aPointer) {
    ::delete aPointer;
  }

  template<typename tClass>
  static void _deleteArray(tClass* aPointer) {
    ::delete[] aPointer;
  }
};

}

#endif
