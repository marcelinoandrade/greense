#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "secrets.h"

#define TAG "APP"

#define LED_PIN        8
#define UART_PORT      UART_NUM_1
#define UART_BAUD      115200
#define UART_RX_PIN    4
#define UART_TX_PIN    5

#define LINHAS         24
#define COLS           32
#define TOTAL          (LINHAS*COLS)
#define UART_BUF_MAX   8192
#define ENVIO_MS       (90*1000)

// ====== Variáveis globais ======
static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static bool s_wifi_connected = false;

// ====== Wi-Fi ======
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi desconectado, reconectando...");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        s_wifi_connected = false;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Conectado com IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        s_wifi_connected = true;
    }
}

static esp_err_t wifi_start_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Conectando ao Wi-Fi: %s", WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(15000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado ao Wi-Fi!");
        s_wifi_connected = true;
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Timeout na conexão Wi-Fi");
        s_wifi_connected = false;
        return ESP_FAIL;
    }
}

// ====== LED ======
static void led_blink(int times, int on_ms, int off_ms) {
    for (int i = 0; i < times; i++) {
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
}

static void led_control_task(void *pvParameters) {
    while (1) {
        if (s_wifi_connected) {
            // Wi-Fi conectado: LED acesso continuamente
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(1000)); // Verifica a cada segundo
        } else {
            // Wi-Fi desconectado: LED piscando (500ms ligado, 500ms desligado)
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(500));
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

// ====== UART ======
static size_t find_header_5A5A(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i + 1 < len; i++) {
        if (buf[i] == 0x5A && buf[i + 1] == 0x5A) return i;
    }
    return len;
}

static bool parse_frame(const uint8_t *payload, size_t plen, float out[TOTAL]) {
    const uint8_t *px = NULL;
    
    if (plen == 1538) {
        px = payload;
        plen = 1536;
    } else if (plen == 1543) {
        px = payload + 5;
        plen = 1536;
    } else {
        return false;
    }

    if (plen != 1536) return false;
    
    for (int i = 0; i < TOTAL; i++) {
        int16_t v = (int16_t)(px[2 * i] | (px[2 * i + 1] << 8));
        out[i] = ((float)v) / 100.0f;
    }
    
    float tmin = out[0], tmax = out[0];
    for (int i = 1; i < TOTAL; i++) {
        if (out[i] < tmin) tmin = out[i];
        if (out[i] > tmax) tmax = out[i];
    }
    
    return !(tmin < -40.0f || tmax > 200.0f);
}

static bool capturar_frame_termo(float out[TOTAL], TickType_t timeout_ticks) {
    uint8_t *buf = malloc(UART_BUF_MAX);
    if (!buf) return false;
    
    size_t used = 0;
    TickType_t t0 = xTaskGetTickCount();

    while ((xTaskGetTickCount() - t0) < timeout_ticks) {
        int n = uart_read_bytes(UART_PORT, buf + used, UART_BUF_MAX - used, pdMS_TO_TICKS(50));
        if (n > 0) used += n;
        
        if (used < 4) continue;

        size_t idx = find_header_5A5A(buf, used);
        if (idx == used) {
            if (used > 1) {
                buf[0] = buf[used - 1];
                used = 1;
            }
            continue;
        }
        
        if (idx > 0) {
            memmove(buf, buf + idx, used - idx);
            used -= idx;
            if (used < 4) continue;
        }

        uint16_t flen = buf[2] | (buf[3] << 8);
        size_t need = 4 + flen;
        
        if (used < need) continue;

        bool ok = parse_frame(buf + 4, flen, out);
        memmove(buf, buf + need, used - need);
        used -= need;
        
        if (ok) {
            free(buf);
            return true;
        }
    }
    
    free(buf);
    return false;
}

// ====== HTTP SIMPLES - COMO NO MICROPYTHON ======
static bool enviar_http_json(const float temps[TOTAL]) {
    ESP_LOGI(TAG, "Preparando dados HTTP...");
    
    // Calcular tamanho necessário
    int tamanho_estimado = 1000; // Base
    for (int i = 0; i < TOTAL; i++) {
        tamanho_estimado += 6; // "xx.xx," em média
    }
    
    char *json_buffer = malloc(tamanho_estimado);
    if (!json_buffer) {
        ESP_LOGE(TAG, "Falha ao alocar buffer");
        return false;
    }
    
    // Construir JSON manualmente - igual ao MicroPython
    char *ptr = json_buffer;
    
    // Header
    strcpy(ptr, "{\"temperaturas\":[");
    ptr += strlen(ptr);
    
    // Temperaturas
    for (int i = 0; i < TOTAL; i++) {
        if (i > 0) {
            *ptr++ = ',';
        }
        // Formatar como no MicroPython: "%.2f"
        int len = sprintf(ptr, "%.2f", temps[i]);
        ptr += len;
    }
    
    // Footer com timestamp
    int64_t ts_us = esp_timer_get_time();
    double ts_s = (double)ts_us / 1e6;
    sprintf(ptr, "],\"timestamp\":%.0f}", ts_s); // %.0f igual ao int(time.time())
    
    int json_len = strlen(json_buffer);
    ESP_LOGI(TAG, "JSON: %d bytes", json_len);

    // Configuração HTTP MÍNIMA - como urequests
    esp_http_client_config_t cfg = {
        .url = URL_POST,
        .timeout_ms = 20000, // 20 segundos
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Connection", "close"); // IMPORTANTE
    
    ESP_LOGI(TAG, "Enviando para: %s", URL_POST);
    esp_http_client_set_post_field(client, json_buffer, json_len);
    
    // Executar de forma SIMPLES
    esp_err_t err = esp_http_client_perform(client);
    
    int status_code = 0;
    if (err == ESP_OK) {
        status_code = esp_http_client_get_status_code(client);
    }
    
    // Limpeza IMEDIATA
    esp_http_client_cleanup(client);
    free(json_buffer);
    
    // Verificar resultado
    if (err == ESP_OK && status_code == 200) {
        ESP_LOGI(TAG, "✅ POST 200 - Sucesso!");
        led_blink(1, 300, 100); // Uma piscada para envio com sucesso
        return true;
    } else {
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Erro HTTP: %s", esp_err_to_name(err));
        } else {
            ESP_LOGW(TAG, "Status: %d", status_code);
        }
        led_blink(5, 100, 100); // Cinco piscadas para erro
        return false;
    }
}

// ====== VERIFICAR CONECTIVIDADE ======
static void verificar_conectividade(void) {
    ESP_LOGI(TAG, "Verificando conectividade com servidor...");
    
    esp_http_client_config_t cfg = {
        .url = "http://greense.com.br/",
        .timeout_ms = 10000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    
    esp_http_client_cleanup(client);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✅ Servidor acessível - Status: %d", status);
    } else {
        ESP_LOGE(TAG, "❌ Servidor inacessível: %s", esp_err_to_name(err));
    }
}

// ====== APP ======
void app_main(void) {
    ESP_LOGI(TAG, "Iniciando aplicação...");
    ESP_LOGI(TAG, "Memória livre: %d bytes", esp_get_free_heap_size());
    
    // Configurar LED
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_PIN, 0);

    led_blink(2, 200, 200);

    // Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Conectar Wi-Fi
    if (wifi_start_sta() != ESP_OK) {
        ESP_LOGE(TAG, "Falha Wi-Fi. Reiniciando...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }

    // Iniciar tarefa de controle do LED
    xTaskCreate(led_control_task, "led_control", 2048, NULL, 1, NULL);

    // Verificar conectividade
    verificar_conectividade();

    // Configurar UART
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_MAX, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    float temps[TOTAL];
    int ciclo = 0;

    ESP_LOGI(TAG, "=== INICIANDO LOOP PRINCIPAL ===");
    
    while (1) {
        ciclo++;
        ESP_LOGI(TAG, "Ciclo %d", ciclo);
        ESP_LOGI(TAG, "Lendo frame térmico...");
        
        if (capturar_frame_termo(temps, pdMS_TO_TICKS(5000))) {
            float tmin = temps[0], tmax = temps[0];
            for (int i = 1; i < TOTAL; i++) {
                if (temps[i] < tmin) tmin = temps[i];
                if (temps[i] > tmax) tmax = temps[i];
            }
            
            ESP_LOGI(TAG, "Frame: min=%.2f°C max=%.2f°C", tmin, tmax);
            
            // Tentar enviar 3 vezes como fallback
            bool enviado = false;
            for (int tentativa = 0; tentativa < 3 && !enviado; tentativa++) {
                if (tentativa > 0) {
                    ESP_LOGI(TAG, "Tentativa %d/3", tentativa + 1);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                }
                enviado = enviar_http_json(temps);
            }
            
            if (enviado) {
                ESP_LOGI(TAG, "✅ Ciclo %d completo", ciclo);
            } else {
                ESP_LOGW(TAG, "❌ Ciclo %d falhou após 3 tentativas", ciclo);
            }
        } else {
            ESP_LOGW(TAG, "❌ Sem frame válido no ciclo %d", ciclo);
        }
        
        ESP_LOGI(TAG, "Aguardando %d segundos...", ENVIO_MS / 1000);
        vTaskDelay(pdMS_TO_TICKS(ENVIO_MS));
    }
}