#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "usart.h"
#include "cbb_uart_dma.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define NUM_TASKS 3

TaskHandle_t tasks[NUM_TASKS];
StackType_t  task_stacks[NUM_TASKS * configMINIMAL_STACK_SIZE];
StaticTask_t tasks_controldata[NUM_TASKS];

TaskHandle_t main_task;
StackType_t  main_task_stack[configMINIMAL_STACK_SIZE];
StaticTask_t main_task_controldata;

void task_func(void *argument);

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
    for(uint8_t i = 0; i < NUM_TASKS; i++)
    {
        char task_name[configMAX_TASK_NAME_LEN + 1];

        snprintf(task_name, sizeof(task_name), "task%02d", i);
        tasks[i] = xTaskCreateStatic(task_func, task_name, (sizeof(task_stacks) / NUM_TASKS) / sizeof(StackType_t), (void const *)i, i,
                                     &task_stacks[i * configMINIMAL_STACK_SIZE], &tasks_controldata[i]);
    }
}

void task_func(void *argument)
{
    uint32_t    task_id   = (uint32_t)argument;
    const char *task_name = pcTaskGetName(NULL);
    char        data[32]  = { 0 };

    snprintf(data, sizeof(data), "%s\n", task_name);
    cbb_uart_dma_write(dma_buffer, data, strlen(data));
    while(true)
    {
        for(uint8_t i = 0; i < 10; i++)
        {
            snprintf(data, sizeof(data), "%s: %u\n", task_name, i);
            cbb_uart_dma_write(dma_buffer, data, strlen(data));
        }
        vTaskSuspend(NULL);
    }
}
