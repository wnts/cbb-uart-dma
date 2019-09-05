#include "main.h"
#include "usart.h"
#include "stm32f4xx_it.h"
#include "cmsis_os.h"
#include "cbb_uart_dma.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_usart.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include <stdbool.h>

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    while(1)
    {
    }
}

void MemManage_Handler(void)
{
    while(1)
    {
    }
}

void BusFault_Handler(void)
{
    while(1)
    {
    }
}

void UsageFault_Handler(void)
{
    while(1)
    {
    }
}

void DebugMon_Handler(void)
{
}

void DMA1_Stream6_IRQHandler(void)
{
    if(LL_DMA_IsActiveFlag_TE6(CBB_UART_DMA_INSTANCE))
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    cbb_uart_dma_end_of_dma_transfer_callback(dma_buffer, true);
}
void USART2_IRQHandler(void)
{
    if(LL_USART_IsActiveFlag_FE(huart2.Instance) || LL_USART_IsActiveFlag_ORE(huart2.Instance))
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
}
void SysTick_Handler(void)
{
    HAL_IncTick();
    osSystickHandler();
}
