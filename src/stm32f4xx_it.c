#include "main.h"
#include "usart.h"
#include "stm32f4xx_it.h"
#include "cmsis_os.h"
#include "cbb_uart_dma.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_usart.h"

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
    if(LL_DMA_IsActiveFlag_TC6(CBB_UART_DMA_INSTANCE))
    {
        LL_DMA_ClearFlag_TC6(CBB_UART_DMA_INSTANCE);
        LL_USART_DisableDMAReq_TX(huart2.Instance);
    }
}
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}
void SysTick_Handler(void)
{
    HAL_IncTick();
    osSystickHandler();
}
