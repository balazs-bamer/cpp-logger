#ifndef NOWTECH_LOG_TEST_LOG
#define NOWTECH_LOG_TEST_LOG

// One Log configuration per test executable, chosen by compile definitions:
//   LOG_TEST_DIRECT           direct mode, otherwise background mode with QueueStdCircular
//   LOG_TEST_QUEUE_BOOST      background mode with QueueStdBoost
//   LOG_TEST_MESSAGE_VARIANT  MessageVariant instead of MessageCompact
//   LOG_TEST_MAX_TASKS        number of task IDs, default 2 (main and one more)
// Everything logged ends up in gOutput.

#include "TestCheck.h"
#include "LogAppInterfaceStd.h"
#include "LogConverterCustomText.h"
#include "LogSenderStdOstream.h"
#if defined(LOG_TEST_DIRECT)
#include "LogQueueVoid.h"
#elif defined(LOG_TEST_QUEUE_BOOST)
#include "LogQueueStdBoost.h"
#else
#include "LogQueueStdCircular.h"
#endif
#ifdef LOG_TEST_MESSAGE_VARIANT
#include "LogMessageVariant.h"
#else
#include "LogMessageCompact.h"
#endif
#include "Log.h"

#include <sstream>
#include <string>
#include <vector>

#ifndef LOG_TEST_MAX_TASKS
#define LOG_TEST_MAX_TASKS 2u
#endif

constexpr nowtech::log::TaskId cgMaxTaskCount = LOG_TEST_MAX_TASKS;
constexpr size_t cgQueueSize = 4096u;   // Background mode drops messages when the queue is full, keep that out of the way.
#ifdef LOG_TEST_DIRECT
constexpr size_t cgDirectBufferSize = 43u;
#else
constexpr size_t cgDirectBufferSize = 0u;
#endif
using AtomicBufferType = int32_t;
constexpr size_t cgAtomicBufferExponent = 3u;
constexpr AtomicBufferType cgAtomicBufferInvalidValue = 1234546789;

using LogAppInterface = nowtech::log::AppInterfaceStd<cgMaxTaskCount, false, 10u>;
constexpr typename LogAppInterface::LogTime cgRefreshPeriod = 2u;   // The transmitter serves atomic buffer sends when idle.
#ifdef LOG_TEST_MESSAGE_VARIANT
using LogMessage = nowtech::log::MessageVariant<8u, true>;
#else
using LogMessage = nowtech::log::MessageCompact<8u, true>;
#endif
using LogConverter = nowtech::log::ConverterCustomText<LogMessage, true, 100u, true, false>;
using LogSender = nowtech::log::SenderStdOstream<LogAppInterface, LogConverter, 123u, 123u>;
#if defined(LOG_TEST_DIRECT)
using LogQueue = nowtech::log::QueueVoid<LogMessage, LogAppInterface>;
#elif defined(LOG_TEST_QUEUE_BOOST)
using LogQueue = nowtech::log::QueueStdBoost<LogMessage, LogAppInterface, cgQueueSize>;
#else
using LogQueue = nowtech::log::QueueStdCircular<LogMessage, LogAppInterface, cgQueueSize>;
#endif
using LogAtomicBuffer = nowtech::log::AtomicBufferOperational<LogAppInterface, AtomicBufferType, cgAtomicBufferExponent, cgAtomicBufferInvalidValue>;
using LogConfig = nowtech::log::Config<true, 2, nowtech::log::TaskRepresentation::cName, cgDirectBufferSize, cgRefreshPeriod, nowtech::log::ErrorLevel::Error>;
using Log = nowtech::log::Log<LogQueue, LogSender, LogAtomicBuffer, LogConfig>;

inline std::ostringstream gOutput;
inline nowtech::log::LogFormatConfig gLogFormatConfig;

inline void initLog() {
  LogSender::init(&gOutput);
  Log::init(gLogFormatConfig);
}

/// Only valid after Log::done(), when the transmitter has finished writing.
inline std::vector<std::string> outputLines() {
  std::vector<std::string> result;
  std::istringstream lines(gOutput.str());
  for(std::string line; std::getline(lines, line);) {
    result.push_back(line);
  }
  return result;
}

#endif
