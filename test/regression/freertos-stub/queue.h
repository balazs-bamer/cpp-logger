#ifndef NOWTECH_LOG_TEST_STUB_QUEUE
#define NOWTECH_LOG_TEST_STUB_QUEUE

#include "FreeRTOS.h"

struct QueueDefinition;
using QueueHandle_t = QueueDefinition*;

QueueHandle_t xQueueCreate(UBaseType_t aLength, UBaseType_t aItemSize);
BaseType_t xQueueIsQueueEmptyFromISR(QueueHandle_t aQueue);
BaseType_t xQueueSendFromISR(QueueHandle_t aQueue, void const *aItem, BaseType_t *aHigherPriorityTaskWoken);
BaseType_t xQueueReceive(QueueHandle_t aQueue, void *aBuffer, TickType_t aTicksToWait);

#endif
