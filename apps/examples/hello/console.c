#include "console.h"
#include "stm32f4xx.h"
//#include "../../../arch/arm/src/stm32/chip/stm32f40xxx_rcc.h"

// pin connections
// PE0 = uart8 TX on AF8
// PE1 = uart8 RX on AF8

#define PORTE_RX 0
#define PORTE_TX 1

static volatile uint8_t s_console_init_complete = 0;

void console_init()
{
  s_console_init_complete = 1;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
  RCC->APB1ENR |= RCC_APB1ENR_UART8EN;
  GPIOE->MODER   |= (0x2) << (PORTE_TX * 2);
  GPIOE->AFR[0]  |= (0x8) << (PORTE_TX * 4);
  // RX not used at the moment. TX only used for stdout
  UART8->CR1 &= ~USART_CR1_UE;
  UART8->CR1 |=  USART_CR1_TE | USART_CR1_RE;
  // we want 1 megabit. do this with mantissa=2 and fraction (sixteenths)=10
  UART8->BRR  = (((uint16_t)2) << 4) | 10;
  UART8->CR1 |=  USART_CR1_UE;
}

/*void console_send_block(const uint8_t *buf, uint32_t len)
{
  if (!s_console_init_complete)
    console_init();
  for (uint32_t i = 0; i < len; i++)
  {
    while (!(UART8->SR & USART_SR_TXE)) { } // wait for tx buffer to clear
    UART8->DR = buf[i];
  }
  while (!(UART8->SR & USART_SR_TC)) { } // wait for TX to finish
  //for (volatile int i = 0; i < 100000; i++) { } // give usb uart some time...
}*/

