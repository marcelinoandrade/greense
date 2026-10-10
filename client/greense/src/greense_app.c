#include "greense_app.h"
#include "conexoes.h"
#include "sensores.h"
#include "atuadores.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"

static int arredondar(float valor)
{
    return (int)(valor >= 0.0f ? valor + 0.5f : valor - 0.5f);
}

void greense_app_start(const greense_app_config_t *cfg)
{
    conexao_wifi_init(cfg);
    conexao_mqtt_start(cfg);
    sensores_init(cfg);
    atuadores_init();

    while (true) {
        if (!conexao_wifi_is_connected()) {
            led_set_color(10, 0, 0);
            printf("Wi-Fi está desconectado. Tentando reconectar...\n");
            esp_wifi_disconnect();
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            esp_wifi_connect();
        } else {
            printf("Wi-Fi está conectado.\n");
            led_set_color(0, 0, 10);

            if (conexao_mqtt_is_connected()) {
                sensor_data_t dados = sensores_ler_dados();
                char temp_ext_txt[16];
                char umid_ext_txt[16];
                if (cfg->mqtt_ext_inteiro) {
                    snprintf(temp_ext_txt, sizeof(temp_ext_txt), "%d", arredondar(dados.temp_externa));
                    snprintf(umid_ext_txt, sizeof(umid_ext_txt), "%d", arredondar(dados.umid_externa));
                } else {
                    snprintf(temp_ext_txt, sizeof(temp_ext_txt), "%.2f", dados.temp_externa);
                    snprintf(umid_ext_txt, sizeof(umid_ext_txt), "%.2f", dados.umid_externa);
                }
                char payload[384];
                snprintf(payload, sizeof(payload),
                    "{\"temp\": %.2f, \"umid\": %.2f, \"co2\": %.2f, \"luz\": %.2f, \"agua_min\": %d, \"agua_max\": %d, "
                    "\"temp_reserv_int\": %.2f, \"ph\": %.2f, \"ec\": %.2f, \"temp_reserv_ext\": %.2f, "
                    "\"temp_externa\": %s, \"umid_externa\": %s, "
                    "\"umid_solo_raw\": %d, \"umid_solo_pct\": %.2f}",
                    dados.temp, dados.umid, dados.co2, dados.luz, dados.agua_min, dados.agua_max,
                    dados.temp_reserv_int, dados.ph, dados.ec, dados.temp_reserv_ext,
                    temp_ext_txt, umid_ext_txt,
                    dados.umid_solo_raw, dados.umid_solo_pct);

                conexao_mqtt_publish(cfg->mqtt_topic, payload);
            } else {
                printf("MQTT não está conectado.\n");
                led_set_color(10, 0, 0);
            }
        }

        vTaskDelay((cfg->intervalo_s * 1000) / portTICK_PERIOD_MS);
    }
}
