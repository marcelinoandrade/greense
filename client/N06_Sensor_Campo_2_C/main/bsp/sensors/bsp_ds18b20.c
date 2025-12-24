#include "bsp_ds18b20.h"
#include "../board.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include <stdint.h>

static const char *TAG = "BSP_DS18B20";

/* Comandos 1-Wire */
#define CMD_SKIP_ROM      0xCC
#define CMD_CONVERT_T     0x44
#define CMD_READ_SCRATCH  0xBE

static gpio_num_t ds_gpio = BSP_GPIO_DS18B20;
static bool initialized = false;

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

esp_err_t ds18b20_bsp_init(void)
{
    if (initialized) {
        return ESP_OK;
    }
    
    ds_gpio = BSP_GPIO_DS18B20;
    gpio_set_direction(ds_gpio, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(ds_gpio, GPIO_PULLUP_ONLY);
    
    initialized = true;
    ESP_LOGI(TAG, "DS18B20 inicializado no GPIO %d", ds_gpio);
    return ESP_OK;
}

float ds18b20_bsp_read_temperature(void)
{
    // Simulação: retorna temperatura de solo aleatória entre 20 e 30 °C.
    // Mantém interface para não quebrar camadas superiores.
    uint32_t r = esp_random();
    float temp = 20.0f + (r % 1000) / 100.0f; // 20.00 a 29.99
    // Log em nível INFO para visualizar facilmente a temperatura simulada.
    ESP_LOGI(TAG, "Temp solo simulada: %.2f C", temp);
    return temp;
}

