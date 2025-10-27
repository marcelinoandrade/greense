#include "atuadores.h"
#include <esp_log.h>
#include <esp_err.h>

static const char* TAG = "Atuadores";

// Handle do LED strip (privado)
static led_strip_handle_t led_strip;

static void leds_init(void) {
    // Configuração do LED strip
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_PIN,
        .max_leds = LED_STRIP_NUM_LEDS,
        .flags = {
            .invert_out = false
        }
    };

    // Configuração manual do RMT (alternativa à macro não disponível)
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,    // Fonte de clock padrão
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
        .mem_block_symbols = 0,            // Usar valor padrão
        .flags = {
            .with_dma = false              // Sem DMA
        }
    };

    ESP_LOGI(TAG, "Inicializando LED RGB...");
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    
    // Inicializa com azul fraco
    led_set_color(0, 0, 10);
}

void led_set_color(uint8_t red, uint8_t green, uint8_t blue) {
    ESP_LOGD(TAG, "Mudando cor para R:%d G:%d B:%d", red, green, blue);
    led_strip_set_pixel(led_strip, 0, red, green, blue);
    led_strip_refresh(led_strip);
}

void atuadores_init(void) {
    ESP_LOGI(TAG, "Inicializando atuadores...");
    leds_init();
}