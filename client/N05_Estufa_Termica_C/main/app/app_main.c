#include "app_main.h"
#include "app_thermal.h"
#include "app_http.h"
#include "app_time.h"
#include "../config.h"
#include "../bsp/bsp_gpio.h"
#include "../bsp/bsp_uart.h"
#include "../bsp/bsp_wifi.h"
#include "../gui/gui_led.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "freertos/task.h"
#include <time.h>

#define TAG "APP_MAIN"

void app_main(void) {
    ESP_LOGI(TAG, "Iniciando aplicação...");
    
    ESP_ERROR_CHECK(bsp_gpio_init());
    gui_led_blink(2, 200, 200);  // Pisca 2x na inicialização

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Criar task dedicada para controlar o LED
    xTaskCreate(gui_led_task, "led_task", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Task do LED criada");

    if (bsp_wifi_init() != ESP_OK) {
        ESP_LOGE(TAG, "Falha Wi-Fi. Reiniciando...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }

    // Aguardar um pouco para garantir que o Wi-Fi está estável
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Verificar conectividade inicial e atualizar LED
    ESP_LOGI(TAG, "Verificando conectividade inicial...");
    bool has_internet = app_http_check_connectivity("http://greense.com.br/");
    if (has_internet) {
        gui_led_set_state_wifi_connected_with_internet();
        ESP_LOGI(TAG, "✅ Internet disponível - LED aceso");
        
        // Sincronizar NTP quando tiver internet
        app_time_init();
    } else {
        gui_led_set_state_wifi_connected_no_internet();
        ESP_LOGW(TAG, "⚠️ Sem internet - LED piscando lento");
    }

    ESP_ERROR_CHECK(bsp_uart_init());

    float temps[APP_THERMAL_TOTAL];
    int ciclo = 0;
    int ciclo_verificacao = 0;  // Contador para verificar conectividade periodicamente

    while (1) {
        ciclo++;
        ciclo_verificacao++;
        
        // Verificar conectividade periodicamente
        if (ciclo_verificacao >= 5) {
            ciclo_verificacao = 0;
            ESP_LOGI(TAG, "Verificando conectividade...");
            bool has_internet = app_http_check_connectivity("http://greense.com.br/");
            if (has_internet) {
                gui_led_set_state_wifi_connected_with_internet();
                ESP_LOGI(TAG, "✅ Internet disponível");
                
                // Se NTP não está válido, tentar sincronizar novamente
                if (!app_time_is_valid()) {
                    ESP_LOGW(TAG, "⚠️ NTP não sincronizado, tentando novamente...");
                    app_time_init();
                }
            } else {
                gui_led_set_state_wifi_connected_no_internet();
                ESP_LOGW(TAG, "⚠️ Sem internet");
            }
        }
        
        // Verificar se NTP está sincronizado antes de calcular horários
        if (!app_time_is_valid()) {
            ESP_LOGW(TAG, "⚠️ NTP não sincronizado, aguardando sincronização...");
            vTaskDelay(pdMS_TO_TICKS(10000));  // Aguardar 10 segundos
            continue;  // Pular este ciclo
        }
        
        // Calcular próximo horário de aquisição
        time_t now = app_time_get_unix_timestamp();
        time_t next_acquisition = app_time_get_next_acquisition_time();
        
        // Validar se o cálculo foi bem-sucedido
        if (next_acquisition == 0) {
            ESP_LOGW(TAG, "⚠️ Não foi possível calcular próximo horário, aguardando NTP...");
            vTaskDelay(pdMS_TO_TICKS(10000));  // Aguardar 10 segundos
            continue;
        }
        
        time_t wait_seconds = next_acquisition - now;
        
        // Validar wait_seconds (não deve ser absurdo - mais de 24 horas é suspeito)
        if (wait_seconds < 0) {
            char duration_str[128];
            app_time_format_duration(-wait_seconds, duration_str, sizeof(duration_str));
            ESP_LOGW(TAG, "⚠️ Tempo de espera negativo (%s), recalculando...", duration_str);
            // Fazer aquisição imediatamente se o horário já passou
            wait_seconds = 0;
        } else if (wait_seconds > 86400) {  // Mais de 24 horas
            char duration_str[128];
            app_time_format_duration(wait_seconds, duration_str, sizeof(duration_str));
            ESP_LOGW(TAG, "⚠️ Tempo de espera muito grande (%s), recalculando...", duration_str);
            // Tentar sincronizar NTP novamente
            app_time_init();
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        if (wait_seconds > 0) {
            char time_str[64];
            char next_time_str[64];
            char duration_str[128];
            
            app_time_get_formatted(time_str, sizeof(time_str));
            
            // Formatar próximo horário de aquisição
            struct tm next_tm;
            localtime_r(&next_acquisition, &next_tm);
            strftime(next_time_str, sizeof(next_time_str), "%Y-%m-%d %H:%M:%S", &next_tm);
            
            // Formatar duração de forma legível
            app_time_format_duration(wait_seconds, duration_str, sizeof(duration_str));
            
            ESP_LOGI(TAG, "Hora atual: %s", time_str);
            ESP_LOGI(TAG, "Próxima aquisição: %s (em %s)", next_time_str, duration_str);
            
            // Aguardar até próximo horário (verificar a cada minuto)
            int minutes_to_wait = (wait_seconds + 59) / 60;  // Arredondar para cima
            for (int i = 0; i < minutes_to_wait; i++) {
                vTaskDelay(pdMS_TO_TICKS(60000));  // 1 minuto
                
                // Verificar se é hora de aquisição
                if (app_time_is_acquisition_time()) {
                    ESP_LOGI(TAG, "⏰ Hora de aquisição!");
                    break;
                }
            }
        }
        
        // Fazer aquisição
        ESP_LOGI(TAG, "Ciclo %d - Iniciando aquisição...", ciclo);
        if (app_thermal_capture_frame(temps, pdMS_TO_TICKS(5000))) {
            bool enviado = false;
            for (int tentativa = 0; tentativa < 3 && !enviado; tentativa++) {
                if (tentativa > 0) {
                    // Delay entre tentativas para evitar sobrecarga do servidor
                    ESP_LOGI(TAG, "Aguardando %dms antes da tentativa %d...", 3000, tentativa + 1);
                    vTaskDelay(pdMS_TO_TICKS(3000));
                }
                enviado = app_http_send_thermal_data(temps);
                if (enviado) {
                    gui_led_flash_success();  // Flash de sucesso
                    ESP_LOGI(TAG, "✅ Dados enviados com sucesso na tentativa %d", tentativa + 1);
                } else {
                    gui_led_flash_error();  // Flash de erro
                    ESP_LOGW(TAG, "❌ Falha no envio na tentativa %d", tentativa + 1);
                }
            }
            if (!enviado) {
                ESP_LOGE(TAG, "❌ Falha após todas as tentativas");
            }
        }
        
        // Pequeno delay após aquisição para evitar múltiplas aquisições no mesmo minuto
        vTaskDelay(pdMS_TO_TICKS(60000));  // 1 minuto
    }
}

