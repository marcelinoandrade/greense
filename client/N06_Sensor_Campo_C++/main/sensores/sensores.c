#include "sensores.h"
#include "ds18b20.h"
#include "soil_moisture.h"

#include <math.h>
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "SENSORES";

/* Mapeamento de hardware atual */
#define GPIO_DS18B20_SOLO   4   /* seu DS18B20 no GPIO4 */

/* Cache interno básico para evitar NAN intermitente */
static float ultimo_temp_solo = NAN;
static int   ultimo_umid_raw  = -1;

void sensores_init(void)
{
    /* Inicializa DS18B20 */
    ds18b20_init(GPIO_DS18B20_SOLO);

    /* Inicializa ADC da umidade do solo */
    esp_err_t err = soil_moisture_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "soil_moisture_init falhou: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Sensores inicializados.");
}

float sensores_get_temp_ar(void)
{
    /* Placeholder. Você logou 25.0 °C no firmware original. */
    return 25.0f;
}

float sensores_get_umid_ar(void)
{
    /* Placeholder. Você logou 50.0 % no firmware original. */
    return 50.0f;
}

float sensores_get_temp_solo(void)
{
    float t = ds18b20_read_temperature(GPIO_DS18B20_SOLO);
    /* DS18B20 stub retorna -127.0 em erro. Convertemos para NAN. */
    if (t <= -126.0f) {
        return ultimo_temp_solo; /* devolve último válido se existir */
    }

    ultimo_temp_solo = t;
    return t;
}

int sensores_get_umid_solo_raw(void)
{
    int leitura = soil_moisture_leitura_bruta();
    if (leitura < 0) {
        return ultimo_umid_raw;
    }

    ultimo_umid_raw = leitura;
    return leitura;
}
