#ifndef NOWTECH_LOG
#define NOWTECH_LOG

#include "LogMessageBase.h"
#include "LogAtomicBuffers.h"
#include "PoolAllocator.h"
#include <type_traits>
#include <algorithm>
#include <string>
#include <atomic>
#include <limits>
#include <array>

namespace nowtech::log {
  
enum class Exception : uint8_t {
  cOutOfTaskIdsOrDoubleRegistration = 0u,
  cOutOfTopics                      = 1u,
  cSenderError                      = 2u,
  cCount                            = 3u
};

enum class TaskRepresentation : uint8_t {
  cNone = 0u,
  cId   = 1u,
  cName = 2u
};

using LogTopic = int8_t; // this needs to be signed to let the overload resolution work

enum class ErrorLevel : uint8_t {
  Off      = 0u,
  Fatal    = 1u,
  Error    = 2u,
  Warning  = 3u,
  Info     = 4u,
  Debug    = 5u,
  All      = 6u
};

template<typename tQueue, typename tSender, typename tAtomicBuffer, typename tLogConfig>
class Log;

class TopicInstance final {
  template<typename tQueue, typename tSender, typename tAtomicBuffer, typename tLogConfig>
  friend class Log;

public:
  static constexpr LogTopic csInvalidTopic = std::numeric_limits<LogTopic>::min();

private:
  LogTopic mValue = csInvalidTopic;

  LogTopic operator=(LogTopic const aValue) {
    mValue = aValue;
    return aValue;
  }

public:
  LogTopic operator*() const {
    return mValue;
  }

  operator LogTopic() const {
    return mValue;
  }
};

template<bool tAllowRegistrationLog, LogTopic tMaxTopicCount, TaskRepresentation tTaskRepresentation, size_t tDirectBufferSize, int32_t tRefreshPeriod, ErrorLevel tErrorLevel = ErrorLevel::All>
struct Config final {
public:
  static constexpr bool               csAllowRegistrationLog = tAllowRegistrationLog;
  static constexpr LogTopic           csMaxTopicCount        = tMaxTopicCount;
  static constexpr TaskRepresentation csTaskRepresentation   = tTaskRepresentation;
  static constexpr size_t             csDirectBufferSize     = tDirectBufferSize;
  static constexpr int32_t            csRefreshPeriod        = tRefreshPeriod; // Can represent 1s even if the unit is ns.
  static constexpr ErrorLevel         csErrorLevel           = tErrorLevel;
};

struct LogFormatConfig final {
public:
  /// This is the default logging format and the only one I will document
  /// here. For the others, the letter represents the base of the number
  /// system and the number represents the minimum digits to write, possibly
  /// with leading zeros. When formats are applied to floating point
  /// numbers, the numeric system info is discarded.
  inline static constexpr LogFormat cInvalid{ 0u,  0u};
  inline static constexpr LogFormat B4      { 2u,  4u};
  inline static constexpr LogFormat B8      { 2u,  8u};
  inline static constexpr LogFormat B12     { 2u, 12u};
  inline static constexpr LogFormat B16     { 2u, 16u};
  inline static constexpr LogFormat B24     { 2u, 24u};
  inline static constexpr LogFormat B32     { 2u, 32u};
  inline static constexpr LogFormat Fm      {10u,  0u}; // for maximum precision in floating-point types
  inline static constexpr LogFormat D1      {10u,  1u};
  inline static constexpr LogFormat D2      {10u,  2u};
  inline static constexpr LogFormat D3      {10u,  3u};
  inline static constexpr LogFormat D4      {10u,  4u};
  inline static constexpr LogFormat D5      {10u,  5u};
  inline static constexpr LogFormat D6      {10u,  6u};
  inline static constexpr LogFormat D7      {10u,  7u};
  inline static constexpr LogFormat D8      {10u,  8u};
  inline static constexpr LogFormat D16     {10u, 16u};
  inline static constexpr LogFormat X1      {16u,  1u};
  inline static constexpr LogFormat X2      {16u,  2u};
  inline static constexpr LogFormat X3      {16u,  3u};
  inline static constexpr LogFormat X4      {16u,  4u};
  inline static constexpr LogFormat X6      {16u,  6u};
  inline static constexpr LogFormat X8      {16u,  8u};
  inline static constexpr LogFormat X16     {16u, 16u};

