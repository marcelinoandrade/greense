#include "bsp_dht11.h"

#include "../board.h"
#include "dht.h"

#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "BSP_DHT11";
static bool dht_available = false;

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

esp_err_t dht11_bsp_read(float *temperature, float *humidity)
{
    if (!dht_available) {
        return ESP_ERR_INVALID_STATE;
    }

    dht11_t dht = {
        .dht11_pin = BSP_GPIO_DHT11,
        .humidity = 0.0f,
        .temperature = 0.0f,
    };

    int ret = dht11_read(&dht, 5);
    if (ret == 0) {
        if (temperature) *temperature = dht.temperature;
        if (humidity) *humidity = dht.humidity;
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Falha na leitura do DHT11 (ret=%d)", ret);
    return ESP_ERR_INVALID_RESPONSE;
}


