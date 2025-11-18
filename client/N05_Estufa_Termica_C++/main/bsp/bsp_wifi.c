#include "bsp_wifi.h"
#include "bsp_gpio.h"
#include "../gui/gui_led.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "../config.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#define TAG "BSP_WIFI"

#define MAX_RETRY 15  // Aumentado para redes corporativas que podem demorar mais
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi STA iniciado. Conectando ao SSID: %s", WIFI_SSID);
        gui_led_set_state_wifi_disconnected();  // LED apagado enquanto tenta conectar
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* discon = (wifi_event_sta_disconnected_t*) event_data;
        
        // Atualizar LED para desconectado
        gui_led_set_state_wifi_disconnected();
        
        // Logs detalhados dos motivos de desconexão
        const char* motivo_str = "Desconhecido";
        switch(discon->reason) {
            case 1: motivo_str = "UNSPECIFIED (Não especificado - pode ser timeout ou política)"; break;
            case 2: motivo_str = "AUTH_EXPIRE (Autenticação expirada)"; break;
            case 205: motivo_str = "NO_AP_FOUND (AP não encontrado)"; break;
            case 15: motivo_str = "4WAY_HANDSHAKE_TIMEOUT"; break;
            case 201: motivo_str = "ASSOC_FAIL (Falha na associação)"; break;
            case 200: motivo_str = "BEACON_TIMEOUT (Timeout de beacon)"; break;
            default: motivo_str = "Outro"; break;
        }
        
        ESP_LOGW(TAG, "Desconectado. Motivo: %d (%s)", discon->reason, motivo_str);
        
        // Se estava conectado e desconectou, resetar contador para dar mais chances
        EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGW(TAG, "Desconexão após conexão bem-sucedida. Resetando contador de tentativas.");
            s_retry_num = 0;  // Resetar para dar mais chances
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }

        if (s_retry_num < MAX_RETRY) {
            s_retry_num++;
            
            // Delay simples e progressivo: 2s, 3s, 4s, 5s (máximo 5s)
            int delay_ms = 2000 + (s_retry_num * 500);
            if (delay_ms > 5000) delay_ms = 5000;
            
            ESP_LOGI(TAG, "Tentativa %d/%d: Reconectando ao SSID: %s... (aguardando %dms)", 
                     s_retry_num, MAX_RETRY, WIFI_SSID, delay_ms);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            
            // Reinicializar Wi-Fi apenas a cada 10 tentativas (último recurso)
            if (s_retry_num % 10 == 0 && s_retry_num > 0) {
                ESP_LOGI(TAG, "Reinicializando Wi-Fi para limpar estado...");
                esp_wifi_stop();
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_wifi_start();
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Falha ao conectar ao Wi-Fi após %d tentativas", MAX_RETRY);
            ESP_LOGE(TAG, "Verifique: SSID='%s' está correto e visível?", WIFI_SSID);
            ESP_LOGE(TAG, "MAC Address: 10:00:3b:00:56:44 - Solicite liberação no firewall se necessário");
            gui_led_set_state_wifi_disconnected();  // LED apagado quando falha conexão
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "✅ Conectado! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        // LED será atualizado após verificação de conectividade no app_main
        // Mas por enquanto, indicar que Wi-Fi está conectado (piscando lento até verificar internet)
        gui_led_set_state_wifi_connected_no_internet();  // Inicia sem internet até verificar
        ESP_LOGI(TAG, "LED atualizado para: Wi-Fi conectado (aguardando verificação de internet)");
    }
}

esp_err_t bsp_wifi_init(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Configuração específica para ESP32-C3 (RISC-V single-core)
    // A C3 pode precisar de mais tempo para processar eventos Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    
    // Aumentar buffers para melhor estabilidade em redes corporativas
    // (já configurado no sdkconfig, mas garantimos aqui também)
    cfg.static_rx_buf_num = 10;
    cfg.dynamic_rx_buf_num = 32;
    cfg.dynamic_tx_buf_num = 32;
    
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold = {
                .rssi = -127,  // Aceitar qualquer sinal (sem threshold mínimo)
                .authmode = WIFI_AUTH_WPA2_PSK,  // WPA2 (compatível com redes corporativas)
            },
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
            .scan_method = WIFI_FAST_SCAN,  // Scan rápido
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,  // Conectar ao AP com melhor sinal
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // ESP32-C3: Desabilitar power saving completamente para melhor estabilidade
    // A C3 pode ter problemas com power management em redes corporativas
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    
    // ESP32-C3: Nota sobre diferenças da C3:
    // - Single-core RISC-V pode processar eventos Wi-Fi mais lentamente
    // - Pode precisar de mais tempo entre operações Wi-Fi
    // - Power management pode ser mais agressivo por padrão

    ESP_LOGI(TAG, "Wi-Fi inicializado (ESP32-C3)");
    ESP_LOGI(TAG, "SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "Aguardando conexão...");
    
    // ESP32-C3: Delay maior para garantir que o Wi-Fi está totalmente pronto
    // RISC-V single-core pode precisar de mais tempo para inicializar
    vTaskDelay(pdMS_TO_TICKS(500));

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado ao Wi-Fi com sucesso");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Falha ao conectar ao Wi-Fi");
        return ESP_FAIL;
    }
}

esp_err_t bsp_wifi_connect(void) {
    return esp_wifi_connect();
}

bool bsp_wifi_is_connected(void)
{
    wifi_ap_record_t ap_info;
    return (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
}

esp_err_t bsp_wifi_wait_connected(TickType_t timeout) {
    // Esta função não é mais necessária já que bsp_wifi_init() já espera
    // Mas mantemos para compatibilidade
    if (bsp_wifi_is_connected()) {
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}
