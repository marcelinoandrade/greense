#include "ds18b20.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"

/* Comandos 1-Wire */
#define CMD_SKIP_ROM      0xCC
#define CMD_CONVERT_T     0x44
#define CMD_READ_SCRATCH  0xBE

static gpio_num_t ds_gpio;

/* delay em microssegundos */
static void ds_delay_us(int us) {
    esp_rom_delay_us(us);
}

/* Pull-down e release do barramento 1-Wire (bit-bang) */

static void ds_write_bit(int bit) {
    gpio_set_direction(ds_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(ds_gpio, 0);
    ds_delay_us(2);

    if (bit) {
        gpio_set_level(ds_gpio, 1);
    }

    ds_delay_us(60);
    gpio_set_level(ds_gpio, 1);
    ds_delay_us(2);
}

static int ds_read_bit(void) {
    int r;
    gpio_set_direction(ds_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(ds_gpio, 0);
    ds_delay_us(2);

    gpio_set_direction(ds_gpio, GPIO_MODE_INPUT);
    ds_delay_us(8);

    r = gpio_get_level(ds_gpio);
    ds_delay_us(60);
    return r;
}

static void ds_write_byte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        ds_write_bit(data & 0x1);
        data >>= 1;
    }
}

static uint8_t ds_read_byte(void) {
    uint8_t r = 0;
    for (int i = 0; i < 8; i++) {
        r >>= 1;
        if (ds_read_bit()) {
            r |= 0x80;
        }
    }
    return r;
}

/* Reset 1-Wire. Retorna 1 se OK, 0 se erro (sem presença). */
static int ds_reset(void) {
    gpio_set_direction(ds_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(ds_gpio, 0);
    ds_delay_us(480);

    gpio_set_level(ds_gpio, 1);
    gpio_set_direction(ds_gpio, GPIO_MODE_INPUT);
    ds_delay_us(70);

    int presence = !gpio_get_level(ds_gpio);
    ds_delay_us(410);

    return presence;
}

void ds18b20_init(gpio_num_t gpio) {
    ds_gpio = gpio;
    gpio_set_direction(ds_gpio, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(ds_gpio, GPIO_PULLUP_ONLY);
}

/* Lê temperatura do DS18B20 em °C.
 * Retorna -127.0 se falhar.
 */
float ds18b20_read_temperature(gpio_num_t gpio) {
    (void)gpio; /* usamos ds_gpio interno */

    if (!ds_reset()) {
        return -127.0f;
    }

    ds_write_byte(CMD_SKIP_ROM);
    ds_write_byte(CMD_CONVERT_T);

    /* aguarda conversão */
    vTaskDelay(pdMS_TO_TICKS(750));

    if (!ds_reset()) {
        return -127.0f;
    }

    ds_write_byte(CMD_SKIP_ROM);
    ds_write_byte(CMD_READ_SCRATCH);

    uint8_t temp_lsb = ds_read_byte();
    uint8_t temp_msb = ds_read_byte();
    int16_t raw_temp = (temp_msb << 8) | temp_lsb;

    return (float)raw_temp / 16.0f;
}
