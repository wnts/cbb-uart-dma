#ifndef __usart_H
#define __usart_H
#ifdef __cplusplus
 extern "C" {
#endif

#include "main.h"

extern UART_HandleTypeDef huart2;
void MX_USART2_UART_Init(void);

#ifdef __cplusplus
}
#endif
#endif /*__ usart_H */

