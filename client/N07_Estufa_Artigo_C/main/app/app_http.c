#include "app_http.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <stdint.h>
#include <string.h>

// Certificado embutido (definido no CMakeLists.txt)
extern const uint8_t greense_cert_pem_start[] asm("_binary_greense_cert_pem_start");
extern const uint8_t greense_cert_pem_end[]   asm("_binary_greense_cert_pem_end");

#define TAG "APP_HTTP"

esp_err_t app_http_send_data(const char *url, const uint8_t *data, size_t data_len, const char *content_type)
{
    ESP_LOGI(TAG, "Enviando dados para: %s", url);
    ESP_LOGI(TAG, "Tamanho dos dados: %d bytes", data_len);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = (const char *)greense_cert_pem_start,
        .timeout_ms = 10000
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Falha ao inicializar cliente HTTP");
        return ESP_FAIL;
    }
    
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_http_client_set_post_field(client, (const char *)data, data_len);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Dados enviados. Status HTTP: %d", status_code);
        if (status_code == 200) {
            ESP_LOGI(TAG, "✅ Envio bem-sucedido!");
        } else {
            ESP_LOGW(TAG, "⚠️ Servidor retornou status: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "Erro ao enviar dados: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
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

