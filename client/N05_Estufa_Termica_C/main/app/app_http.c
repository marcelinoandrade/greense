#include "app_http.h"
#include "app_time.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "../config.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

// Certificado embutido (não usado em HTTP, apenas em HTTPS)
// extern const uint8_t greense_cert_pem_start[] asm("_binary_greense_cert_pem_start");
// extern const uint8_t greense_cert_pem_end[]   asm("_binary_greense_cert_pem_end");

#define TAG "APP_HTTP"

bool app_http_send_thermal_data(const float temps[APP_THERMAL_TOTAL]) {
    ESP_LOGI(TAG, "Preparando dados HTTP...");
    
    int tamanho_buffer = (APP_THERMAL_TOTAL * 10) + 1000;
    char *json_buffer = malloc(tamanho_buffer);
    if (!json_buffer) {
        ESP_LOGE(TAG, "Falha ao alocar buffer JSON");
        return false;
    }
    
    char *ptr = json_buffer;
    int espaco_restante = tamanho_buffer;
    int len = 0;

    len = snprintf(ptr, espaco_restante, "{\"temperaturas\":[");
    ptr += len;
    espaco_restante -= len;

    for (int i = 0; i < APP_THERMAL_TOTAL; i++) {
        if (espaco_restante < 2) break;

        if (i > 0) {
            *ptr++ = ',';
            espaco_restante--;
        }

        if (espaco_restante < 15) break;

        len = snprintf(ptr, espaco_restante, "%.2f", temps[i]);
        
        if (len >= espaco_restante) {
            ESP_LOGE(TAG, "Buffer JSON estourou!");
            break; 
        }
        
        ptr += len;
        espaco_restante -= len;
    }
    
    if (espaco_restante < 50) {
         ESP_LOGE(TAG, "Buffer JSON sem espaço para timestamp!");
         free(json_buffer);
         return false;
    }

    // Usar timestamp Unix real (sincronizado via NTP)
    time_t unix_timestamp = app_time_get_unix_timestamp();
    snprintf(ptr, espaco_restante, "],\"timestamp\":%lld}", (long long)unix_timestamp);
    
    int json_len = strlen(json_buffer);
    
    // Log do JSON completo para debug
    ESP_LOGI(TAG, "JSON completo (%d bytes): %s", json_len, json_buffer);
    
    ESP_LOGI(TAG, "Conectando HTTP: %s", URL_POST);
    
    esp_http_client_config_t cfg = {
        .url = URL_POST,
        .method = HTTP_METHOD_POST,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .timeout_ms = 30000
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "Falha ao inicializar cliente HTTP");
        free(json_buffer);
        return false;
    }
    
    // Headers necessários
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "User-Agent", "ESP32-C3-Thermal/1.0");
    esp_http_client_set_post_field(client, json_buffer, json_len);
    
    ESP_LOGI(TAG, "Enviando POST... (JSON: %d bytes)", json_len);
    esp_err_t err = esp_http_client_perform(client);
    
    int status_code = 0;
    if (err == ESP_OK) {
        status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Status HTTP: %d", status_code);
        
        // Capturar resposta do servidor para debug (apenas se erro)
        if (status_code != 200) {
            int content_length = esp_http_client_get_content_length(client);
            ESP_LOGW(TAG, "Content-Length da resposta: %d bytes", content_length);
            
            // Ler resposta do servidor (até 1024 bytes)
            int buffer_size = (content_length > 0 && content_length < 1024) ? content_length + 1 : 1024;
            char *response_buffer = malloc(buffer_size);
            if (response_buffer) {
                int data_read = esp_http_client_read(client, response_buffer, buffer_size - 1);
                if (data_read > 0) {
                    response_buffer[data_read] = '\0';
                    ESP_LOGW(TAG, "Resposta do servidor (%d bytes): %s", data_read, response_buffer);
                } else if (data_read == 0) {
                    ESP_LOGW(TAG, "Resposta do servidor vazia");
                } else {
                    ESP_LOGW(TAG, "Erro ao ler resposta: %d", data_read);
                }
                free(response_buffer);
            } else {
                ESP_LOGE(TAG, "Falha ao alocar buffer para resposta");
            }
        }
    } else {
        ESP_LOGE(TAG, "Erro HTTP: %s (0x%x)", esp_err_to_name(err), err);
        
        // Logs adicionais para debug
        int content_length = esp_http_client_get_content_length(client);
        ESP_LOGE(TAG, "Content-Length: %d", content_length);
    }
    
    esp_http_client_cleanup(client);
    free(json_buffer);
    
    if (err == ESP_OK && status_code == 200) {
        ESP_LOGI(TAG, "✅ POST 200 - Sucesso!");
        return true;
    } else {
        ESP_LOGW(TAG, "❌ Falha HTTP (%s), status %d", esp_err_to_name(err), status_code);
        return false;
    }
}

bool app_http_check_connectivity(const char *url) {
    ESP_LOGI(TAG, "Verificando conectividade: %s", url);
    
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
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

