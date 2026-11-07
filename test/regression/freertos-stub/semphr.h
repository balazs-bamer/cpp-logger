#ifndef NOWTECH_LOG_TEST_STUB_SEMPHR
#define NOWTECH_LOG_TEST_STUB_SEMPHR

#include "queue.h"

using SemaphoreHandle_t = QueueHandle_t;

SemaphoreHandle_t xSemaphoreCreateMutex();
BaseType_t xSemaphoreTake(SemaphoreHandle_t aSemaphore, TickType_t aTicksToWait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t aSemaphore);

#endif
