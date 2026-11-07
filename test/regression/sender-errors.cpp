// REVIEW.md bug 7: SenderStdOstream::send reported a failed write through AppInterfaceStd::error,
// which threw. send runs in noexcept code (the transmitter, Log::end, atomic buffer dumps), so that
// meant std::terminate, and in direct mode an exception from an item left the line's output lock
// held. Without stream exceptions a failed write went unnoticed. Now senders must not throw, and
// AppInterfaceStd counts the errors. The stream here fails every write, with LOG_TEST_STREAM_EXCEPTIONS
// it throws as well.

#define LOG_TEST_MAX_TASKS 3u
#include "TestLog.h"

#include <ostream>
#include <streambuf>
#include <thread>

static_assert(noexcept(LogSender::send(nullptr, nullptr)));

class FailingBuffer final : public std::streambuf {
protected:
  std::streamsize xsputn(char const *, std::streamsize) override {
    return 0;
  }

  int_type overflow(int_type) override {
    return traits_type::eof();
  }
};

int main() {
  FailingBuffer buffer;
  std::ostream stream(&buffer);
#ifdef LOG_TEST_STREAM_EXCEPTIONS
  stream.exceptions(std::ios::badbit | std::ios::failbit);
#endif
  LogSender::init(&stream);
  Log::init(gLogFormatConfig);
  Log::registerCurrentTask("main");
  Log::pushAtomic(1);
  Log::n() << "first" << 1 << Log::end;
  std::thread([]{   // In direct mode this needs the output lock, which a throwing first line kept.
    Log::registerCurrentTask("other");
    Log::n() << "second" << 2 << Log::end;
    Log::unregisterCurrentTask();
  }).join();
  Log::sendAtomicBuffer();
  Log::unregisterCurrentTask();
  Log::done();

  CHECK(LogAppInterface::getErrorCount(nowtech::log::Exception::cSenderError) > 0u);
  CHECK(LogAppInterface::getErrorCount(nowtech::log::Exception::cOutOfTopics) == 0u);
  return finish();
}
