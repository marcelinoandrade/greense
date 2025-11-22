#include "app_main.h"
#include "app_http.h"
#include "app_thermal.h"
#include "app_time.h"
#include "../config.h"
#include "../bsp/bsp_gpio.h"
#include "../bsp/bsp_wifi.h"
#include "../bsp/bsp_camera.h"
#include "../bsp/bsp_sdcard.h"
#include "../bsp/bsp_spiffs.h"
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
#include "esp_task_wdt.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define TAG "APP_MAIN"
#define WATCHDOG_TIMEOUT_SEC 30  // ✅ CORREÇÃO 3: Timeout de 30s para watchdog

// Declaração das tasks térmicas
static void task_captura_termica(void *pvParameter);
static void task_envio_termica(void *pvParameter);

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
    // ✅ CORREÇÃO 3: Adiciona task ao watchdog
    esp_task_wdt_add(NULL);
    
    int last_captured_hour = -1;
    int last_captured_minute = -1;
    int log_counter = 0;  // Contador para logs periódicos
    
    while (true) {
        // ✅ CORREÇÃO 3: Alimenta watchdog periodicamente
        esp_task_wdt_reset();
        // Verifica conexão Wi-Fi
        if (bsp_wifi_is_connected()) {
            gui_led_set_state_wifi_connected();  // LED azul quando conectado
        } else {
            gui_led_set_state_wifi_disconnected();  // LED vermelho quando desconectado
            ESP_LOGW(TAG, "Sem conexão Wi-Fi. Aguardando reconexão...");
            // ✅ CORREÇÃO 3: Divide delay em múltiplos delays menores com resets
            // Watchdog timeout é 5s, então dividimos 5s em 2 delays de ~2.5s cada
            for (int i = 0; i < 2; i++) {
                esp_task_wdt_reset();
                vTaskDelay(2500 / portTICK_PERIOD_MS);  // 2.5s * 2 = 5s
            }
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
            // ✅ CORREÇÃO 3: Reset watchdog antes de operação longa (envio HTTP pode demorar)
            esp_task_wdt_reset();
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
        // ✅ CORREÇÃO 3: Divide delay longo em múltiplos delays menores com resets
        // Watchdog timeout é 5s, então dividimos 30s em 8 delays de ~4s cada
        for (int i = 0; i < 8; i++) {
            esp_task_wdt_reset();
            vTaskDelay(3750 / portTICK_PERIOD_MS);  // ~3.75s * 8 = 30s
        }
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
    
    // Inicializa SPIFFS para buffer de dados parciais térmicos
    if (bsp_spiffs_init() != ESP_OK) {
        ESP_LOGW(TAG, "SPIFFS não inicializado. Parciais não serão protegidas contra reboot.");
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
    
    // ✅ CORREÇÃO 3: Watchdog já é inicializado automaticamente pelo ESP-IDF
    // Não precisamos inicializar manualmente - apenas adicionar tasks
    ESP_LOGI(TAG, "✅ Watchdog timer gerenciado pelo ESP-IDF (timeout: 5s)");
    
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
    
    // Task para envio de dados térmicos pendentes para servidor
    xTaskCreate(task_envio_termica, 
                "termica_send_task", 
                16384,  // Stack maior para JSON e buffers
                NULL, 
                3,  // Prioridade menor que captura
                NULL);
    ESP_LOGI(TAG, "Task de envio térmico criada");
    
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
        
        // Loop principal não precisa resetar watchdog (não é task crítica)
        // As tasks críticas já estão monitoradas pelo watchdog
        vTaskDelay(5000 / portTICK_PERIOD_MS); // Aguarda 5 segundos (igual ao N01/N02)
    }
}

// Task para captura e exibição de dados térmicos (agendamento baseado em horários)
static void task_captura_termica(void *pvParameter) {
    // ✅ CORREÇÃO 3: Adiciona task ao watchdog
    esp_task_wdt_add(NULL);
    
    // Array temporário na stack (pequeno, apenas 1 frame)
    float temps[APP_THERMAL_TOTAL];
    
    int contador_aquisicoes = 0;
    int last_captured_hour = -1;
    int last_captured_minute = -1;
    int log_counter = 0;  // Contador para logs periódicos
    
    ESP_LOGI(TAG, "Task de captura térmica iniciada");
    ESP_LOGI(TAG, "Configuração: arquivo acumulativo SPIFFS (limite: %d bytes)", (int)THERMAL_SPIFFS_MAX_SIZE);
    
    // Verifica se há dados acumulativos na SPIFFS que precisam ser migrados
    if (bsp_spiffs_is_mounted()) {
        size_t accum_size = bsp_spiffs_get_thermal_file_size();
        if (accum_size > 0) {
            ESP_LOGI(TAG, "📦 Encontrados %d bytes no arquivo acumulativo SPIFFS (será migrado quando atingir limite)", (int)accum_size);
        }
    }
    
    while (true) {
        // ✅ CORREÇÃO 3: Alimenta watchdog periodicamente
        esp_task_wdt_reset();
        
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
        
        // ✅ CORREÇÃO 3: Reset watchdog antes de captura (pode demorar até 5s)
        esp_task_wdt_reset();
        if (app_thermal_capture_frame(temps, pdMS_TO_TICKS(5000))) {
            contador_aquisicoes++;
            ESP_LOGI(TAG, "✅ Frame térmico capturado com sucesso! (Aquisição #%d)", contador_aquisicoes);
            
                    // Calcula estatísticas
            float tmin = temps[0], tmax = temps[0], tavg = 0.0f;
            for (int i = 0; i < APP_THERMAL_TOTAL; i++) {
                if (temps[i] < tmin) tmin = temps[i];
                if (temps[i] > tmax) tmax = temps[i];
                tavg += temps[i];
            }
            tavg /= APP_THERMAL_TOTAL;
            
            ESP_LOGI(TAG, "=== Estatísticas Térmicas ===");
            ESP_LOGI(TAG, "Temperatura Mínima: %.2f°C", tmin);
            ESP_LOGI(TAG, "Temperatura Máxima: %.2f°C", tmax);
            ESP_LOGI(TAG, "Temperatura Média:  %.2f°C", tavg);
            ESP_LOGI(TAG, "================================");
            
                    // Adiciona frame ao arquivo acumulativo na SPIFFS
                    size_t frame_size = APP_THERMAL_TOTAL * sizeof(float);
                    if (bsp_spiffs_is_mounted()) {
                        esp_err_t append_ret = bsp_spiffs_append_thermal_frame(temps, frame_size, now);
                        if (append_ret == ESP_OK) {
                            ESP_LOGD(TAG, "✅ Frame adicionado ao arquivo acumulativo SPIFFS");
                            
                            // Verifica se atingiu o limite
                            size_t current_size = bsp_spiffs_get_thermal_file_size();
                            if (current_size >= THERMAL_SPIFFS_MAX_SIZE) {
                                ESP_LOGI(TAG, "📦 Limite da SPIFFS atingido (%d bytes). Migrando para SD card...", (int)current_size);
                                
                                // Migra dados da SPIFFS para SD card
                if (bsp_sdcard_is_mounted()) {
                                    // ✅ MELHORIA: Migração em chunks menores para reduzir uso de memória
                                    // Ao invés de carregar tudo na RAM, processa em pedaços menores
                                    // Isso evita problemas de OOM (Out Of Memory) quando há muitos frames
                                    size_t chunk_size = THERMAL_MIGRATION_CHUNK_SIZE;
                                    size_t total_migrated = 0;
                                    bool migration_success = true;
                                    
                                    ESP_LOGI(TAG, "📊 Migrando %d bytes em chunks de %d bytes", 
                                             (int)current_size, (int)chunk_size);
                                    
                                    // Aloca buffer para chunk (muito menor que carregar tudo)
                                    uint8_t *chunk_buffer = malloc(chunk_size);
                                    if (chunk_buffer) {
                                        // Lê arquivo da SPIFFS em chunks e migra para SD card
                                        FILE *spiffs_file = fopen("/spiffs/thermal_accum.bin", "rb");
                                        if (spiffs_file) {
                                            size_t bytes_read_chunk = 0;
                                            
                                            while ((bytes_read_chunk = fread(chunk_buffer, 1, chunk_size, spiffs_file)) > 0 && migration_success) {
                                                // Obtém tamanho do arquivo SD antes do append (para verificação)
                                                size_t sd_size_before = bsp_sdcard_get_file_size(THERMAL_ACCUM_FILE_LOCAL);
                                                
                                                // Adiciona chunk ao arquivo acumulativo no SD card
                                                esp_err_t append_ret = bsp_sdcard_append_file(THERMAL_ACCUM_FILE_LOCAL, chunk_buffer, bytes_read_chunk);
                                                
                                                if (append_ret == ESP_OK) {
                                                    // ✅ MELHORIA: Verificação read-after-write (garante integridade)
                                                    size_t sd_size_after = bsp_sdcard_get_file_size(THERMAL_ACCUM_FILE_LOCAL);
                                                    if (sd_size_after >= sd_size_before + bytes_read_chunk) {
                                                        // Verifica integridade dos dados escritos lendo de volta
                                                        esp_err_t verify_ret = bsp_sdcard_verify_write(THERMAL_ACCUM_FILE_LOCAL, chunk_buffer, bytes_read_chunk, sd_size_before);
                                                        if (verify_ret == ESP_OK) {
                                                            total_migrated += bytes_read_chunk;
                                                            ESP_LOGD(TAG, "✅ Chunk migrado e verificado: %d bytes (total: %d/%d)", 
                                                                     (int)bytes_read_chunk, (int)total_migrated, (int)current_size);
                                                        } else {
                                                            ESP_LOGE(TAG, "❌ Falha na verificação read-after-write do chunk");
                                                            migration_success = false;
                                                        }
                                                    } else {
                                                        ESP_LOGE(TAG, "❌ Tamanho do arquivo SD incorreto após append (antes: %d, depois: %d, esperado: %d)", 
                                                                 (int)sd_size_before, (int)sd_size_after, (int)(sd_size_before + bytes_read_chunk));
                                                        migration_success = false;
                                                    }
                                                } else {
                                                    ESP_LOGE(TAG, "❌ Falha ao adicionar chunk ao SD card");
                                                    migration_success = false;
                                                }
                                            }
                                            
                                            fclose(spiffs_file);
                                            
                                            // ✅ MELHORIA: Valida checksum total migrado
                                            if (migration_success && total_migrated == current_size) {
                                                ESP_LOGI(TAG, "✅ Dados migrados com sucesso: %d bytes (checksum validado)", (int)total_migrated);
                                                
                                                // Lê e migra metadados (timestamps)
                                                size_t frame_count = total_migrated / frame_size;
                                                if (frame_count > 0) {
                                                    time_t *timestamps = malloc(frame_count * sizeof(time_t));
                                                    if (timestamps) {
                                                        size_t timestamps_read = 0;
                                                        esp_err_t meta_ret = bsp_spiffs_read_thermal_metadata(timestamps, frame_count, &timestamps_read);
                                                        
                                                        if (meta_ret == ESP_OK && timestamps_read == frame_count) {
                                                            // ✅ CORREÇÃO 2: Valida tamanho necessário antes de usar buffer fixo
                                                            // Cada frame JSON: ~80 bytes ({"timestamp":...,"datetime":"..."})
                                                            size_t json_size_needed = (timestamps_read * 80) + 100; // ~80 bytes por frame + overhead
                                                            if (json_size_needed > 8192) {
                                                                ESP_LOGE(TAG, "❌ Buffer insuficiente para %d timestamps (necessário: %d bytes)", 
                                                                         (int)timestamps_read, (int)json_size_needed);
                                                                free(timestamps);
                                                                migration_success = false;
                                                                break;
                                                            }
                                                            
                                                            // Gera JSON de metadados
                                                            char json_buffer[8192];
                                                            int json_len = snprintf(json_buffer, sizeof(json_buffer), "{\"frames\":[");
                                                            
                                                            for (size_t i = 0; i < timestamps_read && json_len < (int)sizeof(json_buffer) - 150; i++) {
                                                                struct tm timeinfo_meta;
                                                                localtime_r(&timestamps[i], &timeinfo_meta);
                                                                
                                                                char datetime_str[32];
                                                                strftime(datetime_str, sizeof(datetime_str), "%Y-%m-%d %H:%M:%S", &timeinfo_meta);
                                                                
                                                                json_len += snprintf(json_buffer + json_len, sizeof(json_buffer) - json_len,
                                                                    "%s{\"timestamp\":%ld,\"datetime\":\"%s\"}",
                                                                    (i > 0) ? "," : "", (long)timestamps[i], datetime_str);
                                                            }
                                                            
                                                            json_len += snprintf(json_buffer + json_len, sizeof(json_buffer) - json_len, "]}\n");
                                                            
                                                            // Adiciona metadados ao arquivo acumulativo no SD card
                                                            esp_err_t meta_append_ret = bsp_sdcard_append_file(THERMAL_ACCUM_FILE_META_LOCAL, (const uint8_t*)json_buffer, json_len);
                                                            
                                                            // ✅ MELHORIA: Verifica metadados também
                                                            if (meta_append_ret == ESP_OK) {
                                                                esp_err_t meta_verify_ret = bsp_sdcard_verify_write(THERMAL_ACCUM_FILE_META_LOCAL, (const uint8_t*)json_buffer, json_len, 0);
                                                                if (meta_verify_ret == ESP_OK) {
                                                                    ESP_LOGI(TAG, "✅ Metadados migrados e verificados: %d timestamps", (int)timestamps_read);
                                                                    
                                                                    // ✅ MELHORIA: Limpa SPIFFS APENAS após confirmação completa (dados + metadados)
                                                                    esp_err_t clear_ret = bsp_spiffs_clear_thermal_file();
                                                                    if (clear_ret == ESP_OK) {
                                                                        ESP_LOGI(TAG, "✅ Arquivo acumulativo SPIFFS limpo após migração completa e verificada");
                                                                    } else {
                                                                        ESP_LOGW(TAG, "⚠️ Falha ao limpar arquivo acumulativo SPIFFS");
                                                                    }
                                                                } else {
                                                                    ESP_LOGE(TAG, "❌ Falha na verificação dos metadados. SPIFFS NÃO será limpo.");
                                                                    migration_success = false;
                                                                }
                                                            } else {
                                                                ESP_LOGE(TAG, "❌ Falha ao adicionar metadados ao SD card. SPIFFS NÃO será limpo.");
                                                                migration_success = false;
                                                            }
                                                        } else {
                                                            ESP_LOGE(TAG, "❌ Falha ao ler metadados ou quantidade incorreta. SPIFFS NÃO será limpo.");
                                                            migration_success = false;
                                                        }
                                                        
                                                        free(timestamps);
                                                    }
                                                } else {
                                                    ESP_LOGE(TAG, "❌ Frame count inválido. SPIFFS NÃO será limpo.");
                                                    migration_success = false;
                                                }
                                            } else {
                                                ESP_LOGE(TAG, "❌ Migração incompleta. SPIFFS NÃO será limpo.");
                                                ESP_LOGE(TAG, "   Migrado: %d bytes, Esperado: %d bytes", (int)total_migrated, (int)current_size);
                                            }
                                        } else {
                                            ESP_LOGE(TAG, "❌ Falha ao abrir arquivo SPIFFS para migração");
                                            free(chunk_buffer);  // ✅ CORREÇÃO: Libera buffer se fopen falhar
                                        }
                                        
                                        // Nota: chunk_buffer é liberado acima em todos os caminhos
                                    } else {
                                        ESP_LOGE(TAG, "❌ Falha ao alocar buffer para chunk de migração");
                                    }
                                } else {
                                    ESP_LOGW(TAG, "⚠️ SD Card não disponível. Dados mantidos na SPIFFS até SD estar disponível");
                                }
                            } else {
                                ESP_LOGD(TAG, "📊 Tamanho atual SPIFFS: %d / %d bytes", (int)current_size, (int)THERMAL_SPIFFS_MAX_SIZE);
                            }
                        } else {
                            ESP_LOGE(TAG, "❌ Falha ao adicionar frame ao arquivo acumulativo SPIFFS");
                        }
                    } else {
                        ESP_LOGW(TAG, "⚠️ SPIFFS não montado. Frame não foi salvo.");
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
        // ✅ CORREÇÃO 3: Divide delay longo em múltiplos delays menores com resets
        // Watchdog timeout é 5s, então dividimos 30s em 8 delays de ~4s cada
        for (int i = 0; i < 8; i++) {
            esp_task_wdt_reset();
            vTaskDelay(3750 / portTICK_PERIOD_MS);  // ~3.75s * 8 = 30s
        }
    }
}

static void task_envio_termica(void *pvParameter) {
    // ✅ CORREÇÃO 3: Adiciona task ao watchdog ANTES de qualquer operação
    esp_task_wdt_add(NULL);
    
    ESP_LOGI(TAG, "Task de envio térmico iniciada");
    
    // Buffer para um frame (3072 bytes)
    const size_t frame_size = APP_THERMAL_TOTAL * sizeof(float);
    float *frame_buffer = malloc(frame_size);
    if (!frame_buffer) {
        ESP_LOGE(TAG, "Falha ao alocar buffer para frame. Reiniciando...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    
    // Buffer para timestamps (até 1000 frames)
    const size_t max_timestamps = 1000;
    time_t *timestamps_buffer = malloc(max_timestamps * sizeof(time_t));
    if (!timestamps_buffer) {
        ESP_LOGE(TAG, "Falha ao alocar buffer para timestamps. Reiniciando...");
        free(frame_buffer);
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    
    while (true) {
        // ✅ CORREÇÃO 3: Alimenta watchdog periodicamente
        esp_task_wdt_reset();
        
        // Verifica se Wi-Fi está conectado e servidor acessível
        if (!bsp_wifi_is_connected()) {
            ESP_LOGD(TAG, "🌐 Wi-Fi desconectado. Aguardando conexão...");
            // ✅ CORREÇÃO 3: Divide delay longo em múltiplos delays menores com resets
            // Watchdog timeout é 5s, então dividimos 30s em 8 delays de ~4s cada
            for (int i = 0; i < 8; i++) {
                esp_task_wdt_reset();
                vTaskDelay(3750 / portTICK_PERIOD_MS);  // ~3.75s * 8 = 30s
            }
            continue;
        }
        
        // Verifica se há arquivo acumulativo pendente
        if (!bsp_sdcard_is_mounted()) {
            ESP_LOGD(TAG, "💾 SD Card não montado. Aguardando...");
            // ✅ CORREÇÃO 3: Divide delay longo em múltiplos delays menores com resets
            // Watchdog timeout é 5s, então dividimos 30s em 8 delays de ~4s cada
            for (int i = 0; i < 8; i++) {
                esp_task_wdt_reset();
                vTaskDelay(3750 / portTICK_PERIOD_MS);  // ~3.75s * 8 = 30s
            }
            continue;
        }
        
        // Verifica se existe arquivo pendente
        size_t file_size = bsp_sdcard_get_file_size(THERMAL_ACCUM_FILE_LOCAL);
        if (file_size == 0) {
            // Nenhum arquivo pendente
            ESP_LOGD(TAG, "📤 Nenhum arquivo térmico pendente para envio");
            // ✅ CORREÇÃO 3: Divide delay longo em múltiplos delays menores com resets
            // Watchdog timeout é 5s, então dividimos 60s em 15 delays de ~4s cada
            for (int i = 0; i < 15; i++) {
                esp_task_wdt_reset();
                vTaskDelay(4000 / portTICK_PERIOD_MS);  // 4s * 15 = 60s
            }
            continue;
        }
        
        // Calcula quantos frames tem no arquivo
        size_t total_frames = file_size / frame_size;
        if (total_frames == 0) {
            ESP_LOGW(TAG, "⚠️ Arquivo térmico muito pequeno ou inválido");
            // ✅ CORREÇÃO 3: Divide delay longo em múltiplos delays menores com resets
            // Watchdog timeout é 5s, então dividimos 60s em 15 delays de ~4s cada
            for (int i = 0; i < 15; i++) {
                esp_task_wdt_reset();
                vTaskDelay(4000 / portTICK_PERIOD_MS);  // 4s * 15 = 60s
            }
            continue;
        }
        
        ESP_LOGI(TAG, "📤 Processando envio térmico: %d frames (%d bytes)", 
                 (int)total_frames, (int)file_size);
        
        // Lê índice de progresso (quantos frames já foram enviados)
        uint32_t frames_enviados = 0;
        esp_err_t idx_ret = bsp_sdcard_read_send_index(THERMAL_INDEX_FILE, &frames_enviados);
        if (idx_ret == ESP_ERR_NOT_FOUND) {
            // Arquivo de índice não existe = nenhum frame foi enviado ainda
            frames_enviados = 0;
            ESP_LOGI(TAG, "📊 Iniciando envio do arquivo térmico (0/%d frames enviados)", (int)total_frames);
        } else if (idx_ret != ESP_OK) {
            ESP_LOGW(TAG, "⚠️ Erro ao ler índice. Assumindo 0 frames enviados");
            frames_enviados = 0;
        } else {
            ESP_LOGI(TAG, "📊 Retomando envio do arquivo térmico (%lu/%d frames enviados)", 
                     (unsigned long)frames_enviados, (int)total_frames);
        }
        
        // Carrega timestamps do arquivo de metadados
        size_t timestamps_read = 0;
        esp_err_t meta_ret = bsp_sdcard_read_thermal_timestamps(
            THERMAL_ACCUM_FILE_META_LOCAL, 
            timestamps_buffer, 
            max_timestamps, 
            &timestamps_read);
        
        if (meta_ret != ESP_OK || timestamps_read == 0) {
            ESP_LOGW(TAG, "⚠️ Não foi possível ler timestamps. Usando timestamp atual para todos os frames");
            // Usa timestamp atual se não conseguir ler metadados
            time_t now = time(NULL);
            for (size_t i = 0; i < total_frames && i < max_timestamps; i++) {
                timestamps_buffer[i] = now;
            }
            timestamps_read = total_frames;
        }
        
        // Processa frames pendentes
        bool envio_interrompido = false;
        uint32_t frames_enviados_nesta_sessao = 0;
        
        for (uint32_t i = frames_enviados; i < total_frames; i++) {
            // Verifica conexão antes de cada envio
            if (!bsp_wifi_is_connected()) {
                ESP_LOGW(TAG, "🌐 Wi-Fi desconectado durante envio. Interrompendo...");
                envio_interrompido = true;
                break;
            }
            
            // Lê frame do arquivo acumulativo
            esp_err_t read_ret = bsp_sdcard_read_thermal_frame(
                THERMAL_ACCUM_FILE_LOCAL,
                i,
                (uint8_t*)frame_buffer,
                frame_size);
            
            if (read_ret != ESP_OK) {
                ESP_LOGE(TAG, "❌ Falha ao ler frame %lu do arquivo", (unsigned long)i);
                continue;  // Pula este frame e tenta o próximo
            }
            
            // Obtém timestamp correspondente (ou timestamp atual se não disponível)
            time_t timestamp;
            if (i < timestamps_read) {
                timestamp = timestamps_buffer[i];
            } else {
                timestamp = time(NULL);  // Fallback: timestamp atual
            }
            
            // Envia frame para servidor
            ESP_LOGI(TAG, "📤 Enviando frame %lu/%d (timestamp: %ld)...", 
                     (unsigned long)(i + 1), (int)total_frames, (long)timestamp);
            
            // ✅ CORREÇÃO 3: Reset watchdog antes de envio HTTP (pode demorar)
            esp_task_wdt_reset();
            esp_err_t send_ret = app_http_send_thermal_frame(frame_buffer, timestamp);
            
            if (send_ret == ESP_OK) {
                frames_enviados++;
                frames_enviados_nesta_sessao++;
                
                // Atualiza índice de progresso após cada frame enviado
                esp_err_t save_idx_ret = bsp_sdcard_save_send_index(THERMAL_INDEX_FILE, frames_enviados);
                if (save_idx_ret != ESP_OK) {
                    ESP_LOGW(TAG, "⚠️ Falha ao salvar índice de progresso (mas frame foi enviado)");
                }
                
                ESP_LOGI(TAG, "✅ Frame %lu enviado com sucesso (%lu/%d enviados)", 
                         (unsigned long)(i + 1), (unsigned long)frames_enviados, (int)total_frames);
                
                // Pequeno delay entre envios para não sobrecarregar rede/servidor
                vTaskDelay(1000 / portTICK_PERIOD_MS);  // 1 segundo entre envios
            } else {
                ESP_LOGE(TAG, "❌ Falha ao enviar frame %lu. Interrompendo envio...", (unsigned long)(i + 1));
                envio_interrompido = true;
                break;  // Para de enviar e tenta novamente depois
            }
        }
        
        // Verifica se todos os frames foram enviados
        if (frames_enviados >= total_frames && !envio_interrompido) {
            ESP_LOGI(TAG, "✅ Todos os frames foram enviados! (%d frames)", (int)total_frames);
            
            // Verifica se arquivo de histórico já existe
            size_t sent_file_size = bsp_sdcard_get_file_size(THERMAL_SENT_FILE);
            
            if (sent_file_size > 0) {
                // Histórico existe: anexa THERML.BIN ao final de THERMS.BIN (preserva histórico)
                ESP_LOGI(TAG, "📦 Arquivo de histórico existe (%d bytes). Anexando novos dados...", (int)sent_file_size);
                
                esp_err_t append_ret = bsp_sdcard_append_file_to_file(
                    THERMAL_ACCUM_FILE_LOCAL, THERMAL_SENT_FILE);
                
                if (append_ret == ESP_OK) {
                    // Anexa metadados também
                    esp_err_t meta_append_ret = bsp_sdcard_append_file_to_file(
                        THERMAL_ACCUM_FILE_META_LOCAL, THERMAL_SENT_META_FILE);
                    
                    if (meta_append_ret == ESP_OK) {
                        // Remove arquivo local (já foi anexado ao histórico)
                        char local_path[128];
                        snprintf(local_path, sizeof(local_path), "%s/%s", SD_MOUNT_POINT, THERMAL_ACCUM_FILE_LOCAL);
                        unlink(local_path);
                        snprintf(local_path, sizeof(local_path), "%s/%s", SD_MOUNT_POINT, THERMAL_ACCUM_FILE_META_LOCAL);
                        unlink(local_path);
                        
                        // Remove arquivo de índice (não precisa mais, THERML.BIN foi processado)
                        snprintf(local_path, sizeof(local_path), "%s/%s", SD_MOUNT_POINT, THERMAL_INDEX_FILE);
                        unlink(local_path);
                        
                        size_t new_total_size = bsp_sdcard_get_file_size(THERMAL_SENT_FILE);
                        ESP_LOGI(TAG, "✅ Dados anexados ao histórico. Total acumulado: %d bytes (%d frames)", 
                                 (int)new_total_size, (int)(new_total_size / (APP_THERMAL_TOTAL * sizeof(float))));
                    } else {
                        ESP_LOGW(TAG, "⚠️ Falha ao anexar metadados ao histórico");
                    }
                } else {
                    ESP_LOGW(TAG, "⚠️ Falha ao anexar dados ao histórico");
                }
            } else {
                // Primeira vez: renomeia normalmente (cria arquivo de histórico)
                esp_err_t rename_ret = bsp_sdcard_rename_file(THERMAL_ACCUM_FILE_LOCAL, THERMAL_SENT_FILE);
                if (rename_ret == ESP_OK) {
                    ESP_LOGI(TAG, "✅ Arquivo renomeado: %s -> %s", THERMAL_ACCUM_FILE_LOCAL, THERMAL_SENT_FILE);
                    
                    // Renomeia metadados também
                    bsp_sdcard_rename_file(THERMAL_ACCUM_FILE_META_LOCAL, THERMAL_SENT_META_FILE);
                    
                    // Remove arquivo de índice (não precisa mais)
                    char idx_path[128];
                    snprintf(idx_path, sizeof(idx_path), "%s/%s", SD_MOUNT_POINT, THERMAL_INDEX_FILE);
                    unlink(idx_path);  // Remove arquivo de índice
                    
                    ESP_LOGI(TAG, "✅ Arquivo térmico completamente enviado e marcado como enviado");
        } else {
                    ESP_LOGW(TAG, "⚠️ Falha ao renomear arquivo após envio completo");
                }
            }
        } else if (frames_enviados_nesta_sessao > 0) {
            ESP_LOGI(TAG, "📊 Progresso: %lu/%d frames enviados (salvo no índice)", 
                     (unsigned long)frames_enviados, (int)total_frames);
        }
        
        // Aguarda antes de verificar novamente (60s se completo, 30s se interrompido)
        if (envio_interrompido) {
            ESP_LOGI(TAG, "⏳ Aguardando 30s antes de retomar envio...");
            // ✅ CORREÇÃO 3: Divide delay longo em múltiplos delays menores com resets
            // Watchdog timeout é 5s, então dividimos 30s em 8 delays de ~4s cada
            for (int i = 0; i < 8; i++) {
                esp_task_wdt_reset();
                vTaskDelay(3750 / portTICK_PERIOD_MS);  // ~3.75s * 8 = 30s
            }
        } else {
            ESP_LOGI(TAG, "⏳ Aguardando 60s antes de verificar novos arquivos...");
            // ✅ CORREÇÃO 3: Divide delay longo em múltiplos delays menores com resets
            // Watchdog timeout é 5s, então dividimos 60s em 15 delays de ~4s cada
            for (int i = 0; i < 15; i++) {
                esp_task_wdt_reset();
                vTaskDelay(4000 / portTICK_PERIOD_MS);  // 4s * 15 = 60s
            }
        }
    }
    
    // Nunca chega aqui, mas por segurança:
    free(frame_buffer);
    free(timestamps_buffer);
}

