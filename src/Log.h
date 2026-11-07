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
#include <utility>

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
  static_assert(noexcept(tSender::send(static_cast<char const *>(nullptr), static_cast<char const *>(nullptr))),
                "Senders must not throw, send() runs in noexcept code like the transmitter. Report errors to tAppInterface::error().");

  inline static constexpr char csRegisteredTask[]    = "-=- Registered task:";
  inline static constexpr char csUnregisteredTask[]  = "-=- Unregistered task:";
  inline static constexpr char csNullString[]        = "(null)";   // Logged instead of a nullptr string.
  
  inline static LogFormatConfig const                 *sConfig;
  inline static std::atomic<LogTopic>                  sNextFreeTopic;
  inline static std::atomic<bool>                      sKeepAliveTask;
  inline static std::array<TopicName, csMaxTopicCount> sRegisteredTopics;
  inline static TaskShutdownArray                     *sTaskShutdowns;

  inline static Occupier           sOccupier;
  inline static Allocator         *sAllocator;
  inline static MessageQueueArray *sMessageQueues;

  Log() = delete;

  template<typename tType>
  struct TypeTag final {
    using Type = tType;
  };

  template<size_t tSize, bool tSigned>
  using FixedWidthInteger = std::conditional_t<tSize == 1u, std::conditional_t<tSigned, int8_t, uint8_t>,
                            std::conditional_t<tSize == 2u, std::conditional_t<tSigned, int16_t, uint16_t>,
                            std::conditional_t<tSize == 4u, std::conditional_t<tSigned, int32_t, uint32_t>,
                                                            std::conditional_t<tSigned, int64_t, uint64_t>>>>;

  template<typename tValue>
  static constexpr bool isWideCharacter() noexcept {
    bool result = std::is_same_v<tValue, wchar_t> || std::is_same_v<tValue, char16_t> || std::is_same_v<tValue, char32_t>;
#ifdef __cpp_char8_t
    result = result || std::is_same_v<tValue, char8_t>;
#endif
    return result;
  }

  /// Integers map to the fixed width type of their size and signedness, so the converter and the messages
  /// see int32_t whether it is int or long, and enums map to their underlying type. bool, char and the
  /// floating point types stay, everything else maps to void and can't be logged. Strings have their own
  /// operator<< overloads.
  template<typename tValue>
  static constexpr auto loggedTypeTag() noexcept {
    if constexpr(std::is_enum_v<tValue>) {
      return loggedTypeTag<std::underlying_type_t<tValue>>();
    }
    else if constexpr(std::is_same_v<tValue, bool> || std::is_same_v<tValue, char>) {
      return TypeTag<tValue>{};
    }
    else if constexpr(isWideCharacter<tValue>()) {   // The converter only knows char.
      return TypeTag<void>{};
    }
    else if constexpr(std::is_integral_v<tValue> && sizeof(tValue) <= sizeof(int64_t)) {
      return TypeTag<FixedWidthInteger<sizeof(tValue), std::is_signed_v<tValue>>>{};
    }
    else if constexpr(std::is_floating_point_v<tValue> && tMessage::csSupportFloatingPoint) {
      return TypeTag<tValue>{};
    }
    else {
      return TypeTag<void>{};
    }
  }

  template<typename tValue>
  using LoggedType = typename decltype(loggedTypeTag<std::remove_cv_t<tValue>>())::Type;

  template<typename tValue>
  static constexpr bool isLoggable() noexcept {
    if constexpr(std::is_void_v<LoggedType<tValue>>) {
      return false;
    }
    else if constexpr(csSendInBackground) {
      return tMessage::template csCanStore<LoggedType<tValue>>;
    }
    else {
      return true;
    }
  }

  /// Compile time only. Rejects what can't be logged the same way in every mode, also when logging is
  /// compiled out, so code that builds with one configuration builds with the others. Callers skip the
  /// rest of their work when isLoggable() is false, so this is the only error reported.
  template<typename tValue>
  static constexpr void checkLoggable() noexcept {
    static_assert(!std::is_floating_point_v<tValue> || tMessage::csSupportFloatingPoint,
                  "Floating point logging is turned off, see the tSupportFloatingPoint parameter of the message.");
    static_assert(std::is_floating_point_v<tValue> || !std::is_void_v<LoggedType<tValue>>,
                  "This type can't be logged. Log bool, char, integers, enums, floating point numbers, char const * or std::string, and cast pointers to uintptr_t.");
    static_assert(std::is_void_v<LoggedType<tValue>> || isLoggable<tValue>(),
                  "This type doesn't fit the message payload, increase its size.");
  }

  /// This will be used to send via queue. It stores the first message, and sends it only when the group ends:
  /// at Log::end, or when the helper is destroyed. A helper with a valid task ID owns its group. It is
  /// move-only, and moving hands the group over.
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

    LogShiftChainHelperBackgroundSend(LogShiftChainHelperBackgroundSend const &) = delete;

    LogShiftChainHelperBackgroundSend(LogShiftChainHelperBackgroundSend &&aOther) noexcept
     : mTaskId(aOther.mTaskId)
     , mNextFormat(aOther.mNextFormat)
     , mNextSequence(aOther.mNextSequence)
     , mFirstMessage(aOther.mFirstMessage)
     , mWasMessage(aOther.mWasMessage) {
      aOther.mTaskId = csInvalidTaskId;
    }

    LogShiftChainHelperBackgroundSend& operator=(LogShiftChainHelperBackgroundSend const &) = delete;
    LogShiftChainHelperBackgroundSend& operator=(LogShiftChainHelperBackgroundSend &&) = delete;

    ~LogShiftChainHelperBackgroundSend() noexcept {
      finish();
    }

    /// Can be used in application code to eliminate further operator<< calls when the topic is disabled.
    bool isValid() const noexcept {
      return mTaskId != csInvalidTaskId;
    }

    template<typename tValue>
    LogShiftChainHelperBackgroundSend& operator<<(tValue const aValue) & noexcept {
      checkLoggable<tValue>();
      if constexpr(isLoggable<tValue>()) {
        if(mTaskId != csInvalidTaskId && mNextSequence < std::numeric_limits<MessageSequence>::max()) {
          LogFormat format = obtainFormat();
          tMessage message;
          message.set(static_cast<LoggedType<tValue>>(aValue), format, mTaskId, mNextSequence);
          sendOrStore(message);
        }
        else { // silently discard value, nothing to do
        }
      }
      else { // checkLoggable reported it
      }
      return *this;
    }

    LogShiftChainHelperBackgroundSend& operator<<(char * const aValue) & noexcept {
      return sendCharPointer(aValue);
    }

    LogShiftChainHelperBackgroundSend& operator<<(char const * const aValue) & noexcept {
      return sendCharPointer(aValue);
    }

    LogShiftChainHelperBackgroundSend& operator<<(std::string const &aValue) & noexcept {
      mNextFormat = LogFormatConfig::St;
      return sendCharPointer(aValue.c_str());
    }

    LogShiftChainHelperBackgroundSend& operator<<(LogFormat const aFormat) & noexcept {
      mNextFormat = aFormat;
      return *this;
    }

    /// A chain on a temporary, like Log::n() << a << b, keeps it an rvalue, so auto logger = Log::n() << a;
    /// moves it into logger. Never bind a chain result to auto&&, the temporary dies at the end of the statement.
    template<typename tValue>
    LogShiftChainHelperBackgroundSend&& operator<<(tValue &&aValue) && noexcept {
      return std::move(*this << std::forward<tValue>(aValue));
    }

    void operator<<(LogShiftChainEndMarker const) & noexcept {
      finish();
    }

    /// On a temporary the destructor ends the group at the end of the same statement, ending it here too would
    /// cost a second call per statement.
    void operator<<(LogShiftChainEndMarker const) && noexcept {
    }

  private:
    void finish() noexcept {
      if(mTaskId != csInvalidTaskId) {
        if(!mWasMessage) {
          mFirstMessage.set(csEmptyString, csEmptyFormat, mTaskId, csSequence0);
        }
        else { // nothing to do
        }
        tQueue::push(mFirstMessage);
        mTaskId = csInvalidTaskId;   // Ended, the destructor has nothing left to do.
      }
      else { // nothing to do
      }
    }

    LogShiftChainHelperBackgroundSend& sendCharPointer(char const * aValue) noexcept {
      if(mTaskId != csInvalidTaskId && mNextSequence < std::numeric_limits<MessageSequence>::max()) {
        aValue = (aValue != nullptr) ? aValue : csNullString;
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
  /// The group ends at Log::end, or when the helper is destroyed. A helper with a valid task ID owns its
  /// group. It is move-only, and moving hands the group over.
  class LogShiftChainHelperDirectSend final {
    TaskId          mTaskId;
    LogFormat       mNextFormat;
    bool            mLocked;   // tAppInterface::lock() is held from the first item to the end, so lines from tasks don't mix.

  public:
    LogShiftChainHelperDirectSend() noexcept = delete;

    LogShiftChainHelperDirectSend(TaskId const aTaskId) noexcept
     : mTaskId(aTaskId)
     , mLocked(false) {
       mNextFormat.invalidate();
    }

    LogShiftChainHelperDirectSend(LogShiftChainHelperDirectSend const &) = delete;

    LogShiftChainHelperDirectSend(LogShiftChainHelperDirectSend &&aOther) noexcept
     : mTaskId(aOther.mTaskId)
     , mNextFormat(aOther.mNextFormat)
     , mLocked(aOther.mLocked) {
      aOther.mTaskId = csInvalidTaskId;
      aOther.mLocked = false;
    }

    LogShiftChainHelperDirectSend& operator=(LogShiftChainHelperDirectSend const &) = delete;
    LogShiftChainHelperDirectSend& operator=(LogShiftChainHelperDirectSend &&) = delete;

    ~LogShiftChainHelperDirectSend() noexcept {
      finish();
    }

    /// Can be used in application code to eliminate further operator<< calls when the topic is disabled.
    bool isValid() const noexcept {
      return mTaskId != csInvalidTaskId;
    }

    template<typename tValue>
    LogShiftChainHelperDirectSend& operator<<(tValue const aValue) & noexcept {
      checkLoggable<tValue>();
      if constexpr(isLoggable<tValue>()) {
        return convertAndSend(static_cast<LoggedType<tValue>>(aValue));
      }
      else {   // checkLoggable reported it
        return *this;
      }
    }

    LogShiftChainHelperDirectSend& operator<<(char * const aValue) & noexcept {
      return operator<<(static_cast<char const *>(aValue));
    }

    LogShiftChainHelperDirectSend& operator<<(char const * const aValue) & noexcept {
      return convertAndSend((aValue != nullptr) ? aValue : csNullString);
    }

    LogShiftChainHelperDirectSend& operator<<(std::string const &aValue) & noexcept {   // Converted right away, no need to copy.
      return operator<<(aValue.c_str());
    }

    LogShiftChainHelperDirectSend& operator<<(LogFormat const aFormat) & noexcept {
      mNextFormat = aFormat;
      return *this;
    }

    /// A chain on a temporary, like Log::n() << a << b, keeps it an rvalue, so auto logger = Log::n() << a;
    /// moves it into logger. Never bind a chain result to auto&&, the temporary dies at the end of the statement.
    template<typename tValue>
    LogShiftChainHelperDirectSend&& operator<<(tValue &&aValue) && noexcept {
      return std::move(*this << std::forward<tValue>(aValue));
    }

    void operator<<(LogShiftChainEndMarker const) & noexcept {
      finish();
    }

    /// On a temporary the destructor ends the group at the end of the same statement, ending it here too would
    /// cost a second call per statement.
    void operator<<(LogShiftChainEndMarker const) && noexcept {
    }

  private:
    void finish() noexcept {
      if(mTaskId != csInvalidTaskId) {
        ConversionResult buffer[csDirectBufferSize];
        tConverter converter(buffer, buffer + csDirectBufferSize);
        converter.terminateSequence();
        lockOnce();
        tSender::send(buffer, converter.end());
        tAppInterface::unlock();
        mLocked = false;
        mTaskId = csInvalidTaskId;   // Ended, the destructor has nothing left to do.
      }
      else { // nothing to do
      }
    }

    void lockOnce() noexcept {
      if(!mLocked) {
        tAppInterface::lock();
        mLocked = true;
      }
      else { // nothing to do
      }
    }

    template<typename tValue>
    LogShiftChainHelperDirectSend& convertAndSend(tValue const aValue) noexcept {
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
        lockOnce();
        tSender::send(buffer, converter.end());
      }
      else { // silently discard value, nothing to do
      }
      return *this;
    }
  }; // class LogShiftChainHelperDirectSend

  /// This shuts down all logging code generation without uncommenting anything from user code, when compiled with aT least -O1 
  class LogShiftChainHelperEmpty final {
  public:
    LogShiftChainHelperEmpty() noexcept = delete;

    LogShiftChainHelperEmpty(TaskId const) noexcept {
    }

    // Move-only like the others, so code compiles the same with logging compiled out. Trivial destructor.
    LogShiftChainHelperEmpty(LogShiftChainHelperEmpty const &) = delete;
    LogShiftChainHelperEmpty(LogShiftChainHelperEmpty &&) noexcept = default;
    LogShiftChainHelperEmpty& operator=(LogShiftChainHelperEmpty const &) = delete;
    LogShiftChainHelperEmpty& operator=(LogShiftChainHelperEmpty &&) = delete;
    ~LogShiftChainHelperEmpty() noexcept = default;

    /// Can be used in application code to eliminate further operator<< calls when the topic is disabled.
    bool isValid() const noexcept {
      return false;
    }

    template<typename tValue>
    LogShiftChainHelperEmpty& operator<<(tValue const) & noexcept {
      checkLoggable<tValue>();
      return *this;
    }

    LogShiftChainHelperEmpty& operator<<(char * const) & noexcept {
      return *this;
    }

    LogShiftChainHelperEmpty& operator<<(char const * const) & noexcept {
      return *this;
    }

    LogShiftChainHelperEmpty& operator<<(std::string const &) & noexcept {
      return *this;
    }

    LogShiftChainHelperEmpty& operator<<(LogFormat const) & noexcept {
      return *this;
    }

    template<typename tValue>
    LogShiftChainHelperEmpty&& operator<<(tValue &&aValue) && noexcept {
      return std::move(*this << std::forward<tValue>(aValue));
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
  /// Not noexcept: tAppInterface::fatalError may throw when running out of task IDs.
  static void registerCurrentTask() {
    if constexpr(!csShutdownLog) {
      registerCurrentTask(nullptr);
    }
    else { // nothing to do
    }
  }

/// Registers the current task if not already present. It can register
  /// at most 254 tasks. All others will be handled as one.
  /// @param aTaskName Task name to use, when the osInterface supports it. May be nullptr.
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
          // Log the name the logger will use, aTaskName may be nullptr or ignored by tAppInterface.
          auto logger = n(taskId);
          logger << csRegisteredTask;
          appendTaskName(logger, taskId);
          logger << taskId << end;
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
      if(taskId != csInvalidTaskId) {   // Not registered: nothing to do, and taskId would index out of bounds.
        if constexpr(csAllowRegistrationLog) {
          n(taskId) << csUnregisteredTask << taskId << end;
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
        tAppInterface::lock();   // Several sends, keep log lines out of them.
        doSendAtomicBuffer();
        tAppInterface::unlock();
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
  static void appendTaskName(tLogShiftChainHelper &aLogger, TaskId const aTaskId) noexcept {
    if constexpr (csConstantTaskNames || !csSendInBackground) {
      aLogger << tAppInterface::getTaskName(aTaskId);
    }
    else {  // The name may change or go away before the transmitter converts it, so copy it.
      aLogger << LogFormatConfig::St << tAppInterface::getTaskName(aTaskId);
    }
  }

  template <typename tLogShiftChainHelper>
  static tLogShiftChainHelper sendHeader(TaskId const aTaskId) noexcept {
    tLogShiftChainHelper result{aTaskId};
    if(result.isValid()) {
      if constexpr(csTaskRepresentation == TaskRepresentation::cId) {
        result << sConfig->taskIdFormat << aTaskId;
      }
      else if constexpr (csTaskRepresentation == TaskRepresentation::cName) {
        appendTaskName(result, aTaskId);
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
