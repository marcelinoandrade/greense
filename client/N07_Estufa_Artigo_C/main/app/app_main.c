#include "app_main.h"
#include "app_http.h"
#include "../config.h"
#include "../bsp/bsp_gpio.h"
#include "../bsp/bsp_wifi.h"
#include "../bsp/bsp_camera.h"
#include "../bsp/bsp_sdcard.h"
#include "../bsp/bsp_pins.h"
#include "../gui/gui_led.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <time.h>
#include <string.h>

#define TAG "APP_MAIN"

// Task periódica para captura e envio de imagens (similar à ESP32-CAM)
static void task_envia_foto_periodicamente(void *pvParameter) {
    while (true) {
        // Verifica conexão Wi-Fi
        if (bsp_wifi_is_connected()) {
            gui_led_set_state_wifi_connected();  // LED azul quando conectado
        } else {
            gui_led_set_state_wifi_disconnected();  // LED vermelho quando desconectado
            ESP_LOGW(TAG, "Sem conexão Wi-Fi. Aguardando reconexão...");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        // Ativa flash LED (se disponível)
        if (CAM_FLASH_GPIO >= 0) {
            gpio_set_level(CAM_FLASH_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        // Captura imagem da câmera
        camera_fb_t* fb = bsp_camera_capture();
        vTaskDelay(pdMS_TO_TICKS(200));  // Atraso opcional

        // Desativa flash LED
        if (CAM_FLASH_GPIO >= 0) {
            gpio_set_level(CAM_FLASH_GPIO, 0);
        }

        if (fb) {
            // Envia imagem via HTTPS
            esp_err_t err = app_http_send_data(CAMERA_UPLOAD_URL, 
                                               fb->buf, 
                                               fb->len,
                                               "image/jpeg");
            
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "✅ Imagem enviada com sucesso");
                gui_led_flash_success();
            } else {
                ESP_LOGE(TAG, "❌ Erro ao enviar imagem");
                gui_led_flash_error();
            }

            // Salva imagem no SD Card (se disponível)
            if (bsp_sdcard_is_mounted()) {
                char filename[64];
                time_t now;
                struct tm timeinfo;
                
                time(&now);
                if (now < 1609459200) {  // antes de 2021-01-01 (sem NTP)
                    snprintf(filename, sizeof(filename), "img_%lu.jpg", esp_log_timestamp());
                } else {
                    localtime_r(&now, &timeinfo);
                    strftime(filename, sizeof(filename), "%Y%m%d_%H%M%S.jpg", &timeinfo);
                }
                
                esp_err_t sd_err = bsp_sdcard_save_file(filename, fb->buf, fb->len);
                if (sd_err == ESP_OK) {
                    ESP_LOGI(TAG, "✅ Imagem salva no SD Card: %s", filename);
                } else {
                    ESP_LOGW(TAG, "⚠️ Falha ao salvar no SD Card");
                }
            }

            // Libera buffer da câmera
            bsp_camera_release(fb);
        } else {
            ESP_LOGE(TAG, "Erro ao capturar imagem");
            gui_led_flash_error();
        }

        // Aguarda antes da próxima captura (60 segundos, como na ESP32-CAM)
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Iniciando N07_Estufa_Artigo...");
    
    // Inicializa NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_LOGI(TAG, "NVS inicializado com sucesso");
    
    // Inicializa GPIO
    ESP_ERROR_CHECK(bsp_gpio_init());
    
    // Configura Flash LED (se disponível)
    if (CAM_FLASH_GPIO >= 0) {
        gpio_reset_pin(CAM_FLASH_GPIO);
        gpio_set_direction(CAM_FLASH_GPIO, GPIO_MODE_OUTPUT);
        gpio_set_level(CAM_FLASH_GPIO, 0);
    }
    
    // Inicializa GUI (LED)
    gui_led_init();
    gui_led_blink(2, 200, 200);  // Pisca 2x na inicialização
    
    // Inicializa SD Card (opcional - continua mesmo se falhar)
    if (bsp_sdcard_init() != ESP_OK) {
        ESP_LOGW(TAG, "SD Card não disponível. Continuando sem armazenamento local.");
    }
    
    // Inicializa câmera
    if (bsp_camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "Falha na inicialização da câmera. Reiniciando...");
        gui_led_flash_error();
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    ESP_LOGI(TAG, "Câmera inicializada com sucesso");
    
    // Inicializa Wi-Fi (igual ao projeto N01/N02 - aguarda conexão)
    bsp_wifi_init();
    
    // Cria task periódica para captura e envio de imagens
    BaseType_t task_result = xTaskCreate(task_envia_foto_periodicamente, 
                                          "envia_foto_task", 
                                          8192, 
                                          NULL, 
                                          5, 
                                          NULL);
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar task de captura de imagens");
        esp_restart();
    }
    ESP_LOGI(TAG, "Task de captura e envio de imagens criada");
    
    // Loop principal (monitoramento - igual ao projeto N01/N02)
    while (true) {
        // Verifica conexão Wi-Fi
        if (!bsp_wifi_is_connected()) {
            gui_led_set_state_wifi_disconnected();  // LED vermelho quando desconectado
            ESP_LOGW(TAG, "Wi-Fi está desconectado. Tentando reconectar...");
            esp_wifi_disconnect();
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            esp_wifi_connect();
        } else {
            ESP_LOGI(TAG, "Wi-Fi está conectado.");
            gui_led_set_state_wifi_connected();  // LED azul quando conectado
        }
        
        vTaskDelay(5000 / portTICK_PERIOD_MS); // Aguarda 5 segundos (igual ao N01/N02)
    }
}

