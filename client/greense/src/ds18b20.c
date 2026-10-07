#include "ds18b20.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define CMD_SKIP_ROM      0xCC
#define CMD_CONVERT_T     0x44
#define CMD_READ_SCRATCH  0xBE

static const char *TAG = "DS18B20";
static gpio_num_t ds_gpio;
static portMUX_TYPE ds_mux = portMUX_INITIALIZER_UNLOCKED;

static void IRAM_ATTR ds_bus_low(void)
{
    gpio_set_level(ds_gpio, 0);
}

static void IRAM_ATTR ds_bus_release(void)
{
    gpio_set_level(ds_gpio, 1);
}

static int IRAM_ATTR ds_reset_pulse(void)
{
    ds_bus_low();
    esp_rom_delay_us(480);
    ds_bus_release();
    esp_rom_delay_us(70);
    int presence = gpio_get_level(ds_gpio) == 0;
    esp_rom_delay_us(410);
    return presence;
}

static void IRAM_ATTR ds_write_bit(int bit)
{
    ds_bus_low();
    if (bit) {
        esp_rom_delay_us(6);
        ds_bus_release();
        esp_rom_delay_us(64);
    } else {
        esp_rom_delay_us(60);
        ds_bus_release();
        esp_rom_delay_us(10);
    }
}

static int IRAM_ATTR ds_read_bit(void)
{
    ds_bus_low();
    esp_rom_delay_us(3);
    ds_bus_release();
    esp_rom_delay_us(10);
    int bit = gpio_get_level(ds_gpio);
    esp_rom_delay_us(53);
    return bit;
}

static void ds_write_byte(uint8_t byte)
{
    taskENTER_CRITICAL(&ds_mux);
    for (int i = 0; i < 8; i++) {
        ds_write_bit(byte & 0x01);
        byte >>= 1;
    }
    taskEXIT_CRITICAL(&ds_mux);
}

static uint8_t ds_read_byte(void)
{
    uint8_t byte = 0;
    taskENTER_CRITICAL(&ds_mux);
    for (int i = 0; i < 8; i++) {
        byte >>= 1;
        if (ds_read_bit()) {
            byte |= 0x80;
        }
    }
    taskEXIT_CRITICAL(&ds_mux);
    return byte;
}

static int ds_reset(void)
{
    int presence;
    taskENTER_CRITICAL(&ds_mux);
    presence = ds_reset_pulse();
    taskEXIT_CRITICAL(&ds_mux);
    return presence;
}

void ds18b20_init(gpio_num_t gpio)
{
    ds_gpio = gpio;
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

float ds18b20_read_temperature(gpio_num_t gpio)
{
    ds18b20_init(gpio);

    if (!ds_reset()) {
        ESP_LOGW(TAG, "sem presenca no GPIO %d", gpio);
        return -127.0f;
    }

    ds_write_byte(CMD_SKIP_ROM);
    ds_write_byte(CMD_CONVERT_T);
    vTaskDelay(pdMS_TO_TICKS(750));

    if (!ds_reset()) {
        ESP_LOGW(TAG, "sem presenca apos conversao no GPIO %d", gpio);
        return -127.0f;
    }

    ds_write_byte(CMD_SKIP_ROM);
    ds_write_byte(CMD_READ_SCRATCH);

    uint8_t temp_lsb = ds_read_byte();
    uint8_t temp_msb = ds_read_byte();
    int16_t raw_temp = (int16_t)((temp_msb << 8) | temp_lsb);
    return raw_temp / 16.0f;
}
