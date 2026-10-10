#include "nvs_flash.h"
#include "greense_app.h"
#include "config.h"

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    greense_app_config_t cfg = {
        .wifi_ssid = WIFI_SSID,
        .wifi_pass = WIFI_PASS,
        .mqtt_broker = MQTT_BROKER,
        .mqtt_topic = MQTT_TOPIC,
        .mqtt_client_id = MQTT_CLIENT_ID,
        .mqtt_keepalive = MQTT_KEEPALIVE,
        .intervalo_s = SENSOR_READ_INTERVAL,
        .umid_solo_adc_seco = UMID_SOLO_ADC_SECO,
        .umid_solo_adc_umido = UMID_SOLO_ADC_UMIDO,
        .boias = GREENSE_BOIAS,
        .dht_ext = GREENSE_DHT_EXT,
        .mqtt_ext_inteiro = GREENSE_MQTT_EXT_INTEIRO,
    };
    greense_app_start(&cfg);
}
