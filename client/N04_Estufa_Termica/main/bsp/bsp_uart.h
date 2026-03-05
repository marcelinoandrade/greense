#ifndef BSP_UART_H
#define BSP_UART_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "bsp_pins.h"

esp_err_t bsp_uart_init(void);
int bsp_uart_read(uint8_t *buf, size_t len, TickType_t timeout);
esp_err_t bsp_uart_write(const uint8_t *buf, size_t len);

#endif // BSP_UART_H

