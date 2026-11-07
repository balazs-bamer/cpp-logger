// REVIEW.md bug 3: LogQueueStdCircular.h and LogQueueStdBoost.h shared a header guard, so
// whichever was included second was silently skipped. All headers that build on a host go
// into this one translation unit, and each one's main type is used, so a skipped header is
// a compile error. The FreeRTOS headers are covered by the freertos-* targets.

#include "Log.h"
#include "LogAppInterfaceStd.h"
#include "LogAtomicBuffers.h"
#include "LogConverterCustomText.h"
#include "LogMessageBase.h"
#include "LogMessageCompact.h"
#include "LogMessageVariant.h"
#include "LogNumericSystem.h"
#include "LogQueueStdBoost.h"
#include "LogQueueStdCircular.h"
#include "LogQueueVoid.h"
#include "LogSenderStdOstream.h"
#include "LogSenderVoid.h"

using AppInterface = nowtech::log::AppInterfaceStd<1u, false, 10u>;
using MessageCompact = nowtech::log::MessageCompact<8u, true>;
using MessageVariant = nowtech::log::MessageVariant<8u, true>;
using Converter = nowtech::log::ConverterCustomText<MessageCompact, true, 100u, true, false>;

static_assert(MessageCompact::csSupportFloatingPoint && MessageVariant::csSupportFloatingPoint);
static_assert(nowtech::log::QueueStdBoost<MessageCompact, AppInterface, 8u>::csQueueSize == 8u);
static_assert(nowtech::log::QueueStdCircular<MessageCompact, AppInterface, 8u>::csQueueSize == 8u);
static_assert(nowtech::log::QueueVoid<MessageCompact, AppInterface>::csQueueSize == 0u);
static_assert(!nowtech::log::SenderStdOstream<AppInterface, Converter, 8u, 1u>::csVoid);
static_assert(nowtech::log::SenderVoid<AppInterface, Converter, 8u, 1u>::csVoid);
static_assert(nowtech::log::AtomicBufferOperational<AppInterface, int32_t, 3u, 0>::csAtomicBufferSize == 8u);
static_assert(nowtech::log::NumericSystem::csBaseMax == 16u);
static_assert(sizeof(nowtech::log::LogFormat) == 2u);

int main() {
  return 0;
}
