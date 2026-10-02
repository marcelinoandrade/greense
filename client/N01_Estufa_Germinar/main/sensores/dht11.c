#include "dht11.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DHT11";

static int wait_level(gpio_num_t pin, int level, int timeout_us)
{
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(pin) != level) {
        if ((esp_timer_get_time() - start) > timeout_us) {
            return -1;
        }
    }
    return (int)(esp_timer_get_time() - start);
}

void dht11_init(gpio_num_t gpio)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(gpio, 1);
}

bool dht11_read(gpio_num_t gpio, float *temperature, float *humidity)
{
    uint8_t data[5] = {0};

    gpio_set_level(gpio, 0);
    esp_rom_delay_us(20000);
    gpio_set_level(gpio, 1);
    esp_rom_delay_us(30);

    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    taskENTER_CRITICAL(&mux);

    bool ok = wait_level(gpio, 0, 200) >= 0
        && wait_level(gpio, 1, 200) >= 0
        && wait_level(gpio, 0, 200) >= 0;

    for (int i = 0; ok && i < 40; i++) {
        if (wait_level(gpio, 1, 100) < 0) {
            ok = false;
            break;
        }
        int high_us = wait_level(gpio, 0, 150);
        if (high_us < 0) {
            ok = false;
            break;
        }
        data[i / 8] = (uint8_t)((data[i / 8] << 1) | (high_us > 40 ? 1 : 0));
    }

    taskEXIT_CRITICAL(&mux);

    if (!ok) {
        ESP_LOGW(TAG, "sem resposta no GPIO %d", gpio);
        return false;
    }

    uint8_t sum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (sum != data[4]) {
        ESP_LOGW(TAG, "checksum invalido: %u != %u", sum, data[4]);
        return false;
    }

    if (humidity) {
        *humidity = data[0] + (data[1] * 0.1f);
    }
    if (temperature) {
        float temp = data[2] + ((data[3] & 0x7F) * 0.1f);
        if (data[3] & 0x80) {
            temp = -temp;
        }
        *temperature = temp;
    }
    return true;
}
