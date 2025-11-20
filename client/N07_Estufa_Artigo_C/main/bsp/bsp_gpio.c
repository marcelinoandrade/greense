#include "bsp_gpio.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "BSP_GPIO"

esp_err_t bsp_gpio_init(void)
{
    ESP_LOGI(TAG, "Inicializando GPIO...");
    // Configurações de GPIO podem ser adicionadas aqui
    return ESP_OK;
}

