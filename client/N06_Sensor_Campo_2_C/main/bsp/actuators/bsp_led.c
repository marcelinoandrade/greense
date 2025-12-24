#include "bsp_led.h"
#include "../board.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdbool.h>

static const char *TAG = "BSP_LED";
static bool initialized = false;

esp_err_t led_bsp_init(void)
{
    if (initialized) {
        return ESP_OK;
    }
    
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << BSP_GPIO_LED_STATUS,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao configurar LED GPIO");
        return err;
    }
    
    led_bsp_off();
    initialized = true;
    ESP_LOGI(TAG, "LED inicializado no GPIO %d", BSP_GPIO_LED_STATUS);
    return ESP_OK;
}

void led_bsp_on(void)  { gpio_set_level(BSP_GPIO_LED_STATUS, 1); }
void led_bsp_off(void) { gpio_set_level(BSP_GPIO_LED_STATUS, 0); }
void led_bsp_toggle(void) {
    int level = gpio_get_level(BSP_GPIO_LED_STATUS);
    gpio_set_level(BSP_GPIO_LED_STATUS, !level);
}