  // Indicates the next char* value should be stored in messages instead of taking only its address
  inline static constexpr LogFormat St      {13u, LogFormat::csFillValueStoreString };

  /// Format for displaying the task ID in the message header.
  LogFormat taskIdFormat    = X2;

  /// Format for displaying the FreeRTOS ticks in the header, if any. Should be
  /// cInvalid to disable tick output.
  LogFormat tickFormat      = D5;
  LogFormat defaultFormat   = Fm;
  LogFormat atomicFormat    = Fm;

  LogFormatConfig() noexcept = default;
};

/// Dummy type to use in << chain as end marker.
enum class LogShiftChainEndMarker : uint8_t {
  cEnd      = 0u
};

template<typename tQueue, typename tSender, typename tAtomicBuffer, typename tLogConfig>
class Log final {
private:
  using tMessage = typename tQueue::tMessage_;
  using tAppInterface = typename tSender::tAppInterface_;
  using tConverter = typename tSender::tConverter_;
  using ConversionResult = typename tConverter::ConversionResult;
  using LogTime = typename tAppInterface::LogTime;
  using TopicName = char const *;
  using tAtomicBufferType = typename tAtomicBuffer::tAtomicBufferType_;

  static constexpr size_t   csDirectBufferSize         = tLogConfig::csDirectBufferSize;
  static constexpr bool     csShutdownLog              = tSender::csVoid;
  static constexpr bool     csSendInBackground         = (csDirectBufferSize == 0u); // will omit tQueue
  static constexpr size_t   csAtomicBufferSizeExponent = tAtomicBuffer::csAtomicBufferSizeExponent;
  static constexpr size_t   csAtomicBufferSize         = tAtomicBuffer::csAtomicBufferSize;
  static constexpr bool     csAtomicBufferOperational  = tAtomicBuffer::csAtomicBufferSizeExponent > 0u;
  static constexpr tAtomicBufferType csAtomicBufferInvalidValue = tAtomicBuffer::csInvalidValue;
  static constexpr size_t   csMaxAtomicBufferSizeExp   = std::max<size_t>(32u, sizeof(void*) * 8u - 1u);
  static constexpr size_t   csPayloadSizeBr            = tMessage::csPayloadSize;
  static constexpr size_t   csPayloadSizeNet           = tMessage::csPayloadSize - 1u;  // we leave space for terminal 0 to avoid counting bytes
  static constexpr int32_t  csRefreshPeriod            = tLogConfig::csRefreshPeriod;
  static constexpr size_t   csQueueSize                = tQueue::csQueueSize;
  static constexpr TaskId   csInvalidTaskId            = tAppInterface::csInvalidTaskId;
  static constexpr TaskId   csIsrTaskId                = tAppInterface::csIsrTaskId;
  static constexpr TaskId   csMaxTaskCount             = tAppInterface::csMaxTaskCount;
  static constexpr TaskId   csMaxTotalTaskCount        = tAppInterface::csMaxTaskCount + 1u;
  static constexpr size_t   csListItemOverhead         = sizeof(void*) * 8u;
  static constexpr bool     csConstantTaskNames        = tAppInterface::csConstantTaskNames;
  static constexpr bool     csAllowRegistrationLog     = tLogConfig::csAllowRegistrationLog;

  static constexpr ErrorLevel         csErrorLevel          = tLogConfig::csErrorLevel;
  static constexpr TaskRepresentation csTaskRepresentation = tLogConfig::csTaskRepresentation;

  static constexpr LogTopic csMaxTopicCount     = tLogConfig::csMaxTopicCount;
  static constexpr LogTopic csFirstFreeTopic    = 0;
  static constexpr MessageSequence csSequence0  = 0u;
  static constexpr MessageSequence csSequence1  = 1u;
  static constexpr char csTerminalChar          = 0;

