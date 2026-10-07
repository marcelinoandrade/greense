#include "atuadores.h"
#include "greense.h"
#include <led_strip.h>
#include <esp_log.h>
#include <esp_err.h>

static const char *TAG = "Atuadores";

static led_strip_handle_t led_strip;

static void leds_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = GREENSE_LED_GPIO,
        .max_leds = GREENSE_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
        .flags = {
            .invert_out = false
        }
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 0,
        .flags = {
            .with_dma = false
        }
    };

    ESP_LOGI(TAG, "LED RGB no GPIO%d (%s)", GREENSE_LED_GPIO, GREENSE_PLACA_NOME);
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_set_color(0, 0, 10);
}

void led_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    ESP_LOGD(TAG, "Mudando cor para R:%d G:%d B:%d", red, green, blue);
    led_strip_set_pixel(led_strip, 0, red, green, blue);
    led_strip_refresh(led_strip);
}

void atuadores_init(void)
{
    ESP_LOGI(TAG, "Inicializando atuadores...");
    leds_init();
}
