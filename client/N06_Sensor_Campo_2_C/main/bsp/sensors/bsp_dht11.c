#include "bsp_dht11.h"

#include "../board.h"
#include "dht.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "BSP_DHT11";
static bool dht_available = false;

// Última leitura bem-sucedida (para garantir intervalo mínimo)
static TickType_t last_successful_read_ticks = 0;
static const TickType_t MIN_READ_INTERVAL_MS = 2100; // DHT11 precisa de ~2s entre leituras

esp_err_t dht11_bsp_init(void)
{
#ifdef BSP_GPIO_DHT11
    gpio_reset_pin(BSP_GPIO_DHT11);
    // Linha ociosa deve ficar em nível alto.
    gpio_set_direction(BSP_GPIO_DHT11, GPIO_MODE_OUTPUT);
    gpio_set_level(BSP_GPIO_DHT11, 1);
    dht_available = true;
    ESP_LOGI(TAG, "DHT11 configurado no GPIO %d", BSP_GPIO_DHT11);
    return ESP_OK;
#else
    ESP_LOGW(TAG, "DHT11 não configurado (BSP_GPIO_DHT11 ausente)");
    return ESP_ERR_NOT_FOUND;
#endif
}

bool dht11_bsp_is_available(void)
{
    return dht_available;
}

/**
 * @brief Valida se os valores lidos do DHT11 são razoáveis
 */
static bool validate_dht11_values(float temp, float humid)
{
    // DHT11 range: temperatura -40°C a +80°C, umidade 0-100%
    if (isnan(temp) || isnan(humid)) {
        return false;
    }
    if (temp < -40.0f || temp > 80.0f) {
        ESP_LOGW(TAG, "Temperatura fora do range: %.1f°C", temp);
        return false;
    }
    if (humid < 0.0f || humid > 100.0f) {
        ESP_LOGW(TAG, "Umidade fora do range: %.1f%%", humid);
        return false;
    }
    return true;
}

/**
 * @brief Garante que o pino está em estado correto antes de iniciar leitura
 */
static void prepare_dht11_pin(void)
{
    // Garante que o pino está em nível alto (estado ocioso)
    gpio_set_direction(BSP_GPIO_DHT11, GPIO_MODE_OUTPUT);
    gpio_set_level(BSP_GPIO_DHT11, 1);
    vTaskDelay(pdMS_TO_TICKS(10)); // Pequeno delay para estabilizar
}

esp_err_t dht11_bsp_read(float *temperature, float *humidity)
{
    if (!dht_available) {
        return ESP_ERR_INVALID_STATE;
    }

    // Garante intervalo mínimo entre leituras (DHT11 precisa de ~2s)
    TickType_t now = xTaskGetTickCount();
    if (last_successful_read_ticks != 0) {
        TickType_t elapsed_ticks = now - last_successful_read_ticks;
        TickType_t min_interval_ticks = pdMS_TO_TICKS(MIN_READ_INTERVAL_MS);
        if (elapsed_ticks < min_interval_ticks) {
            TickType_t wait_ticks = min_interval_ticks - elapsed_ticks;
            vTaskDelay(wait_ticks);
            now = xTaskGetTickCount();
        }
    }

    dht11_t dht = {
        .dht11_pin = BSP_GPIO_DHT11,
        .humidity = 0.0f,
        .temperature = 0.0f,
    };

    // Preparação do pino antes de cada tentativa
    prepare_dht11_pin();

    // Retry com backoff: tenta até 3 vezes com delays progressivos
    const int max_retries = 3;
    int ret = -1;
    
    for (int attempt = 0; attempt < max_retries; attempt++) {
        if (attempt > 0) {
            // Delay progressivo: 50ms, 100ms, 200ms
            vTaskDelay(pdMS_TO_TICKS(50 * (1 << (attempt - 1))));
            prepare_dht11_pin();
        }

        // Aumenta o número de tentativas internas do dht11_read para 8
        ret = dht11_read(&dht, 8);
        
        if (ret == 0) {
            // Valida valores antes de aceitar
            if (validate_dht11_values(dht.temperature, dht.humidity)) {
                if (temperature) *temperature = dht.temperature;
                if (humidity) *humidity = dht.humidity;
                last_successful_read_ticks = xTaskGetTickCount();
                return ESP_OK;
            } else {
                ESP_LOGW(TAG, "Valores inválidos: temp=%.1f°C, umid=%.1f%%", 
                         dht.temperature, dht.humidity);
                ret = -1; // Força nova tentativa
            }
        }
    }

    ESP_LOGW(TAG, "Falha na leitura do DHT11 após %d tentativas (ret=%d)", max_retries, ret);
    return ESP_ERR_INVALID_RESPONSE;
}