  using Occupier = typename tAppInterface::Occupier;
  using Allocator = memory::PoolAllocator<tMessage, Occupier>;
  using MessageQueue = std::list<tMessage, Allocator>;
  using MessageQueueArray = std::array<MessageQueue*, csMaxTotalTaskCount>; // Need the indirection to be able use allocator in constructor call.
  // Could introduce a new list type but the performance gain would be less than a percent.
  using TaskShutdownArray = std::array<std::atomic<bool>, csMaxTotalTaskCount>;

  static_assert(csPayloadSizeNet > 0u);
  static_assert(csInvalidTaskId == std::numeric_limits<TaskId>::max());
  static_assert(csIsrTaskId == std::numeric_limits<TaskId>::min());
  static_assert(csMaxTaskCount < std::numeric_limits<TaskId>::max());
  static_assert(std::is_same_v<tAppInterface, typename tQueue::tAppInterface_>);
  static_assert(std::is_same_v<tMessage, typename tConverter::tMessage_>);
  static_assert(std::is_integral_v<tAtomicBufferType>);
  static_assert(csAtomicBufferSizeExponent <= csMaxAtomicBufferSizeExp);

  inline static constexpr char csRegisteredTask[]    = "-=- Registered task:";
  inline static constexpr char csUnregisteredTask[]  = "-=- Unregistered task:";
  
  inline static LogFormatConfig const                 *sConfig;
  inline static std::atomic<LogTopic>                  sNextFreeTopic;
  inline static std::atomic<bool>                      sKeepAliveTask;
  inline static std::array<TopicName, csMaxTopicCount> sRegisteredTopics;
  inline static TaskShutdownArray                     *sTaskShutdowns;

  inline static Occupier           sOccupier;
  inline static Allocator         *sAllocator;
  inline static MessageQueueArray *sMessageQueues;

  Log() = delete;

  /// This will be used to send via queue. It stores the first message, and sends it only with the terminal marker.
  class LogShiftChainHelperBackgroundSend final {
  private:
    inline static constexpr char      csEmptyString[] = "";
    inline static constexpr LogFormat csEmptyFormat {2u, 0u};

    TaskId          mTaskId;
    LogFormat       mNextFormat;
    MessageSequence mNextSequence;
    tMessage        mFirstMessage;
    bool            mWasMessage;

  public:
    static constexpr LogShiftChainEndMarker end = LogShiftChainEndMarker::cEnd;

    LogShiftChainHelperBackgroundSend() noexcept = delete;

    LogShiftChainHelperBackgroundSend(TaskId const aTaskId) noexcept
     : mTaskId(aTaskId)
     , mNextSequence(0u)
     , mWasMessage(false) {
      mNextFormat.invalidate();
    }

    /// Can be used in application code to eliminate further operator<< calls when the topic is disabled.
    bool isValid() const noexcept {
      return mTaskId != csInvalidTaskId;
    }

    template<typename tValue>
    LogShiftChainHelperBackgroundSend& operator<<(tValue const aValue) noexcept {
      if(mTaskId != csInvalidTaskId && mNextSequence < std::numeric_limits<MessageSequence>::max()) {
        LogFormat format = obtainFormat();
        tMessage message;
        message.set(aValue, format, mTaskId, mNextSequence);
        sendOrStore(message);
      }
      else { // silently discard value, nothing to do
      }
      return *this;
    }

    LogShiftChainHelperBackgroundSend& operator<<(char * const aValue) noexcept {
      return sendCharPointer(aValue);
    }

    LogShiftChainHelperBackgroundSend& operator<<(char const * const aValue) noexcept {
      return sendCharPointer(aValue);
    }

    LogShiftChainHelperBackgroundSend& operator<<(std::string const &aValue) noexcept {
      mNextFormat = LogFormatConfig::St;
      return sendCharPointer(aValue.c_str());
    }

    LogShiftChainHelperBackgroundSend& operator<<(LogFormat const aFormat) noexcept {
      mNextFormat = aFormat;
      return *this;
    }

    void operator<<(LogShiftChainEndMarker const) noexcept {
      if(mTaskId != csInvalidTaskId) {
        if(!mWasMessage) {
          mFirstMessage.set(csEmptyString, csEmptyFormat, mTaskId, csSequence0);
        }
        else { // nothing to do
        }
        tQueue::push(mFirstMessage);
      }
      else { // nothing to do
      }
    }

