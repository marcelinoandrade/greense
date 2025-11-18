#include "gui_led.h"
#include "../bsp/bsp_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define GUI_LED_PIN BSP_LED_PIN

// Estados do LED
typedef enum {
    LED_STATE_WIFI_DISCONNECTED,
    LED_STATE_WIFI_CONNECTED_NO_INTERNET,
    LED_STATE_WIFI_CONNECTED_WITH_INTERNET,
    LED_STATE_SUCCESS_FLASH,
    LED_STATE_ERROR_FLASH
} led_state_t;

static led_state_t s_led_state = LED_STATE_WIFI_DISCONNECTED;

// Função auxiliar para piscar o LED
void gui_led_blink(int times, int on_ms, int off_ms) {
    for (int i = 0; i < times; i++) {
        bsp_gpio_set_level(GUI_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        bsp_gpio_set_level(GUI_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
}

// Funções para mudar o estado do LED
void gui_led_set_state_wifi_disconnected(void) {
    s_led_state = LED_STATE_WIFI_DISCONNECTED;
    ESP_LOGI("GUI_LED", "Estado LED: Wi-Fi desconectado (apagado)");
}

void gui_led_set_state_wifi_connected_no_internet(void) {
    s_led_state = LED_STATE_WIFI_CONNECTED_NO_INTERNET;
    ESP_LOGI("GUI_LED", "Estado LED: Wi-Fi OK, sem internet (piscando lento)");
}

void gui_led_set_state_wifi_connected_with_internet(void) {
    s_led_state = LED_STATE_WIFI_CONNECTED_WITH_INTERNET;
    ESP_LOGI("GUI_LED", "Estado LED: Wi-Fi OK, com internet (aceso)");
}

void gui_led_flash_success(void) {
    s_led_state = LED_STATE_SUCCESS_FLASH;
}

void gui_led_flash_error(void) {
    s_led_state = LED_STATE_ERROR_FLASH;
}

// Task dedicada que controla o LED continuamente
void gui_led_task(void *pvParameters) {
    led_state_t previous_state = LED_STATE_WIFI_DISCONNECTED;
    
    ESP_LOGI("GUI_LED", "Task do LED iniciada. Estado inicial: %d", s_led_state);
    
    while (1) {
        // Se o estado mudou, fazer um pequeno delay para evitar flicker
        if (s_led_state != previous_state) {
            ESP_LOGI("GUI_LED", "Estado LED mudou: %d -> %d", previous_state, s_led_state);
            vTaskDelay(pdMS_TO_TICKS(50));
            previous_state = s_led_state;
        }
        
        switch (s_led_state) {
            case LED_STATE_WIFI_DISCONNECTED:
                // LED apagado quando Wi-Fi desconectado
                bsp_gpio_set_level(GUI_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
                
            case LED_STATE_WIFI_CONNECTED_NO_INTERNET:
                // Pisca lentamente: 1s ligado, 1s desligado (Wi-Fi OK mas sem internet)
                bsp_gpio_set_level(GUI_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(1000));
                bsp_gpio_set_level(GUI_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
                
            case LED_STATE_WIFI_CONNECTED_WITH_INTERNET:
                // LED aceso (parado) quando tem internet
                bsp_gpio_set_level(GUI_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
                
            case LED_STATE_SUCCESS_FLASH:
                // 1 piscada rápida para indicar sucesso no envio
                bsp_gpio_set_level(GUI_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                bsp_gpio_set_level(GUI_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
                // Volta ao estado anterior (com internet)
                s_led_state = LED_STATE_WIFI_CONNECTED_WITH_INTERNET;
                previous_state = s_led_state;
                break;
                
            case LED_STATE_ERROR_FLASH:
                // 3 piscadas rápidas para indicar erro no envio
                bsp_gpio_set_level(GUI_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                bsp_gpio_set_level(GUI_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                bsp_gpio_set_level(GUI_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                bsp_gpio_set_level(GUI_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                bsp_gpio_set_level(GUI_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                bsp_gpio_set_level(GUI_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
                // Volta ao estado anterior (sem internet, pois houve erro)
                s_led_state = LED_STATE_WIFI_CONNECTED_NO_INTERNET;
                previous_state = s_led_state;
                break;
        }
    }
}
