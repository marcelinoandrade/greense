#include "bsp_gpio.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "BSP_GPIO"

esp_err_t bsp_gpio_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BSP_LED_PIN),
        .mode = GPIO_MODE_OUTPUT
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret == ESP_OK) {
        bsp_gpio_set_level(BSP_LED_PIN, 0);
    }
    return ret;
}

esp_err_t bsp_gpio_set_level(int pin, int level) {
    gpio_set_level(pin, level);
    return ESP_OK;
}

int bsp_gpio_get_level(int pin) {
    return gpio_get_level(pin);
}




