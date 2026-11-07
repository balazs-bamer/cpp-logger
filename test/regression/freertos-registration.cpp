// AppInterfaceFreeRtosMinimal::registerCurrentTask checked for a full task table before looking
// for the calling task, so once the table was full, a registered task registering again got
// cOutOfTaskIdsOrDoubleRegistration instead of its own ID. Runs on the host against the
// freertos-stub headers, with the few FreeRTOS calls registration makes faked below.

#include "TestCheck.h"
#include "LogAppInterfaceFreeRtosMinimal.h"

struct tskTaskControlBlock final {
  int mDummy;
};

namespace {
  tskTaskControlBlock gTasks[3];
  TaskHandle_t gCurrentTask = nullptr;
  SCB_Type gScb{};   // ICSR == 0: never in an interrupt.
}

SCB_Type *SCB = &gScb;

TaskHandle_t xTaskGetCurrentTaskHandle() {
  return gCurrentTask;
}

SemaphoreHandle_t xSemaphoreCreateMutex() {
  return nullptr;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) {   // Single threaded test, no locking needed.
  return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t) {
  return pdTRUE;
}

using AppInterface = nowtech::log::AppInterfaceFreeRtosMinimal<2u, false, 10u>;

int main() {
  AppInterface::init();
  gCurrentTask = &gTasks[0];
  auto const first = AppInterface::registerCurrentTask(nullptr);
  gCurrentTask = &gTasks[1];
  auto const second = AppInterface::registerCurrentTask(nullptr);
  CHECK(first != AppInterface::csInvalidTaskId);
  CHECK(second != AppInterface::csInvalidTaskId);
  CHECK(first != second);

  // The table is full now.
  gCurrentTask = &gTasks[0];
  CHECK(AppInterface::registerCurrentTask(nullptr) == first);
  CHECK(AppInterface::getCurrentTaskId() == first);
  gCurrentTask = &gTasks[2];
  CHECK(AppInterface::registerCurrentTask(nullptr) == AppInterface::csInvalidTaskId);
  return finish();
}