  private:
    LogShiftChainHelperBackgroundSend& sendCharPointer(char const * const aValue) noexcept {
      if(mTaskId != csInvalidTaskId && mNextSequence < std::numeric_limits<MessageSequence>::max()) {
        LogFormat format = obtainFormat();
        tMessage message;
        if(format.isStoredString()) {
          std::array<char, csPayloadSizeBr> payload;
          char const * where = aValue;
          while(*where != csTerminalChar) {
            size_t copied = 0u;
            while(*where != csTerminalChar && copied < csPayloadSizeNet) {
              payload[copied] = *where;
              ++where;
              ++copied;
            }
            payload[copied] = csTerminalChar;
            if(*where == csTerminalChar) {
              format.mFill = LogFormat::csFillValueStoreStringTerminal;
            }
            else { // nothing to do
            }
            message.set(payload, format, mTaskId, mNextSequence);
            sendOrStore(message);
          }
        }
        else {
          message.set(aValue, format, mTaskId, mNextSequence);
          sendOrStore(message);
        }
      }
      else { // silently discard value, nothing to do
      }
      return *this;
    }

    LogFormat obtainFormat() noexcept {
      LogFormat result;
      if(mNextFormat.isValid()) {
        result = mNextFormat;
        mNextFormat.invalidate();
      }
      else {
        result = sConfig->defaultFormat;
      }
      return result;
    }

    void sendOrStore(tMessage const & aMessage) noexcept {
      if(mNextSequence == csSequence0) {
        mWasMessage = true;
        mFirstMessage = aMessage;
      }
      else {
        tQueue::push(aMessage);
      }
      ++mNextSequence;
    }
  }; // class LogShiftChainHelperBackgroundSend

  /// This will be used to send directly, blocking the current thread.
  class LogShiftChainHelperDirectSend final {
    TaskId          mTaskId;
    LogFormat       mNextFormat;

  public:
    LogShiftChainHelperDirectSend() noexcept = delete;

    LogShiftChainHelperDirectSend(TaskId const aTaskId) noexcept
     : mTaskId(aTaskId) {
       mNextFormat.invalidate();
    }

    /// Can be used in application code to eliminate further operator<< calls when the topic is disabled.
    bool isValid() const noexcept {
      return mTaskId != csInvalidTaskId;
    }

    template<typename tValue>
    LogShiftChainHelperDirectSend& operator<<(tValue const aValue) {
      if(mTaskId != csInvalidTaskId) {
        LogFormat format;
        if(mNextFormat.isValid()) {
          format = mNextFormat;
          mNextFormat.invalidate();
        }
        else {
          format = sConfig->defaultFormat;
        }
        ConversionResult buffer[csDirectBufferSize];
        tConverter converter(buffer, buffer + csDirectBufferSize);
        converter.convert(aValue, format.mBase, format.mFill);
        tAppInterface::lock();
        tSender::send(buffer, converter.end());
        tAppInterface::unlock();
      }
      else { // silently discard value, nothing to do
      }
      return *this;
    }

    LogShiftChainHelperDirectSend& operator<<(LogFormat const aFormat) noexcept {
      mNextFormat = aFormat;
      return *this;
    }

    void operator<<(LogShiftChainEndMarker const) noexcept {
      if(mTaskId != csInvalidTaskId) {
        ConversionResult buffer[csDirectBufferSize];
        tConverter converter(buffer, buffer + csDirectBufferSize);
        converter.terminateSequence();
        tAppInterface::lock();
        tSender::send(buffer, converter.end());
        tAppInterface::unlock();
      }
      else { // nothing to do
      }
    }
  }; // class LogShiftChainHelperDirectSend

  /// This shuts down all logging code generation without uncommenting anything from user code, when compiled with aT least -O1 
  class LogShiftChainHelperEmpty final {
  public:
    LogShiftChainHelperEmpty() noexcept = delete;

    LogShiftChainHelperEmpty(TaskId const) noexcept {
    }

    /// Can be used in application code to eliminate further operator<< calls when the topic is disabled.
    bool isValid() const noexcept {
      return false;
    }

    template<typename tValue>
    LogShiftChainHelperEmpty& operator<<(tValue const) noexcept {
      return *this;
    }

