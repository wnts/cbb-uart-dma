#include "cbb_uart_dma.h"
#include "stm32f4xx_ll_dma.h"

typedef struct
{
    DMA_TypeDef *instance;
    uint32_t     channel;
    uint32_t     stream;

} dma_t;

static dma_t cbb_uart_dma_handle;

cbb_uart_dma_status_t dma_initialize(void **dma_handle)
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
    cbb_uart_dma_handle.channel  = LL_DMA_CHANNEL_4;
    cbb_uart_dma_handle.stream   = LL_DMA_STREAM_6;
    cbb_uart_dma_handle.instance = DMA1;

    *dma_handle = &cbb_uart_dma_handle;

    return CBB_UART_DMA_OK;
}
