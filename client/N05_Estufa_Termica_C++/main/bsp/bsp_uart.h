#ifndef BSP_UART_H
#define BSP_UART_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#define BSP_UART_PORT      UART_NUM_1
#define BSP_UART_BAUD      115200
#define BSP_UART_RX_PIN    4
#define BSP_UART_TX_PIN    5
#define BSP_UART_BUF_MAX   8192

esp_err_t bsp_uart_init(void);
int bsp_uart_read(uint8_t *buf, size_t len, TickType_t timeout);
esp_err_t bsp_uart_write(const uint8_t *buf, size_t len);

#endif // BSP_UART_H




