#include "app_main.h"
#include "app_http.h"
#include "app_thermal.h"
#include "app_time.h"
#include "../config.h"
#include "../bsp/bsp_gpio.h"
#include "../bsp/bsp_wifi.h"
#include "../bsp/bsp_camera.h"
#include "../bsp/bsp_sdcard.h"
#include "../bsp/bsp_uart.h"
#include "../bsp/bsp_pins.h"
#include "../gui/gui_led.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_heap_caps.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>

#define TAG "APP_MAIN"

// Declaração da task de captura térmica
static void task_captura_termica(void *pvParameter);

// Função auxiliar para verificar se é hora de capturar baseado no agendamento
// schedule: array de schedule_time_t com formato [H1:M1, H2:M2, ..., Hn:Mn]
static bool should_capture_now(const schedule_time_t schedule[], int schedule_size) {
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    int current_hour = timeinfo.tm_hour;
    int current_minute = timeinfo.tm_min;
    
    // Verifica se o horário atual está no agendamento
    for (int i = 0; i < schedule_size; i++) {
        if (schedule[i].hour == current_hour && schedule[i].minute == current_minute) {
            return true;
        }
    }
    return false;
}

// Task periódica para captura e envio de imagens (agendamento baseado em horários)
static void task_envia_foto_periodicamente(void *pvParameter) {
    int last_captured_hour = -1;
    int last_captured_minute = -1;
    int log_counter = 0;  // Contador para logs periódicos
    
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

        // Log periódico da próxima aquisição (a cada 5 verificações = ~2.5 minutos)
        if (app_time_is_valid() && (log_counter % 5 == 0)) {
            time_t now = app_time_get_unix_timestamp();
            time_t next_acquisition = app_time_get_next_acquisition_time(
                camera_visual_schedule, CAMERA_VISUAL_SCHEDULE_SIZE);
            
            if (next_acquisition > 0) {
                char time_str[64];
                char next_time_str[64];
                char duration_str[128];
                
                app_time_get_formatted(time_str, sizeof(time_str));
                
                struct tm next_tm;
                localtime_r(&next_acquisition, &next_tm);
                strftime(next_time_str, sizeof(next_time_str), "%Y-%m-%d %H:%M:%S", &next_tm);
                
                time_t wait_seconds = next_acquisition - now;
                if (wait_seconds > 0) {
                    app_time_format_duration(wait_seconds, duration_str, sizeof(duration_str));
                    ESP_LOGI(TAG, "📸 Câmera Visual - Hora atual: %s | Próxima aquisição: %s (em %s)", 
                             time_str, next_time_str, duration_str);
                }
            }
        }
        log_counter++;

        // Verifica se é hora de capturar baseado no agendamento
        bool should_capture = should_capture_now(camera_visual_schedule, CAMERA_VISUAL_SCHEDULE_SIZE);
        
        if (should_capture) {
            // Obtém horário atual para log
            time_t now = time(NULL);
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);
            
            // Verifica se já capturou neste horário específico
            if (timeinfo.tm_hour != last_captured_hour || timeinfo.tm_min != last_captured_minute) {
                ESP_LOGI(TAG, "📸 Horário agendado para captura visual: %02d:%02d", 
                         timeinfo.tm_hour, timeinfo.tm_min);
                
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
                        ESP_LOGI(TAG, "✅ Imagem visual enviada com sucesso");
                        gui_led_flash_success();
                    } else {
                        ESP_LOGE(TAG, "❌ Erro ao enviar imagem visual");
                        gui_led_flash_error();
                    }

                    // Salva imagem no SD Card (se disponível)
                    if (bsp_sdcard_is_mounted()) {
                        // Formato 8.3 simples: IMG#####.JPG (8 caracteres no nome + .JPG)
                        // Usa timestamp para garantir unicidade
                        char filename[16];
                        time_t now = time(NULL);
                        // Formato: IMG + 5 dígitos do timestamp = 8 caracteres
                        snprintf(filename, sizeof(filename), "IMG%05lu.JPG", (unsigned long)now % 100000);
                        
                        esp_err_t sd_err = bsp_sdcard_save_file(filename, fb->buf, fb->len);
                        if (sd_err == ESP_OK) {
                            ESP_LOGI(TAG, "✅ Imagem visual salva no SD Card: %s", filename);
                        } else {
                            ESP_LOGW(TAG, "⚠️ Falha ao salvar imagem visual no SD Card");
                        }
                    }

                    // Libera buffer da câmera
                    bsp_camera_release(fb);
                } else {
                    ESP_LOGE(TAG, "Erro ao capturar imagem visual");
                    gui_led_flash_error();
                }
                
                // Atualiza último horário capturado
                last_captured_hour = timeinfo.tm_hour;
                last_captured_minute = timeinfo.tm_min;
            }
        }

        // Aguarda 30 segundos antes de verificar novamente
        vTaskDelay(30000 / portTICK_PERIOD_MS);
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
    
    // Inicializa UART para câmera térmica MLX90640
    if (bsp_uart_init() != ESP_OK) {
        ESP_LOGW(TAG, "UART não inicializado. Câmera térmica não disponível.");
    } else {
        ESP_LOGI(TAG, "UART inicializado para câmera térmica MLX90640");
        // Aguarda câmera térmica inicializar (alguns módulos precisam de tempo)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Inicializa Wi-Fi (igual ao projeto N01/N02 - aguarda conexão)
    bsp_wifi_init();
    
    // Inicializa NTP para sincronização de tempo
    if (app_time_init()) {
        ESP_LOGI(TAG, "NTP sincronizado. Tempo correto disponível.");
    } else {
        ESP_LOGW(TAG, "NTP não sincronizado. Usando tempo do sistema.");
    }
    
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
    
    // Task para captura e exibição de dados térmicos na serial
    xTaskCreate(task_captura_termica, 
                "termica_task", 
                16384,  // Stack maior para segurança
                NULL, 
                4, 
                NULL);
    ESP_LOGI(TAG, "Task de captura térmica criada");
    
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

// Task para captura e exibição de dados térmicos (agendamento baseado em horários)
static void task_captura_termica(void *pvParameter) {
    // Array temporário na stack (pequeno, apenas 1 frame)
    float temps[APP_THERMAL_TOTAL];
    
    // Buffer grande alocado no heap/PSRAM (não na stack!)
    float *temps_buffer = (float *)heap_caps_malloc(
        APP_THERMAL_TOTAL * THERMAL_SAVE_INTERVAL * sizeof(float),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    if (!temps_buffer) {
        ESP_LOGE(TAG, "Falha ao alocar buffer térmico. Reiniciando...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    
    int contador_aquisicoes = 0;
    int buffer_index = 0;
    int last_captured_hour = -1;
    int last_captured_minute = -1;
    int log_counter = 0;  // Contador para logs periódicos
    
    ESP_LOGI(TAG, "Task de captura térmica iniciada");
    ESP_LOGI(TAG, "Configuração: salvando a cada %d registros térmicos", THERMAL_SAVE_INTERVAL);
    
    while (true) {
        // Log periódico da próxima aquisição (a cada 5 verificações = ~2.5 minutos)
        if (app_time_is_valid() && (log_counter % 5 == 0)) {
            time_t now = app_time_get_unix_timestamp();
            time_t next_acquisition = app_time_get_next_acquisition_time(
                camera_thermal_schedule, CAMERA_THERMAL_SCHEDULE_SIZE);
            
            if (next_acquisition > 0) {
                char time_str[64];
                char next_time_str[64];
                char duration_str[128];
                
                app_time_get_formatted(time_str, sizeof(time_str));
                
                struct tm next_tm;
                localtime_r(&next_acquisition, &next_tm);
                strftime(next_time_str, sizeof(next_time_str), "%Y-%m-%d %H:%M:%S", &next_tm);
                
                time_t wait_seconds = next_acquisition - now;
                if (wait_seconds > 0) {
                    app_time_format_duration(wait_seconds, duration_str, sizeof(duration_str));
                    ESP_LOGI(TAG, "🌡️ Câmera Térmica - Hora atual: %s | Próxima aquisição: %s (em %s)", 
                             time_str, next_time_str, duration_str);
                }
            }
        }
        log_counter++;

        // Verifica se é hora de capturar baseado no agendamento
        bool should_capture = should_capture_now(camera_thermal_schedule, CAMERA_THERMAL_SCHEDULE_SIZE);
        
        if (should_capture) {
            // Obtém horário atual para log
            time_t now = time(NULL);
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);
            
            // Verifica se já capturou neste horário específico
            if (timeinfo.tm_hour != last_captured_hour || timeinfo.tm_min != last_captured_minute) {
                ESP_LOGI(TAG, "🌡️ Horário agendado para captura térmica: %02d:%02d", 
                         timeinfo.tm_hour, timeinfo.tm_min);
                
                ESP_LOGI(TAG, "Tentando capturar frame térmico...");
                
                if (app_thermal_capture_frame(temps, pdMS_TO_TICKS(5000))) {
                    contador_aquisicoes++;
                    ESP_LOGI(TAG, "✅ Frame térmico capturado com sucesso! (Aquisição #%d)", contador_aquisicoes);
                    
                    // Calcula estatísticas (dados já estão em RAM interna - array temps na stack)
                    float tmin = temps[0], tmax = temps[0], tavg = 0.0f;
                    for (int i = 0; i < APP_THERMAL_TOTAL; i++) {
                        if (temps[i] < tmin) tmin = temps[i];
                        if (temps[i] > tmax) tmax = temps[i];
                        tavg += temps[i];
                    }
                    tavg /= APP_THERMAL_TOTAL;
                    
                    // Exibe apenas estatísticas (sem matriz completa)
                    ESP_LOGI(TAG, "=== Estatísticas Térmicas ===");
                    ESP_LOGI(TAG, "Temperatura Mínima: %.2f°C", tmin);
                    ESP_LOGI(TAG, "Temperatura Máxima: %.2f°C", tmax);
                    ESP_LOGI(TAG, "Temperatura Média:  %.2f°C", tavg);
                    ESP_LOGI(TAG, "================================");
                    
                    // Acumula dados no buffer (copia de RAM interna para PSRAM)
                    memcpy(&temps_buffer[buffer_index * APP_THERMAL_TOTAL], temps, 
                           sizeof(float) * APP_THERMAL_TOTAL);
                    buffer_index++;
                    
                    // A cada THERMAL_SAVE_INTERVAL aquisições, salva arquivo binário no SD card
                    if (buffer_index >= THERMAL_SAVE_INTERVAL) {
                        if (bsp_sdcard_is_mounted()) {
                            // Formato 8.3 simples: THM#####.BIN (8 caracteres no nome + .BIN)
                            char filename[16];
                            time_t now = time(NULL);
                            // Formato: THM + 5 dígitos do timestamp = 8 caracteres
                            snprintf(filename, sizeof(filename), "THM%05lu.BIN", (unsigned long)now % 100000);
                            
                            // Salva dados binários acumulados (array de floats)
                            size_t total_size = sizeof(float) * APP_THERMAL_TOTAL * THERMAL_SAVE_INTERVAL;
                            esp_err_t ret = bsp_sdcard_save_file(filename, (const uint8_t*)temps_buffer, total_size);
                            if (ret == ESP_OK) {
                                ESP_LOGI(TAG, "✅ Arquivo térmico salvo: %s (%d registros, %d bytes)", 
                                         filename, THERMAL_SAVE_INTERVAL, (int)total_size);
                            } else {
                                ESP_LOGE(TAG, "❌ Falha ao salvar arquivo térmico: %s", filename);
                            }
                        } else {
                            ESP_LOGW(TAG, "⚠️ SD Card não disponível. Não foi possível salvar arquivo térmico.");
                        }
                        
                        // Reseta o buffer
                        buffer_index = 0;
                    }
                    
                } else {
                    ESP_LOGW(TAG, "❌ Falha ao capturar frame térmico");
                }
                
                // Atualiza último horário capturado
                last_captured_hour = timeinfo.tm_hour;
                last_captured_minute = timeinfo.tm_min;
            }
        }
        
        // Aguarda 30 segundos antes de verificar novamente
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
    
    // Nunca chega aqui, mas por segurança:
    free(temps_buffer);
}

