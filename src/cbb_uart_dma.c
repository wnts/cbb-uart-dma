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

void cbb_uart_dma_copy_into(cbb_uart_dma_buffer_t *buffer, const uint8_t *data, uint16_t len);
void cbb_uart_dma_write(cbb_uart_dma_buffer_t *buffer, const uint8_t *data, uint16_t len);
void cbb_uart_dma_end_of_dma_transfer_callback(cbb_uart_dma_buffer_t * buffer, bool called_from_isr);
void cbb_uart_dma_transfer_pause(cbb_uart_dma_buffer_t *buffer);
void cbb_uart_dma_transfer_resume(cbb_uart_dma_buffer_t *buffer, uint16_t new_length, uint16_t increment_length, bool blocking, bool called_from_isr);
void cbb_uart_dma_transfer_start(cbb_uart_dma_buffer_t *buffer, uint16_t length, bool blocking, bool called_from_isr);

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
    HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 6, 2);

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
    if(read > buffer->len)
    {
    	read = 0;
    }

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

void cbb_uart_dma_copy_into(cbb_uart_dma_buffer_t *buffer, const uint8_t *data, uint16_t len)
{
	uint16_t bytes_till_end = buffer->len - buffer->write;
	if(len > bytes_till_end)
	{
		memcpy(&buffer->data[buffer->write], data, bytes_till_end);
		memcpy(&buffer->data[0], data + bytes_till_end, len - bytes_till_end);
	}
	else
	{
		memcpy(&buffer->data[buffer->write], data, len);
	}
    buffer->write = (buffer->write + len) % buffer->len;
}

void cbb_uart_dma_write(cbb_uart_dma_buffer_t *buffer, const uint8_t *data, uint16_t len)
{
    xSemaphoreTake(buffer->mutex, portMAX_DELAY);
    {
		uint16_t free = 0;

    	cbb_uart_dma_transfer_pause(buffer);
    	free = cbb_uart_dma_get_free(buffer);
    	if(len > free)
    	{
    		// Resume dma_transfer (blocking) with new length of len-free bytes
    		cbb_uart_dma_transfer_resume(buffer, len-free, 0, true, false);
    		// copy 'len' bytes into the buffer.
    		cbb_uart_dma_copy_into(buffer, data, len);
    		// Start a new DMA transfer of 'len' bytes.
    		cbb_uart_dma_transfer_start(buffer, len, false, false);
    	}
    	else
    	{
    		// Copy 'len' bytes into buffer
    		cbb_uart_dma_copy_into(buffer, data, len);
    		// Resume DMA transfer with length increment of 'len' bytes
    		cbb_uart_dma_transfer_resume(buffer, 0, len, false, false);
    	}

    }
    xSemaphoreGive(buffer->mutex);
}

void cbb_uart_dma_end_of_dma_transfer_callback(cbb_uart_dma_buffer_t * buffer, bool called_from_isr)
{
	if(called_from_isr)
		LL_DMA_ClearFlag_TC6(CBB_UART_DMA_INSTANCE);
	/* Situation 1: non-wrapping DMA transfer ending BEFORE the end of the linear buffer */
	if(buffer->dma_transfer_start + buffer->dma_transfer_size < buffer->len)
	{
		buffer->read = buffer->dma_transfer_start + buffer->dma_transfer_size;
		buffer->dma_transfer_size = 0;
	}
	/* Situation 2: non-wrapping DMA transfer ending AT the end of the linear buffer OR wrapping DMA transfer ended at the end of the linear buffer. */
	else if(buffer->dma_transfer_start + buffer->dma_transfer_size >= buffer->len)
	{
		buffer->read = 0;
		if(buffer->dma_transfer_start + buffer->dma_transfer_size > buffer->len) /* wrapping DMA transfer ending at end of linear buffer */
		{
			buffer->dma_transfer_size -= buffer->len - buffer->dma_transfer_start;
			/* When called from within ISR, we need to start second part of DMA transfer here
			 * When not called from within ISR (but cbb_uart_dma_transfer_pause), second part of DMA transfer will be started by some function down on the call stack.
			 */
			if(called_from_isr)
			{
				cbb_uart_dma_transfer_resume(buffer, 0, 0, false, true);
				return;
			}
		}
		else /* non-wrapping transfer ending AT end of the linear buffer */
		{
			buffer->dma_transfer_size = 0;
		}
	}
	if(buffer->dma_transfer_blocking)
	{
		buffer->dma_transfer_blocking = false;
		if(called_from_isr)
		{
			BaseType_t higher_priority_task_has_woken = pdFALSE;
			xSemaphoreGiveFromISR(buffer->semaphore_non_empty, &higher_priority_task_has_woken);
			while(!LL_USART_IsActiveFlag_TXE(huart2.Instance))
				;
			portYIELD_FROM_ISR(higher_priority_task_has_woken);
		}
		else
		{
			if(!LL_USART_IsActiveFlag_TXE(huart2.Instance))
				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
			xSemaphoreGive(buffer->semaphore_non_empty);
		}
	}
}


