#ifndef NOWTECH_LOG_TEST_STUB_STM32F1XX_HAL
#define NOWTECH_LOG_TEST_STUB_STM32F1XX_HAL

#include <cstdint>

struct SCB_Type {
  volatile uint32_t ICSR;
};
extern SCB_Type *SCB;   // CMSIS defines it as a pointer to fixed address.
#define SCB_ICSR_VECTACTIVE_Msk 0x1FFUL

struct UART_HandleTypeDef {};
struct GPIO_TypeDef {};

int HAL_UART_Transmit(UART_HandleTypeDef *aUart, uint8_t *aData, uint16_t aSize, uint32_t aTimeout);
void HAL_GPIO_TogglePin(GPIO_TypeDef *aPort, uint16_t aPin);

#endif
