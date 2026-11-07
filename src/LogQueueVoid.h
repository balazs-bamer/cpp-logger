#ifndef NOWTECH_LOG_QUEUE_VOID
#define NOWTECH_LOG_QUEUE_VOID

#include <cstddef>

namespace nowtech::log {

template<typename tMessage, typename tAppInterface>
class QueueVoid final {
public:
  using tMessage_ = tMessage;
  using tAppInterface_ = tAppInterface;
  using LogTime = typename tAppInterface::LogTime;

  static constexpr size_t csQueueSize = 0u;

private:
  QueueVoid() = delete;

public:
  static void init() { // nothing to do
  }

  static void done() {  // nothing to do
  }

  static bool empty() noexcept {
    return true;
  }

  static void push(tMessage const) noexcept { // nothing to do
  }

  static bool pop(tMessage &, LogTime const) noexcept { // nothing to do
    return false;
  }
};

}

#endif
