/*
 * Copyright 2018 Now Technologies Zrt.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "LogAppInterfaceFreeRtosMinimal.h"
#include "LogConverterCustomText.h"
#include "LogSenderStmHalMinimal.h"
#include "LogSenderVoid.h"
#include "LogQueueFreeRtos.h"
#include "LogQueueVoid.h"
#include "LogMessageVariant.h"
#include "LogMessageCompact.h"
#include "Log.h"
#include "main.h"

extern UART_HandleTypeDef huart1;

namespace nowtech::LogTopics {
  nowtech::log::TopicInstance system;
  nowtech::log::TopicInstance surplus;
}

constexpr nowtech::log::TaskId cgMaxTaskCount = 1u;
constexpr bool cgAllowRegistrationLog = true;
constexpr bool cgLogFromIsr = false;
constexpr size_t cgTaskShutdownSleepPeriod = 100u;
constexpr bool cgArchitecture64 = false;
constexpr uint8_t cgAppendStackBufferSize = 100u;
constexpr bool cgAppendBasePrefix = true;
constexpr bool cgAlignSigned = false;
constexpr size_t cgTransmitBufferSize = 123u;
constexpr size_t cgPayloadSize = 8u;
constexpr bool cgSupportFloatingPoint = true;
constexpr size_t cgQueueSize = 111u;
constexpr nowtech::log::LogTopic cgMaxTopicCount = 2;
constexpr nowtech::log::TaskRepresentation cgTaskRepresentation = nowtech::log::TaskRepresentation::cName;
constexpr size_t cgDirectBufferSize = 0u;
constexpr nowtech::log::ErrorLevel cgErrorLevel = nowtech::log::ErrorLevel::Error;
constexpr uint32_t cgLogTaskStackSize = 256u;
constexpr uint32_t cgLogTaskPriority = tskIDLE_PRIORITY + 1u;

using LogAppInterfaceFreeRtosMinimal = nowtech::log::AppInterfaceFreeRtosMinimal<cgMaxTaskCount, cgLogFromIsr, cgTaskShutdownSleepPeriod>;
constexpr typename LogAppInterfaceFreeRtosMinimal::LogTime cgTimeout = 123u;
constexpr typename LogAppInterfaceFreeRtosMinimal::LogTime cgRefreshPeriod = 444;
using LogMessage = nowtech::log::MessageVariant<cgPayloadSize, cgSupportFloatingPoint>;
using LogConverterCustomText = nowtech::log::ConverterCustomText<LogMessage, cgArchitecture64, cgAppendStackBufferSize, cgAppendBasePrefix, cgAlignSigned>;
using LogSenderStmHalMinimal = nowtech::log::SenderStmHalMinimal<LogAppInterfaceFreeRtosMinimal, LogConverterCustomText, cgTransmitBufferSize, cgTimeout>;
using LogQueueFreeRtos = nowtech::log::QueueFreeRtos<LogMessage, LogAppInterfaceFreeRtosMinimal, cgQueueSize>;
using LogAtomicBuffer = nowtech::log::AtomicBufferVoid;
using LogConfig = nowtech::log::Config<cgAllowRegistrationLog, cgMaxTopicCount, cgTaskRepresentation, cgDirectBufferSize, cgRefreshPeriod, cgErrorLevel>;
using Log = nowtech::log::Log<LogQueueFreeRtos, LogSenderStmHalMinimal, LogAtomicBuffer, LogConfig>;
 
void step() {
  HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
  vTaskDelay(133);
}

extern "C" void myDefaultTask() {
  nowtech::log::LogFormatConfig logConfig;
  LogSenderStmHalMinimal::init(&huart1);
  Log::init(logConfig, cgLogTaskStackSize, cgLogTaskPriority);

  Log::registerTopic(nowtech::LogTopics::system, "system");
  Log::registerTopic(nowtech::LogTopics::surplus, "surplus");
  Log::registerCurrentTask("main");

  uint64_t const uint64 = 123456789012345;
  int64_t const int64 = -123456789012345;

  Log::i(nowtech::LogTopics::surplus) << "message" << Log::end;
  step();
  Log::i(nowtech::LogTopics::system) << "uint64: " << uint64 << " int64: " << int64 << Log::end;
  step();
  Log::n(nowtech::LogTopics::system) << "uint64: " << uint64 << " int64: " << int64 << Log::end;
  step();
  Log::i() << "uint64: " << uint64 << " int64: " << int64 << Log::end;
  step();
  Log::n() << "uint64: " << uint64 << " int64: " << int64 << Log::end;
  step();
  uint8_t const uint8 = 42;
  int8_t const int8 = -42;

  Log::i(nowtech::LogTopics::system) << uint8 << ' ' << int8 << Log::end;
  step();
  Log::i(nowtech::LogTopics::system) << LC::X2 << uint8 << ' ' << LC::D3 << int8 << Log::end;
  step();
  Log::i() << uint8 << ' ' << int8 << Log::end;
  step();
  Log::i() << LC::X2 << uint8 << int8 << Log::end;
  step();
  Log::i() << Log::end;
  step();
  Log::i() << "int8: " << static_cast<int8_t>(123) << Log::end;
  step();
  Log::i() << "int16: " << static_cast<int16_t>(123) << Log::end;
  step();
  Log::i() << "int32: " << static_cast<int32_t>(123) << Log::end;
  step();
  Log::i() << "int64: " << static_cast<int64_t>(123) << Log::end;
  step();
  Log::i() << "uint8: " << static_cast<uint8_t>(123) << Log::end;
  step();
  Log::i() << "uint16: " << static_cast<uint16_t>(123) << Log::end;
  step();
  Log::i() << "uint32: " << static_cast<uint32_t>(123) << Log::end;
  step();
  Log::i() << "uint64: " << static_cast<uint64_t>(123) << Log::end;
  step();
  Log::i() << "float: " << 1.234567890f << Log::end;
  step();
  Log::i() << "double: " << -1.234567890 << Log::end;
  step();
  Log::i() << "float: " << LC::Fm << -123.4567890f << Log::end;
  step();
  Log::i() << "double: " << LC::Fm << 123.4567890 << Log::end;
  step();
  Log::i() << "long double: " << -0.01234567890L << Log::end;
  step();
  Log::i() << "long double: " << LC::D16 << 0.01234567890L << Log::end;
  step();
  Log::i() << "bool:" << true << Log::end;
  step();
  Log::i() << "bool:" << false << Log::end;
  step();

  while(true) {
    Log::n() << "end" << Log::end;
    step();
  }
}

extern "C" void vApplicationMallocFailedHook(void) {
  while(true) {
    step();
  }
}
