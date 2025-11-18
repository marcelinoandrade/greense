#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"

#include "config.h"
#include "conexoes/conexoes.h"
#include "sensores/sensores.h"
#include "atuadores/atuadores.h"

void app_main(void) {
  
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

    // Inicializa Sensores
    sensores_init();

    // Inicializa os atuadores
    atuadores_init();

    while (true) {
       
        // Verifica conexão Wi-Fi
        if (!conexao_wifi_is_connected()) {
            led_set_color(10, 0, 0);
            printf("Wi-Fi está desconectado. Tentando reconectar...\n");
            esp_wifi_disconnect();
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            esp_wifi_connect();
        } else {
            printf("Wi-Fi está conectado.\n");
            led_set_color(0, 0, 10);

            // Verifica conexão MQTT
            if (conexao_mqtt_is_connected()) {
                // Lê dados dos sensores
                sensor_data_t dados = sensores_ler_dados();
                char payload[512];
                snprintf(payload, sizeof(payload),
                "{\"temp\": %.2f, \"umid\": %.2f, \"co2\": %.2f, \"luz\": %.2f, \"agua_min\": %d, \"agua_max\": %d, "
                "\"temp_reserv_int\": %.2f, \"ph\": %.2f, \"ec\": %.2f, \"temp_reserv_ext\": %.2f, "
                "\"temp_externa\": %.2f, \"umid_externa\": %.2f}",
                dados.temp, dados.umid, dados.co2, dados.luz, dados.agua_min, dados.agua_max,
                dados.temp_reserv_int, dados.ph, dados.ec, dados.temp_reserv_ext,
                dados.temp_externa, dados.umid_externa);
       
                // Publica no tópico MQTT
                conexao_mqtt_publish(MQTT_TOPIC, payload);
            } else {
                printf("MQTT não está conectado.\n");
                led_set_color(10, 0, 0);
            }
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS); // Aguarda 5 segundos
    }
}