#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "esp_wifi.h"
#include "config.h"
#include "conexao.h"  // <- Aqui está tudo: Wi-Fi + MQTT

void app_main(void)
{
    // Inicializa NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Inicializa Wi-Fi
    conexao_wifi_init();

    // Inicializa MQTT
    conexao_mqtt_start();

    while (true) {
        // Verifica conexão Wi-Fi
        if (!conexao_wifi_is_connected()) {
            printf("Wi-Fi está desconectado. Tentando reconectar...\n");
            esp_wifi_disconnect();
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            esp_wifi_connect();
        } else {
            printf("Wi-Fi está conectado.\n");

            // Verifica conexão MQTT
            if (conexao_mqtt_is_connected()) {
                // Simula leitura de sensor
                char payload[64];
                snprintf(payload, sizeof(payload), "{\"temp\": %.1f}", 24.5);

                // Publica no tópico MQTT
                conexao_mqtt_publish(MQTT_TOPIC, payload);
            } else {
                printf("MQTT não está conectado.\n");
            }
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS); // Aguarda 5 segundos
    }
}
