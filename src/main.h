#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "cbb_uart_dma.h"

extern cbb_uart_dma_buffer_t *dma_buffer;
void                          Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
