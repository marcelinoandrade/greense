#include "bsp_uart.h"
#include "driver/uart.h"
#include "esp_log.h"

#define TAG "BSP_UART"

esp_err_t bsp_uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate = BSP_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    
    esp_err_t ret = uart_driver_install(BSP_UART_PORT, BSP_UART_BUF_MAX, 0, 0, NULL, 0);
    if (ret != ESP_OK) return ret;
    
    ret = uart_param_config(BSP_UART_PORT, &uart_config);
    if (ret != ESP_OK) return ret;
    
    ret = uart_set_pin(BSP_UART_PORT, BSP_UART_TX_PIN, BSP_UART_RX_PIN, 
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    return ret;
}

int bsp_uart_read(uint8_t *buf, size_t len, TickType_t timeout) {
    return uart_read_bytes(BSP_UART_PORT, buf, len, timeout);
}

esp_err_t bsp_uart_write(const uint8_t *buf, size_t len) {
    int written = uart_write_bytes(BSP_UART_PORT, buf, len);
    return (written == len) ? ESP_OK : ESP_FAIL;
}