    LogShiftChainHelperEmpty& operator<<(LogFormat const) noexcept {
      return *this;
    }

    void operator<<(LogShiftChainEndMarker const) noexcept {
    }
  }; // class LogShiftChainHelperEmpty

  using LogShiftChainHelperRaw = std::conditional_t<csSendInBackground, LogShiftChainHelperBackgroundSend, LogShiftChainHelperDirectSend>;
  using LogShiftChainHelper = std::conditional_t<csShutdownLog, LogShiftChainHelperEmpty, LogShiftChainHelperRaw>;
  template<ErrorLevel tRequestedErrorLevel>
  using LogShiftChainHelperErrorLevel = std::conditional_t<(csShutdownLog || csErrorLevel < tRequestedErrorLevel), LogShiftChainHelperEmpty, LogShiftChainHelperRaw>;

public:
  /// Will be used as Log << something << to << log << Log::end;
  static constexpr LogShiftChainEndMarker end   = LogShiftChainEndMarker::cEnd;
  static constexpr ErrorLevel             fatal = ErrorLevel::Fatal;
  static constexpr ErrorLevel             error = ErrorLevel::Error;
  static constexpr ErrorLevel             warn  = ErrorLevel::Warning;
  static constexpr ErrorLevel             info  = ErrorLevel::Info;
  static constexpr ErrorLevel             debug = ErrorLevel::Debug;
  static constexpr ErrorLevel             all   = ErrorLevel::All;

  template<typename ...tTypes>
  static void init(LogFormatConfig const &aConfig, tTypes... aArgs) {
    if constexpr(!csShutdownLog) {
      sConfig = &aConfig;
      if constexpr(csSendInBackground) {
        std::byte experiment[sizeof(tMessage) + csListItemOverhead];
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif
        tMessage example;
        size_t nodeSize = memory::AllocatorBlockGauge<std::list<tMessage>>::getNodeSize(experiment, example);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
        sAllocator = tAppInterface::template _new<Allocator>(csQueueSize, nodeSize, sOccupier);
        sMessageQueues = tAppInterface::template _new<MessageQueueArray>();
        sTaskShutdowns = tAppInterface::template _new<TaskShutdownArray>();
        auto &messageQueues = *sMessageQueues;
        for (size_t i = 0; i < csMaxTotalTaskCount; ++i) {
          messageQueues[i] = tAppInterface::template _new<MessageQueue>(*sAllocator);
        }
        sKeepAliveTask = true;
        tAppInterface::init(transmitterTaskFunction, std::forward<tTypes>(aArgs)...);
      } else {
        tAppInterface::init();
      }
      tQueue::init();
      tAtomicBuffer::init();
      sNextFreeTopic = csFirstFreeTopic;
      std::fill_n(sRegisteredTopics.begin(), csMaxTopicCount, nullptr);
    }
    else { // nothing to do
    }
  }

  // TODO note in docs about init and done sequence
  static void done() {
    if constexpr(!csShutdownLog) {
      if constexpr(csSendInBackground) {
        sKeepAliveTask = false;
        tAppInterface::waitForFinished();
        auto &messageQueues = *sMessageQueues;
        for(size_t i = 0; i < csMaxTotalTaskCount; ++i) {
          tAppInterface::template _delete<MessageQueue>(messageQueues[i]);
        }
        tAppInterface::template _delete<MessageQueueArray>(sMessageQueues);
        tAppInterface::template _delete<TaskShutdownArray>(sTaskShutdowns);
        tAppInterface::template _delete<Allocator>(sAllocator);
      }
      else { // nothing to do
      }
      tQueue::done();
      tAtomicBuffer::done();
      tSender::done();
      tAppInterface::done();
    }
    else { // nothing to do
    }
  }