/**
 * Pause an ongoing DMA transfer.
 */
void cbb_uart_dma_transfer_pause(cbb_uart_dma_buffer_t *buffer)
{
	portENTER_CRITICAL();
	if(!buffer->dma_transfer_ongoing)
	{
		portEXIT_CRITICAL();
		return;
	}
	if(LL_USART_IsEnabledDMAReq_TX(huart2.Instance))
	{
		LL_USART_DisableDMAReq_TX(huart2.Instance);
		LL_DMA_DisableStream(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
	}
	while(LL_DMA_IsEnabledStream(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM))
		;
	while(!LL_USART_IsActiveFlag_TXE(huart2.Instance))
			;
	/* If this function is called just at the moment that the last character of an ongoing DMA transfer was flushed out over the UART,
	 * the TC interrupt will not be fired anymore, and we need to perform its work here.
	 * This situation can be detected by checking if the remaining data length of an ongoing dma transfer.
	 */
	if(LL_DMA_GetDataLength(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM) == 0)
	{
		cbb_uart_dma_end_of_dma_transfer_callback(buffer, false);
	}
	else /* Transfer interrupted before it's end */
	{
		buffer->read = buffer->dma_transfer_start + buffer->dma_transfer_size - LL_DMA_GetDataLength(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
		buffer->dma_transfer_size = LL_DMA_GetDataLength(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
	}
	buffer->dma_transfer_ongoing = false;
	LL_DMA_ClearFlag_HT6(CBB_UART_DMA_INSTANCE);
	LL_DMA_ClearFlag_TC6(CBB_UART_DMA_INSTANCE);
	HAL_NVIC_ClearPendingIRQ(DMA1_Stream6_IRQn);
	portEXIT_CRITICAL();
}


/**
 * Resume a previously paused DMA transfer on a buffer.
 * The transfer is restarted from the memory location where it was suspended.
 *
 * @param buffer            Pointer to the buffer structure.
 * @param new_length        New length of the dma transfer. Set to 0 if no new length is required or
 * 							if increment_legnth is nonzero.
 * @param increment_length  Increment with which the length of the paused DMA transfer is to be increased.
 * 							Set to 0 if no increment is required, or if new_length is nonzero.
 * @param blocing			Set to true if this function needs to block until the DMA transfer is completed.
 *
 *
 * @note increment_length and new_length cannot be simultaneously set to a non-zero value.
 */
void cbb_uart_dma_transfer_resume(cbb_uart_dma_buffer_t *buffer, uint16_t new_length, uint16_t increment_length, bool blocking, bool called_from_isr)
{
	if(new_length > 0)
	{
		cbb_uart_dma_transfer_start(buffer, new_length, blocking, called_from_isr);
	}
	else if(increment_length > 0)
	{
		cbb_uart_dma_transfer_start(buffer, buffer->dma_transfer_size + increment_length, blocking, called_from_isr);
	}
	else
	{
		cbb_uart_dma_transfer_start(buffer, buffer->dma_transfer_size, blocking, called_from_isr);
	}
}

void cbb_uart_dma_transfer_start(cbb_uart_dma_buffer_t *buffer, uint16_t length, bool blocking, bool called_from_isr)
{
	buffer->dma_transfer_ongoing = true;
	buffer->dma_transfer_start = buffer->read;
	buffer->dma_transfer_size = length;
	if(!called_from_isr)
		buffer->dma_transfer_blocking = blocking;
	LL_DMA_SetMemoryAddress(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM, (uint32_t)&buffer->data[buffer->read]);
	LL_DMA_SetDataLength(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM, length);
	LL_DMA_EnableStream(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
	LL_DMA_EnableIT_TC(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
	LL_DMA_EnableIT_TE(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
	LL_USART_EnableDMAReq_TX(huart2.Instance);
	if(blocking && !called_from_isr)
	{
		xSemaphoreTake(buffer->semaphore_non_empty, portMAX_DELAY);
	}
}

//void cbb_uart_dma_write(cbb_uart_dma_buffer_t *buffer, const uint8_t *data, uint16_t len)
//{
//    xSemaphoreTake(buffer->mutex, portMAX_DELAY);
//    {
//        HAL_StatusTypeDef hal_status = HAL_OK;
//        bool dma_transfer_ongoing = false;
//        bool wrapping_dma_transfer_ongoing = false;
//        uint16_t read = 0;
//
//		uint16_t num_polls = 0;
//		/* Compute read index */
//		read = buffer->dma_transfer_start + buffer->dma_transfer_size - LL_DMA_GetDataLength(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
//		portEXIT_CRITICAL();
//		while(!LL_USART_IsActiveFlag_TXE(huart2.Instance))
//			;
//
//		uint16_t free = cbb_uart_dma_get_free(buffer);
//		if(free == 0)
//		{
//			/* Resume DMA transfer and wait for its completion */
//			buffer->signal_non_empty = true;
//			LL_DMA_EnableIT_TC(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
//			LL_DMA_EnableStream(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
//			LL_USART_EnableDMAReq_TX(huart2.Instance);
//			xSemaphoreTake(buffer->semaphore_non_empty, portMAX_DELAY);
//			free = cbb_uart_dma_get_free(buffer);
//		}
//
//		uint16_t bytes_to_wriwie = len <= free ? len : free;
//		uint16_t bytes_till_end = buffer->len - buffer->write;
//		if(bytes_to_write > bytes_till_end)
//		{
//			memcpy(&buffer->data[buffer->write], data, bytes_till_end);
//			memcpy(&buffer->data[0], data + bytes_till_end, bytes_to_write - bytes_till_end);
//		}
//		else
//		{
//			memcpy(&buffer->data[buffer->write], data, bytes_to_write);
//		}
//		buffer->write = (buffer->write + bytes_to_write) % buffer->len;
//
//		uint16_t dma_transfer_size = 0;
//		uint16_t dma_transfer_start = read;
//		if(buffer->write > read)
//		{
//			/* DMA transfer till write index - 1 */
//			dma_transfer_size = bytes_to_write;
//			if(dma_transfer_ongoing)
//				dma_transfer_size += buffer->dma_transfer_size - LL_DMA_GetDataLength(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
//		}
//		else
//		{
//			/* Start or resume DMA transfer till end */
//			dma_transfer_size = buffer->len - read;
//			buffer->dma_transfer_is_wrapped = true;
//		}
//		LL_DMA_SetMemoryAddress(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM, (uint32_t)(&buffer->data[dma_transfer_start]));
//		LL_DMA_SetDataLength(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM, dma_transfer_size);
//		LL_DMA_EnableStream(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
//		buffer->dma_transfer_start = dma_transfer_start;
//		buffer->dma_transfer_size = dma_transfer_size;
//		LL_DMA_EnableIT_TC(CBB_UART_DMA_INSTANCE, CBB_UART_DMA_STREAM);
//		LL_USART_EnableDMAReq_TX(huart2.Instance);
//    }
//    xSemaphoreGive(buffer->mutex);
//}
