#ifndef NOWTECH_LOG_SENDER_ROS2
#define NOWTECH_LOG_SENDER_ROS2

#include "rcutils/logging.h"
#include "Log.h"
#include <string>
#include <algorithm>

namespace nowtech::log {

template<typename tAppInterface, typename tConverter, size_t tTransmitBufferSize, typename tAppInterface::LogTime tTimeout, int tSimulatedRos2Loglevel>
class SenderRos2 final {
public:
  using tAppInterface_   = tAppInterface;
  using tConverter_      = tConverter;
  using ConversionResult = typename tConverter::ConversionResult;
  using Iterator         = typename tConverter::Iterator;

  static constexpr bool csVoid = false;

private:
  inline static ConversionResult                 *sTransmitBuffer;
  inline static Iterator                         sBegin;
  inline static Iterator                         sEnd;
  inline static constexpr char                   csDummyName[]         = "";
  inline static constexpr char                   csDummyFunctionName[] = "";
  inline static constexpr char                   csDummyFilename[]     = "";
  inline static constexpr char                   csFormat[]            = "%s";
  inline static constexpr rcutils_log_location_t csDummyLocation       = { csDummyFunctionName, csDummyFilename, tSimulatedRos2Loglevel };
  static constexpr char                          csNewline             = '\n';

  SenderRos2() = delete;

public:
  static void init() {
    sTransmitBuffer = tAppInterface::template _newArray<ConversionResult>(tTransmitBufferSize);
    sBegin = sTransmitBuffer;
    sEnd = sTransmitBuffer + tTransmitBufferSize;
  }

  static void done() noexcept {
    tAppInterface::template _deleteArray<ConversionResult>(sTransmitBuffer);
  }

  static void send(char const * const aBegin, char const * const aEnd) noexcept {
    try {
      std::string buffer{aBegin, static_cast<size_t>(aEnd - aBegin)};
      if(aBegin < aEnd && aEnd[-1] == csNewline) {
        buffer.pop_back();
      }
      else { // nothing to do
      }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
      rcutils_log(&csDummyLocation, tSimulatedRos2Loglevel, csDummyName, csFormat, buffer.c_str());
#pragma GCC diagnostic pop
    } catch(...) {
      tAppInterface::error(Exception::cSenderError);
    }
  }

  static auto getBuffer() {
    return std::pair(sBegin, sEnd);
  }
};
  
}

#endif
