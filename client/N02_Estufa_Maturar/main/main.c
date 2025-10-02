#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "config.h"
#include "conexoes.h"

// ===== Tags =====
static const char *TAG = "THERMAL_SENSOR";

// ===== Variáveis globais =====
static float thermal_data[24][32];
static TickType_t g_last_send = 0;

// ===== UART / Sensor =====
static void uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk= UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
}

static void extractTemperatures(const uint8_t* payload_bytes, float t[24][32]) {
    for (int row = 0; row < 24; row++) {
        for (int col = 0; col < 32; col++) {
            int index = 2 + (row * 32 + col) * 2;
            int16_t raw_value = (payload_bytes[index + 1] << 8) | payload_bytes[index];
            t[row][col] = raw_value / 100.0f;
        }
    }
}

// monta JSON completo: {"temperaturas":[...], "timestamp":...}
static int build_temperaturas_json(const float t[24][32], char *buf, size_t cap) {
    size_t used = 0;
    int n = snprintf(buf, cap, "{\"temperaturas\":[");
    if (n < 0 || (size_t)n >= cap) return -1;
    used += n;

    for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 32; j++) {
            n = snprintf(buf + used, cap - used, "%.2f", t[i][j]);
            if (n < 0 || (size_t)n >= cap - used) return -1;
            used += n;
            if (!(i == 23 && j == 31)) {
                if (used + 1 >= cap) return -1;
                buf[used++] = ',';
            }
        }
    }

    // Usa tick count como timestamp alternativo
    TickType_t ticks = xTaskGetTickCount();
    n = snprintf(buf + used, cap - used, "],\"timestamp\":%lu}", (unsigned long)ticks);
    if (n < 0 || (size_t)n >= cap - used) return -1;
    used += n;
    return (int)used;
}

static void processFrame(uint8_t* frame_data, uint16_t frame_length) {
    if (frame_length != 1542) return;
    if (frame_data[0] != 0x5A || frame_data[1] != 0x5A) return;

    const uint8_t* payload_bytes = frame_data + 4;
    extractTemperatures(payload_bytes, thermal_data);

    if (!conexao_wifi_is_connected() || !conexao_mqtt_is_connected()) return;

    TickType_t now = xTaskGetTickCount();
    if (g_last_send == 0 || (now - g_last_send) >= pdMS_TO_TICKS(SEND_INTERVAL_MS)) {
        ESP_LOGI(TAG, "Enviando dados térmicos via MQTT...");
        
        char payload[4096]; // Buffer maior para os dados térmicos
        int len = build_temperaturas_json(thermal_data, payload, sizeof(payload));
        
        if (len > 0) {
            conexao_mqtt_publish(MQTT_TOPIC, payload);
            ESP_LOGI(TAG, "Dados térmicos publicados (%d bytes)", len);
        } else {
            ESP_LOGE(TAG, "Falha ao construir payload JSON");
        }
        
        g_last_send = now;
    }
}

// ===== Task principal =====
static void thermal_sensor_task(void *pvParameters) {
    ESP_LOGI(TAG, "Leitura do sensor térmico...");
    
    // Aguarda conexões
    while (!conexao_wifi_is_connected() || !conexao_mqtt_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    uint8_t buffer[BUF_SIZE];
    int buffer_index = 0;
    bool frame_started = false;

    for (;;) {
        uint8_t data[256];
        int length = uart_read_bytes(UART_NUM, data, sizeof(data), pdMS_TO_TICKS(50));
        if (length > 0) {
            for (int i = 0; i < length; i++) {
                uint8_t byte = data[i];
                if (!frame_started) {
                    if (byte == 0x5A) {
                        buffer_index = 0;
                        buffer[buffer_index++] = byte;
                        frame_started = true;
                    }
                } else {
                    if (buffer_index >= BUF_SIZE) {
                        frame_started = false; buffer_index = 0; continue;
                    }
                    buffer[buffer_index++] = byte;

                    if (buffer_index == 2 && buffer[1] != 0x5A) {
                        frame_started = false; buffer_index = 0; continue;
                    }
                    if (buffer_index >= 4) {
                        uint16_t payload_length = (buffer[3] << 8) | buffer[2];
                        uint16_t total_frame_size = 4 + payload_length;
                        if (buffer_index >= total_frame_size) {
                            ESP_LOGI(TAG, "Frame completo (%d bytes)", buffer_index);
                            processFrame(buffer, buffer_index);
                            frame_started = false; buffer_index = 0;
                        }
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    // Inicializa NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Inicializa UART
    uart_init();
    
    // Inicializa Wi-Fi
    conexao_wifi_init();
    
    // Inicializa MQTT
    conexao_mqtt_start();

    // Cria task para leitura do sensor térmico
    xTaskCreate(thermal_sensor_task, "thermal_sensor", 16384, NULL, 5, NULL);

    ESP_LOGI(TAG, "Sistema de câmera térmica iniciado");
    
    // Loop principal para manter conexões
    while (true) {
        if (!conexao_wifi_is_connected()) {
            ESP_LOGW(TAG, "Wi-Fi desconectado");
        } else if (!conexao_mqtt_is_connected()) {
            ESP_LOGW(TAG, "MQTT desconectado");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}