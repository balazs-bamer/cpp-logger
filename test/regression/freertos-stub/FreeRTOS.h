#ifndef NOWTECH_LOG_TEST_STUB_FREERTOS
#define NOWTECH_LOG_TEST_STUB_FREERTOS

// Declaration-only stand-ins for the FreeRTOS, CMSIS and STM32 HAL headers, just enough to
// compile the FreeRTOS parts of cpp-logger on a host. Not a FreeRTOS port: a test that runs
// code using them must define the functions it calls.

#include <cstddef>
#include <cstdint>

using TickType_t = uint32_t;
using BaseType_t = long;
using UBaseType_t = unsigned long;

#define portMAX_DELAY ((TickType_t)0xffffffffUL)
#define portTICK_PERIOD_MS ((TickType_t)1)
#define pdTRUE ((BaseType_t)1)
#define pdFALSE ((BaseType_t)0)

void* pvPortMalloc(size_t aSize);
void vPortFree(void* aPointer);

#endif