  /// Registers the current task if not already present. It can register
  /// at most 254 tasks. All others will be handled as one.
  /// NOTE: this method locks to inhibit concurrent access of methods with the same name.
  static void registerCurrentTask() noexcept {
    if constexpr(!csShutdownLog) {
      registerCurrentTask(nullptr);
    }
    else { // nothing to do
    }
  }

/// Registers the current task if not already present. It can register
  /// at most 254 tasks. All others will be handled as one.
  /// @param aTaskName Task name to use, when the osInterface supports it.
  static void registerCurrentTask(char const * const aTaskName) {
    if constexpr(!csShutdownLog) {
      TaskId taskId = tAppInterface::registerCurrentTask(aTaskName);
      if(taskId != csInvalidTaskId) {
        if constexpr(csSendInBackground) {
          (*sTaskShutdowns)[taskId] = false;
        }
        else { // nothing to do
        }
        if constexpr(csAllowRegistrationLog) {
          n(taskId) << csRegisteredTask << aTaskName << taskId << end;
        }
        else { // nothing to do
        }
      }
      else {
        tAppInterface::fatalError(Exception::cOutOfTaskIdsOrDoubleRegistration);
      }
    }
    else { // nothing to do
    }
  }

  // The thread must make sure all of its pending logging is precessed when calling this.
  static void unregisterCurrentTask() noexcept {
    if constexpr(!csShutdownLog) {
      TaskId taskId = tAppInterface::getCurrentTaskId();
      if constexpr(csAllowRegistrationLog) {
        if(taskId != csInvalidTaskId) {
          n(taskId) << csUnregisteredTask << taskId << end;
        }
        else { // nothing to do
        }
      }
      else { // nothing to do
      }
      if constexpr(csSendInBackground) {
        tMessage message;
        message.setShutdown(taskId);
        tQueue::push(message);
        while(!(*sTaskShutdowns)[taskId]) {
          tAppInterface::sleepWhileWaitingForTaskShutdown();
        }
      }
      else { // nothing to do
      }
      tAppInterface::unregisterCurrentTask();
    }
    else { // nothing to do
    }
  }

  static void registerTopic(TopicInstance &aTopic, char const * const aPrefix) {
    if constexpr(!csShutdownLog) {
      aTopic = sNextFreeTopic++;
      if(aTopic >= csMaxTopicCount) {
        tAppInterface::fatalError(Exception::cOutOfTopics);
      }
      else {
        sRegisteredTopics[aTopic] = aPrefix;
      }
    }
    else { // nothing to do
    }
  }

  static TaskId getCurrentTaskId() noexcept {
    if constexpr(!csShutdownLog) {
      return tAppInterface::getCurrentTaskId();
    }
    else {
      return csInvalidTaskId;
    }
  }

  static void pushAtomic(tAtomicBufferType const &aValue) noexcept {
    if constexpr(!csShutdownLog && csAtomicBufferOperational) {
      tAtomicBuffer::push(aValue);
    }
    else { // nothing to do
    }
  }

  static void sendAtomicBuffer() noexcept {
    if constexpr(!csShutdownLog && csAtomicBufferOperational) {
      // One caller at a time: there is a single request flag, and in direct mode a single sender buffer.
      tAppInterface::atomicBufferSendBegin();
      if constexpr(csSendInBackground) {
        tAtomicBuffer::scheduleForSend();
        tAppInterface::atomicBufferSendWait();
      } else {
        doSendAtomicBuffer();
      }
      tAppInterface::atomicBufferSendEnd();
    }
    else { // nothing to do
    }
  }

  template<ErrorLevel tRequestedErrorLevel = ErrorLevel::Off>
  static LogShiftChainHelperErrorLevel<tRequestedErrorLevel> i() noexcept {
    if constexpr(!csShutdownLog && csErrorLevel >= tRequestedErrorLevel) {
      TaskId const taskId = tAppInterface::getCurrentTaskId();
      return sendHeader<LogShiftChainHelperErrorLevel<tRequestedErrorLevel>>(taskId);
    }
    else {
      return LogShiftChainHelperErrorLevel<tRequestedErrorLevel>{csInvalidTaskId};
    }
  }

  template<ErrorLevel tRequestedErrorLevel = ErrorLevel::Off>
  static LogShiftChainHelperErrorLevel<tRequestedErrorLevel> i(TaskId const aTaskId) noexcept {
    if constexpr(!csShutdownLog && csErrorLevel >= tRequestedErrorLevel) {
      return sendHeader<LogShiftChainHelperErrorLevel<tRequestedErrorLevel>>(aTaskId);
    }
    else {
      return LogShiftChainHelperErrorLevel<tRequestedErrorLevel>{csInvalidTaskId};
    }
  }

