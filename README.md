# cbb-uart-dma
Circular debug message buffer using DMA for output over UART.

## Use case
RTOS based application that has multiple tasks writing large amounts of debug messages to a shared circular buffer whose contents
are continuously transmitted using UART.

By using the DMA (as opposed to blocking I/O in a separate task), the blocking times of the tasks writing to the buffer 
are minimized. This way, program tracing has minimal impact on execution time.

## Implementation details
Right now it is written using FreeRTOS and targetting the STM32F4xx MCU family.
In the future, I might add abstractions for easy porting to other RTOS/target hardware combinations.
