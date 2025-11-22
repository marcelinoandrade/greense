#include "app_http.h"
#include "app_thermal.h"
#include "../config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include "freertos/task.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

// Certificado embutido (definido no CMakeLists.txt)
extern const uint8_t greense_cert_pem_start[] asm("_binary_greense_cert_pem_start");
extern const uint8_t greense_cert_pem_end[]   asm("_binary_greense_cert_pem_end");

#define TAG "APP_HTTP"

// ✅ CORREÇÃO: Callback para resetar watchdog durante transferência HTTP
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    // ✅ CORREÇÃO: Reset watchdog em TODOS os eventos para garantir resets frequentes
    esp_task_wdt_reset();
    
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER");
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA: %d bytes", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t app_http_send_data(const char *url, const uint8_t *data, size_t data_len, const char *content_type)
{
    if (!url || !data || data_len == 0) {
        ESP_LOGE(TAG, "Parâmetros inválidos");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Enviando dados para: %s", url);
    ESP_LOGI(TAG, "Tamanho dos dados: %d bytes", (int)data_len);
    
    // Copia dados para buffer em RAM (mais seguro para HTTPS com PSRAM)
    // Tenta alocar em RAM interna primeiro, depois tenta qualquer RAM disponível
    uint8_t *ram_buffer = heap_caps_malloc(data_len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!ram_buffer) {
        ESP_LOGW(TAG, "RAM interna insuficiente, tentando RAM geral...");
        ram_buffer = malloc(data_len);
    }
    
    if (!ram_buffer) {
        ESP_LOGE(TAG, "Falha ao alocar buffer (%d bytes)", (int)data_len);
        return ESP_ERR_NO_MEM;
    }
    
    // Copia dados do PSRAM para RAM
    memcpy(ram_buffer, data, data_len);
    
    // Valida certificado
    size_t cert_len = greense_cert_pem_end - greense_cert_pem_start;
    if (cert_len == 0 || cert_len > 8192) {
        ESP_LOGE(TAG, "Certificado inválido (tamanho: %d)", (int)cert_len);
        free(ram_buffer);
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = (const char *)greense_cert_pem_start,
        .timeout_ms = 30000,  // Aumentado para 30s (mais seguro)
        .skip_cert_common_name_check = false,
        .event_handler = http_event_handler  // ✅ CORREÇÃO: Callback para resetar watchdog
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Falha ao inicializar cliente HTTP");
        free(ram_buffer);
        return ESP_FAIL;
    }
    
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_http_client_set_post_field(client, (const char *)ram_buffer, data_len);

    // ✅ CORREÇÃO 4: Retry com backoff exponencial
    const int max_retries = 3;
    esp_err_t err = ESP_FAIL;
    int status_code = 0;
    
    for (int attempt = 0; attempt < max_retries; attempt++) {
        if (attempt > 0) {
            int backoff_ms = 1000 * (1 << (attempt - 1)); // 1s, 2s, 4s...
            ESP_LOGW(TAG, "Tentativa %d/%d após %d ms...", attempt + 1, max_retries, backoff_ms);
            vTaskDelay(pdMS_TO_TICKS(backoff_ms));
        }
        
        // ✅ CORREÇÃO: Reset watchdog antes de operação longa
        esp_task_wdt_reset();
        
        // ✅ CORREÇÃO: Usa perform normalmente, mas o callback HTTP reseta watchdog periodicamente
        // O callback http_event_handler será chamado durante a transferência e resetará o watchdog
        err = esp_http_client_perform(client);
        
        // ✅ CORREÇÃO: Reset watchdog após operação (mesmo se falhou)
        esp_task_wdt_reset();
        
        if (err == ESP_OK) {
            status_code = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG, "Dados enviados. Status HTTP: %d (tentativa %d/%d)", 
                     status_code, attempt + 1, max_retries);
            
            // Verifica se status HTTP é sucesso (2xx)
            if (status_code >= 200 && status_code < 300) {
                ESP_LOGI(TAG, "✅ Envio bem-sucedido!");
                err = ESP_OK;
                break; // Sucesso, sai do loop
            } else {
                ESP_LOGE(TAG, "❌ Servidor retornou erro HTTP: %d", status_code);
                err = ESP_FAIL;
            }
        } else {
            ESP_LOGE(TAG, "Erro ao enviar dados (tentativa %d/%d): %s (0x%x)", 
                     attempt + 1, max_retries, esp_err_to_name(err), err);
            // Continua tentando se ainda houver tentativas
        }
    }

    esp_http_client_cleanup(client);
    free(ram_buffer);  // Libera buffer temporário
    return err;
}