  static LogShiftChainHelper i(LogTopic const aTopic) noexcept {
    if constexpr(!csShutdownLog) {
      if(aTopic >= 0 && sRegisteredTopics[aTopic] != nullptr) {
        TaskId const taskId = tAppInterface::getCurrentTaskId();
        return sendHeader<LogShiftChainHelper>(taskId, sRegisteredTopics[aTopic]);
      }
      else {
        return sendHeader<LogShiftChainHelper>(csInvalidTaskId);
      }
    }
    else {
      return LogShiftChainHelper{csInvalidTaskId};
    }
  }

  static LogShiftChainHelper i(LogTopic const aTopic, TaskId const aTaskId) noexcept {
    if constexpr(!csShutdownLog) {
      if(aTopic >= 0 && sRegisteredTopics[aTopic] != nullptr) {
        return sendHeader<LogShiftChainHelper>(aTaskId, sRegisteredTopics[aTopic]);
      }
      else {
        return sendHeader<LogShiftChainHelper>(csInvalidTaskId);
      }
    }
    else {
      return LogShiftChainHelper{csInvalidTaskId};
    }
  }

  template<ErrorLevel tRequestedErrorLevel = ErrorLevel::Off>
  static LogShiftChainHelperErrorLevel<tRequestedErrorLevel> n() noexcept {
    if constexpr(!csShutdownLog && csErrorLevel >= tRequestedErrorLevel) {
      return LogShiftChainHelperErrorLevel<tRequestedErrorLevel>{tAppInterface::getCurrentTaskId()};
    }
    else {
      return LogShiftChainHelperErrorLevel<tRequestedErrorLevel>{csInvalidTaskId};
    }
  }

  template<ErrorLevel tRequestedErrorLevel = ErrorLevel::Off>
  static LogShiftChainHelperErrorLevel<tRequestedErrorLevel> n(TaskId const aTaskId) noexcept {
    if constexpr(!csShutdownLog && csErrorLevel >= tRequestedErrorLevel) {
      return LogShiftChainHelperErrorLevel<tRequestedErrorLevel>{aTaskId};
    }
    else {
      return LogShiftChainHelperErrorLevel<tRequestedErrorLevel>{csInvalidTaskId};
    }
  }

  static LogShiftChainHelper n(LogTopic const aTopic) noexcept {
    if constexpr(!csShutdownLog) {
      if(aTopic >= 0 && sRegisteredTopics[aTopic] != nullptr) {
        return LogShiftChainHelper{tAppInterface::getCurrentTaskId()};
      }
      else {
        return LogShiftChainHelper{csInvalidTaskId};
      }
    }
    else {
      return LogShiftChainHelper{csInvalidTaskId};
    }
  }

  static LogShiftChainHelper n(LogTopic const aTopic, TaskId const aTaskId) noexcept {
    if constexpr(!csShutdownLog) {
      if(aTopic >= 0 && sRegisteredTopics[aTopic] != nullptr) {
        return LogShiftChainHelper{aTaskId};
      }
      else {
        return LogShiftChainHelper{csInvalidTaskId};
      }
    }
    else {
      return LogShiftChainHelper{csInvalidTaskId};
    }
  }

  template<typename ...tArgs>       // Not a sophisticated solution, but why offer the possibility?
  static void f(LogShiftChainHelper aHead, tArgs &&... aArgs) noexcept {
    (aHead << ... << aArgs) << end;
  }

private:
  template <typename tLogShiftChainHelper>
  static tLogShiftChainHelper sendHeader(TaskId const aTaskId) noexcept {
    tLogShiftChainHelper result{aTaskId};
    if(result.isValid()) {
      if constexpr(csTaskRepresentation == TaskRepresentation::cId) {
        result << sConfig->taskIdFormat << aTaskId;
      }
      else if constexpr (csTaskRepresentation == TaskRepresentation::cName) {
        if constexpr (csConstantTaskNames || !csSendInBackground) {
          result << tAppInterface::getTaskName(aTaskId);
        }
        else {
          result << LogFormatConfig::St << tAppInterface::getTaskName(aTaskId);
        }
      }
      else { // nothing to do
      }
      if (sConfig->tickFormat.isValid()) {
        result << sConfig->tickFormat << tAppInterface::getLogTime();
      }
      else { // nothing to do
      }
    }
    else { // nothing to do
    }
    return result;
  }

