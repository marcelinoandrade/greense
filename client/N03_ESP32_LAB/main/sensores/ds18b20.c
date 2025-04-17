#include "ds18b20.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"

// Comandos 1-Wire
#define CMD_SKIP_ROM      0xCC
#define CMD_CONVERT_T     0x44
#define CMD_READ_SCRATCH  0xBE

static gpio_num_t ds_gpio;

static void ds_delay_us(int us) {
    esp_rom_delay_us(us);
}

static void ds_write_bit(int bit) {
    gpio_set_direction(ds_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(ds_gpio, 0);
    if (bit) {
        ds_delay_us(5);
        gpio_set_level(ds_gpio, 1);
        ds_delay_us(60);
    } else {
        ds_delay_us(60);
        gpio_set_level(ds_gpio, 1);
        ds_delay_us(5);
    }
}

static int ds_read_bit(void) {
    int bit;
    gpio_set_direction(ds_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(ds_gpio, 0);
    ds_delay_us(2);
    gpio_set_direction(ds_gpio, GPIO_MODE_INPUT);
    ds_delay_us(8);
    bit = gpio_get_level(ds_gpio);
    ds_delay_us(50);
    return bit;
}

static void ds_write_byte(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        ds_write_bit(byte & 0x01);
        byte >>= 1;
    }
}

static uint8_t ds_read_byte(void) {
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        byte >>= 1;
        if (ds_read_bit()) {
            byte |= 0x80;
        }
    }
    return byte;
}

static int ds_reset(void) {
    gpio_set_direction(ds_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(ds_gpio, 0);
    ds_delay_us(480);
    gpio_set_direction(ds_gpio, GPIO_MODE_INPUT);
    ds_delay_us(70);
    int presence = gpio_get_level(ds_gpio);
    ds_delay_us(410);
    return !presence;
}

void ds18b20_init(gpio_num_t gpio) {
    ds_gpio = gpio;
    gpio_set_direction(ds_gpio, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_pull_mode(ds_gpio, GPIO_PULLUP_ONLY);
}

float ds18b20_read_temperature(gpio_num_t gpio) {
    ds18b20_init(gpio);

    if (!ds_reset()) return -127.0;

    ds_write_byte(CMD_SKIP_ROM);
    ds_write_byte(CMD_CONVERT_T);

    vTaskDelay(pdMS_TO_TICKS(750)); // tempo de conversão

    if (!ds_reset()) return -127.0;

    ds_write_byte(CMD_SKIP_ROM);
    ds_write_byte(CMD_READ_SCRATCH);

    uint8_t temp_lsb = ds_read_byte();
    uint8_t temp_msb = ds_read_byte();
    int16_t raw_temp = (temp_msb << 8) | temp_lsb;

    return raw_temp / 16.0;
}
