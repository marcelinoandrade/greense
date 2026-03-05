#include "gui_led.h"
#include "../bsp/bsp_pins.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "GUI_LED"

// Configuração do LED WS2812
#define LED_STRIP_PIN LED_STATUS_GPIO  // GPIO48 para ESP32-S3
#define LED_STRIP_NUM_LEDS 1

// Handle do LED strip (privado)
static led_strip_handle_t led_strip = NULL;

/**
 * @brief Define a cor do LED RGB
 */
void gui_led_set_color(uint8_t red, uint8_t green, uint8_t blue) {
    if (led_strip == NULL) {
        ESP_LOGW(TAG, "LED não inicializado");
        return;
    }
    
    ESP_LOGD(TAG, "Mudando cor para R:%d G:%d B:%d", red, green, blue);
    led_strip_set_pixel(led_strip, 0, red, green, blue);
    led_strip_refresh(led_strip);
}

void gui_led_init(void)
{
    ESP_LOGI(TAG, "Inicializando LED RGB WS2812 no GPIO %d...", LED_STRIP_PIN);
    
    // Configuração do LED strip
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_PIN,
        .max_leds = LED_STRIP_NUM_LEDS,
        .flags = {
            .invert_out = false
        }
    };

    // Configuração do RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,    // Fonte de clock padrão
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
        .mem_block_symbols = 0,            // Usar valor padrão
        .flags = {
            .with_dma = false              // Sem DMA
        }
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar LED strip: %s", esp_err_to_name(err));
        led_strip = NULL;
        return;
    }
    
    // Inicializa com azul fraco (como no projeto N02)
    gui_led_set_color(0, 0, 10);
    ESP_LOGI(TAG, "LED RGB inicializado com sucesso");
}

void gui_led_set_state_wifi_disconnected(void)
{
    // Vermelho quando Wi-Fi desconectado (como no projeto N02)
    gui_led_set_color(10, 0, 0);
}

void gui_led_set_state_wifi_connected(void)
{
    // Azul quando Wi-Fi conectado (como no projeto N02)
    gui_led_set_color(0, 0, 10);
}

void gui_led_flash_success(void)
{
    // Pisca verde 3 vezes
    for (int i = 0; i < 3; i++) {
        gui_led_set_color(0, 10, 0);  // Verde
        vTaskDelay(pdMS_TO_TICKS(100));
        gui_led_set_color(0, 0, 0);   // Apagado
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    // Retorna ao estado anterior (azul se conectado)
    gui_led_set_state_wifi_connected();
}

void gui_led_flash_error(void)
{
    // Pisca vermelho 5 vezes
    for (int i = 0; i < 5; i++) {
        gui_led_set_color(10, 0, 0);  // Vermelho
        vTaskDelay(pdMS_TO_TICKS(50));
        gui_led_set_color(0, 0, 0);   // Apagado
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    // Retorna ao estado anterior
    gui_led_set_state_wifi_disconnected();
}

void gui_led_blink(int times, int on_ms, int off_ms)
{
    for (int i = 0; i < times; i++) {
        gui_led_set_color(10, 10, 10);  // Branco
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        gui_led_set_color(0, 0, 0);     // Apagado
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
}
