#ifndef NOWTECH_LOG_TEST_STUB_MAIN
#define NOWTECH_LOG_TEST_STUB_MAIN

// Stands in for the STM32CubeMX generated main.h that test/test-freertosminimal.cpp includes.

#include "stm32f1xx_hal.h"

extern GPIO_TypeDef *LED_GPIO_Port;
#define LED_Pin ((uint16_t)0x0020)

#endif
