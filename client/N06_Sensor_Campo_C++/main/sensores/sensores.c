#include "sensores.h"
#include "ds18b20.h"
#include "soil_moisture.h" // <-- Novo include
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdlib.h>
#include <time.h>

// Pinos dos sensores
#define GPIO_DS18B20 4
// O pino do sensor de umidade (GPIO 34) é configurado 
// dentro de 'soil_moisture.c' (ADC_CHANNEL_6)

float randf(float min, float max) {
    return min + ((float)rand() / RAND_MAX) * (max - min);
}

void sensores_init(void) {
    srand((unsigned int) time(NULL));

    // Inicializa o sensor DS18B20
    ds18b20_init(GPIO_DS18B20);

    // Inicializa o sensor de Umidade do Solo
    // Ele precisa do caminho do SPIFFS para encontrar o "soil_calib.json"
    // Assumindo que foi montado em "/spiffs" no main.c
    soil_moisture_init("/spiffs"); 
}

sensor_data_t sensores_ler_dados(void) {
    sensor_data_t dados;

    // Leitura do sensor DS18B20
    dados.temp_solo = ds18b20_read_temperature(GPIO_DS18B20);

    // Leitura do sensor de Umidade do Solo
    dados.umid_solo = soil_moisture_umidade_percentual(); 
    

    return dados;
}
