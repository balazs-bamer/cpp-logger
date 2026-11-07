#ifndef NOWTECH_LOG_TEST_STUB_TASK
#define NOWTECH_LOG_TEST_STUB_TASK

#include "FreeRTOS.h"

#define tskIDLE_PRIORITY ((UBaseType_t)0U)

struct tskTaskControlBlock;
using TaskHandle_t = tskTaskControlBlock*;
using TaskFunction_t = void (*)(void*);

BaseType_t xTaskCreate(TaskFunction_t aFunction, char const *aName, uint32_t aStackDepth, void *aParameters, UBaseType_t aPriority, TaskHandle_t *aCreatedTask);
TaskHandle_t xTaskGetCurrentTaskHandle();
char* pcTaskGetName(TaskHandle_t aTask);
TickType_t xTaskGetTickCount();
void vTaskDelay(TickType_t aTicks);
uint32_t ulTaskNotifyTake(BaseType_t aClearCountOnExit, TickType_t aTicksToWait);
BaseType_t xTaskNotifyGive(TaskHandle_t aTask);

#endif
