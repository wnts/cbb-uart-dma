#include "cbb_uart_dma.h"
#include "stm32f4xx_hal.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_usart.h"
#include <stdbool.h>
#include <string.h>

#define CBB_UART_DMA_BUFFER_LEN 1024

DMA_HandleTypeDef dma_handle;

static uint16_t              cbb_uart_dma_get_occupancy(cbb_uart_dma_buffer_t *buffer);
static uint16_t              cbb_uart_dma_get_free(cbb_uart_dma_buffer_t *buffer);
static cbb_uart_dma_buffer_t _dma_buffer = { 0 };
static StaticSemaphore_t     cbb_uart_dma_mutex_controldata;
static StaticSemaphore_t     cbb_uart_dma_semaphore_non_empty_controldata;
static uint8_t               cbb_uart_dma_data[CBB_UART_DMA_BUFFER_LEN];

cbb_uart_dma_buffer_t *cbb_uart_dma_init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    LL_DMA_InitTypeDef dma_init     = { 0 };
    dma_init.Channel                = CBB_UART_DMA_CHANNEL;
    dma_init.Direction              = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    dma_init.FIFOMode               = LL_DMA_FIFOMODE_DISABLE;
    dma_init.MemBurst               = LL_DMA_MBURST_SINGLE;
    dma_init.Mode                   = LL_DMA_MODE_NORMAL;
    dma_init.PeriphBurst            = LL_DMA_PBURST_SINGLE;
    dma_init.Priority               = LL_DMA_PRIORITY_VERYHIGH;
    dma_init.PeriphOrM2MSrcAddress  = (uint32_t)&huart2.Instance->DR;
    dma_init.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
    dma_init.PeriphOrM2MSrcIncMode  = LL_DMA_PERIPH_NOINCREMENT;
    dma_init.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
    dma_init.MemoryOrM2MDstIncMode  = LL_DMA_MEMORY_INCREMENT;
    LL_DMA_Init(DMA1, CBB_UART_DMA_STREAM, &dma_init);
    LL_DMA_EnableIT_TC(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
    HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
    //    HAL_NVIC_EnableIRQ(USART2_IRQn);

    _dma_buffer.mutex               = xSemaphoreCreateMutexStatic(&cbb_uart_dma_mutex_controldata);
    _dma_buffer.semaphore_non_empty = xSemaphoreCreateBinaryStatic(&cbb_uart_dma_semaphore_non_empty_controldata);
    _dma_buffer.data                = cbb_uart_dma_data;
    _dma_buffer.len                 = sizeof(cbb_uart_dma_data);
    _dma_buffer.write               = 0;
    _dma_buffer.dma_transfer_start  = 0;
    _dma_buffer.dma_transfer_size   = 0;

    return &_dma_buffer;
}

/**
 * Get the occupancy (in bytes) of the binary buffer.
 *
 * @warn Call this function only when the DMA controller is suspended!
 */
static uint16_t cbb_uart_dma_get_occupancy(cbb_uart_dma_buffer_t *buffer)
{
    return buffer->len - cbb_uart_dma_get_free(buffer);
}

static uint16_t cbb_uart_dma_get_free(cbb_uart_dma_buffer_t *buffer)
{
    uint16_t read = buffer->dma_transfer_start + buffer->dma_transfer_size - LL_DMA_GetDataLength(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);

    if(read > buffer->write)
    {
    	if(buffer->write == read - 1)
    		return 0;
    	else
			return read - buffer->write;
    }
    else
    {
			return (uint32_t)buffer->len + (uint32_t)read - (uint32_t)buffer->write;
    }
}

void cbb_uart_dma_write(cbb_uart_dma_buffer_t *buffer, const uint8_t *data, uint16_t len)
{
    xSemaphoreTake(buffer->mutex, portMAX_DELAY);
    {
        uint16_t          data_left  = len;
        HAL_StatusTypeDef hal_status = HAL_OK;
        bool dma_transfer_ongoing = false;

        while(data_left > 0)
        {
            uint16_t num_polls = 0;
            if(LL_USART_IsEnabledDMAReq_TX(huart2.Instance))
            {
                LL_USART_DisableDMAReq_TX(huart2.Instance);
                LL_DMA_DisableStream(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
                dma_transfer_ongoing = true;
            }
			while(LL_DMA_IsEnabledStream(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM))
				;
			while(!LL_USART_IsActiveFlag_TXE(huart2.Instance))
				;
            uint16_t rem  = 0;
            uint16_t free = cbb_uart_dma_get_free(buffer);
            if(free == 0)
            {
                xSemaphoreTake(buffer->semaphore_non_empty, portMAX_DELAY);
                free = cbb_uart_dma_get_free(buffer);
            }

            uint16_t read = buffer->dma_transfer_start + buffer->dma_transfer_size - LL_DMA_GetDataLength(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
            uint16_t bytes_to_write = len <= free ? len : free;
            uint16_t bytes_till_end = buffer->len - buffer->write;
            if(bytes_to_write > bytes_till_end)
            {
                memcpy(&buffer->data[buffer->write], data, bytes_till_end);
                memcpy(&buffer->data[0], data + bytes_till_end, bytes_to_write - bytes_till_end);
            }
            else
            {
                memcpy(&buffer->data[buffer->write], data, bytes_to_write);
            }
            buffer->write = (buffer->write + bytes_to_write - 1) % buffer->len;
            data_left -= bytes_to_write;
            if(buffer->write > read)
            {
                // DMA transfer till write index - 1
                uint16_t dma_transfer_size = bytes_to_write;
                if(dma_transfer_ongoing)
                    dma_transfer_size += buffer->dma_transfer_size - LL_DMA_GetDataLength(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
                uint16_t dma_transfer_start = read;
                LL_DMA_SetMemoryAddress(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM, (uint32_t)(&buffer->data[dma_transfer_start]));
                LL_DMA_SetDataLength(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM, dma_transfer_size);
				LL_DMA_EnableStream(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
                LL_USART_EnableDMAReq_TX(huart2.Instance);
                buffer->dma_transfer_start = dma_transfer_start;
                buffer->dma_transfer_size  = dma_transfer_size;
            }
            else
            {
                ;    // DMA transfer till end, second part of DMA transfer (from beginning to write ptr) will be initiated in ISR
            }
        }
    }
    xSemaphoreGive(buffer->mutex);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    /*
		if(block)
			give_semaphore_cb_not_empty()
	*/
}

void cbb_uart_dma_transfer_complete(void)
{
    /*
     * 	  // wrap-around
	  	  if(dma_mem_ptr = linear_buffer[size-1] && write_ptr != &linear_buffer[0])
	  	  {
	  	  	  start_dma_tfer(start = linear_buffer[0], cnt = write_ptr)
		  }
		  else // normal end of tfer
		  {
		  	  // do nothing?
		  }
	 */
}