  template <typename tLogShiftChainHelper>
  static tLogShiftChainHelper sendHeader(TaskId const aTaskId, char const * aTopicName) noexcept {
    tLogShiftChainHelper result = sendHeader<tLogShiftChainHelper>(aTaskId);
    if(result.isValid() && aTopicName != nullptr) {
      result << aTopicName;
    }
    else { // nothing to do
    }
    return result;
  }

  static void transmitterTaskFunction() noexcept {
    while(sKeepAliveTask || !tQueue::empty()) {
      tMessage message;
      if(tQueue::pop(message, csRefreshPeriod)) {
        TaskId taskId = message.getTaskId();
        if (message.isShutdown()) {
          (*sTaskShutdowns)[taskId] = true;
        }
        else {
          checkAndInsertAndTransmit(taskId, message);
        }
      }
      else {
        if constexpr(csAtomicBufferOperational) {
          if(tAtomicBuffer::isScheduledForSent()) {
            doSendAtomicBuffer();
            // Clear the request before waking the caller, otherwise its next request could be cleared here.
            tAtomicBuffer::sendFinished();
            tAppInterface::atomicBufferSendFinished();
          }
          else { // nothing to do
          }
        }
        else { // nothing to do
        }
      }
    }
    tAppInterface::finish();
  }

  static void checkAndInsertAndTransmit(TaskId const aTaskId, tMessage const &aMessage) noexcept {
    auto list = (*sMessageQueues)[aTaskId];
    bool ready = false;
    auto sequence = aMessage.getMessageSequence();
    if(list->empty()) {
      if(sequence <= csSequence1 && sAllocator->hasFree()) {
        ready = push(*list, aMessage, sequence);
      }
      else { // nothing to do
      }
    }
    else {
      auto lastSequence = list->back().getMessageSequence();
      if((sequence == csSequence0 || sequence == lastSequence + 1u) && sAllocator->hasFree()) {
        ready = push(*list, aMessage, sequence);
      }
      else {
        list->clear();
      }
    }
    if (ready) {
      transmit(*list);
    }
    else { // nothing to do
    }
  }

  static bool push(MessageQueue &aList, tMessage const &aMessage, MessageSequence const aSequence) noexcept {
    bool result;
    if(aSequence == csSequence0) {
      aList.push_front(aMessage);
      result = true;
    }
    else {
      aList.push_back(aMessage);
      result = false;
    }
    return result;
  }

  static void transmit(MessageQueue &aList) noexcept {
    auto [begin, end] = tSender::getBuffer();
    tConverter converter(begin, end);
    for(auto &message : aList) {
      message.template output<tConverter>(converter);
    }
    aList.clear();
    converter.terminateSequence();
    tSender::send(begin, converter.end());
  }

  static void doSendAtomicBuffer() noexcept {
    auto [inBuffer, inIndex] = tAtomicBuffer::getBuffer();
    auto [outBegin, outEnd] = tSender::getBuffer();
    size_t processed = 0u;
    while(processed < csAtomicBufferSize) {
      tConverter converter(outBegin, outEnd);
      auto validOutEnd = converter.end();
      while((converter.end() != outEnd) && (processed < csAtomicBufferSize)) {
        if(inBuffer[inIndex] != csAtomicBufferInvalidValue) {
          converter.convert(inBuffer[inIndex], sConfig->atomicFormat.mBase, sConfig->atomicFormat.mFill);
          if (converter.end() != outEnd) {
            validOutEnd = converter.end();
          } else { // nothing to do
          }
        }
        else { // nothing to do
        }
        inIndex = (inIndex + 1u) % csAtomicBufferSize;
        ++processed;
      }
      tSender::send(outBegin, validOutEnd);
    }
    tConverter converter(outBegin, outEnd);
    converter.terminateSequence();
    tSender::send(outBegin, converter.end());
  }
};

}

using LC = nowtech::log::LogFormatConfig;

#endif
