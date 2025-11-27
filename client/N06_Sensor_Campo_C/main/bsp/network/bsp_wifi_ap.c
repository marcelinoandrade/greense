#include "bsp_wifi_ap.h"
#include "../board.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "BSP_WIFI_AP";

/* Callbacks registrados */
static wifi_ap_client_connected_cb_t client_connected_cb = NULL;
static wifi_ap_client_disconnected_cb_t client_disconnected_cb = NULL;
static bool initialized = false;

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *e =
            (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG,
                 "Cliente %02x:%02x:%02x:%02x:%02x:%02x conectou, AID=%d",
                 e->mac[0], e->mac[1], e->mac[2],
                 e->mac[3], e->mac[4], e->mac[5],
                 e->aid);
        
        /* Notifica APP via callback */
        if (client_connected_cb != NULL) {
            client_connected_cb();
        }
    }
    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *e =
            (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG,
                 "Cliente %02x:%02x:%02x:%02x:%02x:%02x saiu, AID=%d",
                 e->mac[0], e->mac[1], e->mac[2],
                 e->mac[3], e->mac[4], e->mac[5],
                 e->aid);
        
        /* Notifica APP via callback */
        if (client_disconnected_cb != NULL) {
            client_disconnected_cb();
        }
    }
}

esp_err_t wifi_ap_init(void)
{
    if (initialized) {
        return ESP_OK;
    }
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            NULL));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = BSP_WIFI_AP_SSID,
            .ssid_len = 0,
            .password = BSP_WIFI_AP_PASSWORD,
            .channel = BSP_WIFI_AP_CHANNEL,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = BSP_WIFI_AP_MAX_CONN,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    if (strlen((char *)ap_config.ap.password) == 0)
    {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG,
             "Modo AP iniciado. SSID='%s' senha='%s'",
             ap_config.ap.ssid,
             ap_config.ap.password);

    /* O esp_netif já cria DHCP server padrão para WIFI_AP_DEF
       e entrega IP tipo 192.168.4.1 */

    initialized = true;
    return ESP_OK;
}

esp_err_t wifi_ap_register_callbacks(wifi_ap_client_connected_cb_t on_client_connected,
                                      wifi_ap_client_disconnected_cb_t on_client_disconnected)
{
    client_connected_cb = on_client_connected;
    client_disconnected_cb = on_client_disconnected;
    return ESP_OK;
}

esp_err_t wifi_ap_stop(void)
{
    if (!initialized) {
        return ESP_OK;
    }
    
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    
    /* Limpa callbacks */
    client_connected_cb = NULL;
    client_disconnected_cb = NULL;
    initialized = false;
    
    return ESP_OK;
}
