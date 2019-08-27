cmake_minimum_required(VERSION 3.14)
project(uart-dma C ASM)
set(C_SOURCES
	  src/system_stm32f4xx.c
	  src/main.c
	  src/gpio.c
	  src/usart.c
	  src/stm32f4xx_it.c
	  src/stm32f4xx_hal_msp.c
	  src/freertos.c
	  src/cbb_uart_dma.c
	  src/drivers/STM32F4xx_HAL_Driver/stm32f4xx_hal.c
    src/drivers/STM32F4xx_HAL_Driver/stm32f4xx_hal_cortex.c
	  src/drivers/STM32F4xx_HAL_Driver/stm32f4xx_hal_rcc.c
	  src/drivers/STM32F4xx_HAL_Driver/stm32f4xx_hal_rcc_ex.c
	  src/drivers/STM32F4xx_HAL_Driver/stm32f4xx_hal_pwr.c
	  src/drivers/STM32F4xx_HAL_Driver/stm32f4xx_hal_gpio.c
	  src/drivers/STM32F4xx_HAL_Driver/stm32f4xx_hal_usart.c
	  src/drivers/STM32F4xx_HAL_Driver/stm32f4xx_hal_uart.c
	  src/drivers/STM32F4xx_HAL_Driver/stm32f4xx_hal_dma.c
	  src/drivers/STM32F4xx_HAL_Driver/stm32f4xx_ll_dma.c
	  src/drivers/STM32F4xx_HAL_Driver/stm32f4xx_ll_usart.c
	  src/FreeRTOS/list.c
	  src/FreeRTOS/queue.c
	  src/FreeRTOS/tasks.c
	  src/FreeRTOS/CMSIS_RTOS/cmsis_os.c
	  src/FreeRTOS/portable/GCC/ARM_CM4F/port.c)
set(ASM_SOURCES src/startup_stm32f411xe.s)


set(ARCH_FLAGS  -mcpu=cortex-m4
				        -mthumb
	              -mfpu=fpv4-sp-d16
				        -mfloat-abi=hard)
set(LDSCRIPT STM32F411RETx_FLASH.ld)

include_directories(src/
					          src/drivers/STM32F4xx_HAL_Driver
		          			src/drivers/CMSIS/Device/ST/STM32F4xx/Include
				          	src/drivers/CMSIS/Include
          					src/FreeRTOS/CMSIS_RTOS
				          	src/FreeRTOS/include
          					src/FreeRTOS/portable/GCC/ARM_CM4F)
add_definitions(-DSTM32F411xE
			         	-DUSE_HAL_DRIVER
				        -DUSE_FULL_LL_DRIVER)

add_compile_options(${ARCH_FLAGS})
add_link_options(${ARCH_FLAGS})
add_compile_options(-fdata-sections
					          -ffunction-sections)

add_link_options(-specs=nano.specs
                 -specs=nosys.specs
				         -T${CMAKE_SOURCE_DIR}/${LDSCRIPT}
				         -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map,--cref
				         -Wl,--gc-sections)
if(CMAKE_BUILD_TYPE MATCHES Debug)
	add_compile_options(-g -O0)
	add_link_options(-Wl,--undefined=uxTopUsedPriority)
	set(C_SOURCES ${C_SOURCES} contrib/FreeRTOS-openocd.c)
elseif(CMAKE_BUILD_TYPE MATCHES Release)
	add_compile_options(-Os)
endif()


										
set(SOURCES ${ASM_SOURCES} ${C_SOURCES})

add_executable(${PROJECT_NAME} ${SOURCES})

