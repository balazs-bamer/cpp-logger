// Compile-only check: Log on AppInterfaceFreeRtosMinimal with the atomic buffer, against the
// freertos-stub headers. Keeps the FreeRTOS side in step with changes to Log and the app
// interface API, such as the atomic buffer send hooks. LOG_TEST_BACKGROUND selects background
// mode with QueueFreeRtos, otherwise direct mode.

#include "LogAppInterfaceFreeRtosMinimal.h"
#include "LogConverterCustomText.h"
#include "LogSenderStmHalMinimal.h"
#include "LogMessageCompact.h"
#ifdef LOG_TEST_BACKGROUND
#include "LogQueueFreeRtos.h"
#else
#include "LogQueueVoid.h"
#endif
#include "Log.h"

using LogAppInterface = nowtech::log::AppInterfaceFreeRtosMinimal<4u, false, 100u>;
using LogMessage = nowtech::log::MessageCompact<8u, false>;
using LogConverter = nowtech::log::ConverterCustomText<LogMessage, false, 100u, true, false>;
using LogSender = nowtech::log::SenderStmHalMinimal<LogAppInterface, LogConverter, 123u, 123u>;
using LogAtomicBuffer = nowtech::log::AtomicBufferOperational<LogAppInterface, int32_t, 4u, 1234546789>;
#ifdef LOG_TEST_BACKGROUND
using LogQueue = nowtech::log::QueueFreeRtos<LogMessage, LogAppInterface, 64u>;
constexpr size_t cgDirectBufferSize = 0u;
#else
using LogQueue = nowtech::log::QueueVoid<LogMessage, LogAppInterface>;
constexpr size_t cgDirectBufferSize = 43u;
#endif
using LogConfig = nowtech::log::Config<true, 2, nowtech::log::TaskRepresentation::cName, cgDirectBufferSize, 10u, nowtech::log::ErrorLevel::Error>;
using Log = nowtech::log::Log<LogQueue, LogSender, LogAtomicBuffer, LogConfig>;

extern UART_HandleTypeDef gUart;

void useLog() {
  nowtech::log::LogFormatConfig config;
  LogSender::init(&gUart);
#ifdef LOG_TEST_BACKGROUND
  Log::init(config, 256u, 1u);
#else
  Log::init(config);
#endif
  Log::registerCurrentTask("task");
  Log::i() << "value" << 42 << Log::end;
  Log::pushAtomic(1);
  Log::sendAtomicBuffer();
  Log::unregisterCurrentTask();
  Log::done();
}
