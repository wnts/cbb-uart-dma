#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "usart.h"
#include "cbb_uart_dma.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

TaskHandle_t main_task;
StackType_t  main_task_stack[configMINIMAL_STACK_SIZE];
StaticTask_t main_task_controldata;

void main_task_func(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize);

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

__weak void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
    while(true)
        ;
}

static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t  xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer   = &xIdleTaskTCBBuffer;
    *ppxIdleTaskStackBuffer = &xIdleStack[0];
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void)
{
    main_task = xTaskCreateStatic(main_task_func, "main task", sizeof(main_task_stack) / sizeof(StackType_t), NULL, 1, main_task_stack,
                                  &main_task_controldata);
}

/**
  * @brief  Function implementing the main_task thread.
  * @param  argument: Not used 
  * @retval None
  */
void main_task_func(void *argument)
{
    uint8_t  foo[64];
    uint16_t i = 0;
    cbb_uart_dma_init();
    while(true)
    {
        sprintf(foo, "%u\n", i);
        cbb_uart_dma_write(dma_buffer, foo, strlen(foo) + 1);
        i++;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
