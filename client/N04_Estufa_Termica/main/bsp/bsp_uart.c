#include "bsp_uart.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "BSP_UART"

esp_err_t bsp_uart_init(void) {
    ESP_LOGI(TAG, "Inicializando UART para MLX90640...");
    ESP_LOGI(TAG, "Porta: UART%d, RX: GPIO%d, TX: GPIO%d, Baudrate: %d", 
             UART_THERMAL_PORT, UART_THERMAL_RX_GPIO, UART_THERMAL_TX_GPIO, UART_THERMAL_BAUD);
    
    uart_config_t uart_config = {
        .baud_rate = UART_THERMAL_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    
    esp_err_t ret = uart_driver_install(UART_THERMAL_PORT, UART_THERMAL_BUF_MAX, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao instalar driver UART: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = uart_param_config(UART_THERMAL_PORT, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao configurar parâmetros UART: %s", esp_err_to_name(ret));
        uart_driver_delete(UART_THERMAL_PORT);
        return ret;
    }
    
    // Configura TX e RX (igual ao projeto N05 que funcionou)
    ret = uart_set_pin(UART_THERMAL_PORT, UART_THERMAL_TX_GPIO, UART_THERMAL_RX_GPIO, 
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao configurar pinos UART: %s", esp_err_to_name(ret));
        uart_driver_delete(UART_THERMAL_PORT);
        return ret;
    }
    
    // Limpa buffer UART (remove dados residuais)
    uint8_t dummy[256];
    int cleared = uart_read_bytes(UART_THERMAL_PORT, dummy, sizeof(dummy), pdMS_TO_TICKS(100));
    if (cleared > 0) {
        ESP_LOGI(TAG, "Buffer UART limpo: %d bytes descartados", cleared);
    }
    
    ESP_LOGI(TAG, "UART inicializado com sucesso (RX: GPIO%d, TX: GPIO%d)", 
             UART_THERMAL_RX_GPIO, UART_THERMAL_TX_GPIO);
    return ESP_OK;
}

int bsp_uart_read(uint8_t *buf, size_t len, TickType_t timeout) {
    return uart_read_bytes(UART_THERMAL_PORT, buf, len, timeout);
}

esp_err_t bsp_uart_write(const uint8_t *buf, size_t len) {
    int written = uart_write_bytes(UART_THERMAL_PORT, buf, len);
    return (written == len) ? ESP_OK : ESP_FAIL;
}

