#ifndef SRC_CBB_UART_DMA_H_
#define SRC_CBB_UART_DMA_H_

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include <stdbool.h>

#define CBB_UART_DMA_INSTANCE DMA1
#define CBB_UART_DMA_STREAM   LL_DMA_STREAM_6
#define CBB_UART_DMA_CHANNEL  LL_DMA_CHANNEL_4

typedef struct
{
    SemaphoreHandle_t mutex;
    SemaphoreHandle_t semaphore_non_empty;
    uint16_t          len;
    uint16_t          write;
    uint16_t          dma_transfer_size;
    uint16_t          dma_transfer_start;
    uint8_t *         data;

} cbb_uart_dma_buffer_t;

extern DMA_HandleTypeDef dma_handle;

cbb_uart_dma_buffer_t *cbb_uart_dma_init(void);
static uint16_t        cbb_uart_dma_get_occupancy(cbb_uart_dma_buffer_t *buffer);
static uint16_t        cbb_uart_dma_get_free(cbb_uart_dma_buffer_t *buffer);
void                   cbb_uart_dma_write(cbb_uart_dma_buffer_t *buffer, const uint8_t *data, uint16_t len);
void                   cbb_uart_dma_half_transfer_complete(void);
void                   cbb_uart_dma_transfer_complete(void);
#endif /* SRC_CBB_UART_DMA_H_ */