bool app_http_check_connectivity(const char *url)
{
    ESP_LOGI(TAG, "Verificando conectividade: %s", url);
    
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = (const char *)greense_cert_pem_start,
        .timeout_ms = 15000
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "Falha ao inicializar cliente HTTP");
        return false;
    }
    
    ESP_LOGI(TAG, "Enviando GET...");
    esp_err_t err = esp_http_client_perform(client);
    
    int status_code = 0;
    if (err == ESP_OK) {
        status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Status HTTP: %d", status_code);
    } else {
        ESP_LOGE(TAG, "Erro HTTP: %s (0x%x)", esp_err_to_name(err), err);
    }
    
    esp_http_client_cleanup(client);
    
    // Considerar sucesso apenas se conexão OK E status HTTP 2xx
    if (err == ESP_OK && status_code >= 200 && status_code < 300) {
        ESP_LOGI(TAG, "✅ Servidor acessível (HTTP %d)", status_code);
        return true;
    } else {
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "❌ Servidor inacessível: %s", esp_err_to_name(err));
        } else {
            ESP_LOGW(TAG, "⚠️ Servidor retornou erro HTTP %d", status_code);
        }
        return false;
    }
}

esp_err_t app_http_send_thermal_frame(const float *temps, time_t timestamp)
{
    if (!temps) {
        ESP_LOGE(TAG, "Parâmetros inválidos em send_thermal_frame");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Calcula tamanho do JSON: {"temperaturas":[768 floats], "timestamp":timestamp}
    // Cada float: até 10 caracteres + vírgula = ~11 chars, mais overhead = ~9000 bytes
    int json_buffer_size = (APP_THERMAL_TOTAL * 12) + 100;  // Buffer generoso
    char *json_buffer = malloc(json_buffer_size);
    if (!json_buffer) {
        ESP_LOGE(TAG, "Falha ao alocar buffer JSON (%d bytes)", json_buffer_size);
        return ESP_ERR_NO_MEM;
    }
    
    // Monta JSON
    char *ptr = json_buffer;
    int espaco_restante = json_buffer_size;
    int len = 0;
    
    // Inicia JSON
    len = snprintf(ptr, espaco_restante, "{\"temperaturas\":[");
    if (len < 0 || len >= espaco_restante) {
        ESP_LOGE(TAG, "Falha ao criar JSON (buffer insuficiente)");
        free(json_buffer);
        return ESP_ERR_INVALID_SIZE;
    }
    ptr += len;
    espaco_restante -= len;
    
    // Adiciona cada temperatura
    for (int i = 0; i < APP_THERMAL_TOTAL; i++) {
        if (espaco_restante < 15) {
            ESP_LOGE(TAG, "Buffer JSON insuficiente");
            free(json_buffer);
            return ESP_ERR_INVALID_SIZE;
        }
        
        if (i > 0) {
            *ptr++ = ',';
            espaco_restante--;
        }
        
        len = snprintf(ptr, espaco_restante, "%.2f", temps[i]);
        if (len < 0 || len >= espaco_restante) {
            ESP_LOGE(TAG, "Falha ao adicionar temperatura ao JSON");
            free(json_buffer);
            return ESP_ERR_INVALID_SIZE;
        }
        ptr += len;
        espaco_restante -= len;
    }
    
    // Adiciona timestamp
    len = snprintf(ptr, espaco_restante, "],\"timestamp\":%ld}", (long)timestamp);
    if (len < 0 || len >= espaco_restante) {
        ESP_LOGE(TAG, "Falha ao adicionar timestamp ao JSON");
        free(json_buffer);
        return ESP_ERR_INVALID_SIZE;
    }
    
    int json_len = strlen(json_buffer);
    ESP_LOGD(TAG, "Enviando frame térmico: JSON de %d bytes", json_len);
    
    // Usa a função existente para enviar
    esp_err_t ret = app_http_send_data(CAMERA_THERMAL_UPLOAD_URL, 
                                       (const uint8_t*)json_buffer, 
                                       json_len, 
                                       "application/json");
    
    free(json_buffer);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Frame térmico enviado com sucesso");
    } else {
        ESP_LOGE(TAG, "❌ Falha ao enviar frame térmico");
    }
    
    return ret;
}

