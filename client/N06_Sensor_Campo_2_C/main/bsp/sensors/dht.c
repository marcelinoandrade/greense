#include "dht.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"

static const char *TAG = "DHT11";

static int wait_for_state(dht11_t dht11, int state, int timeout_us)
{
    gpio_set_direction(dht11.dht11_pin, GPIO_MODE_INPUT);
    int count = 0;
    while (gpio_get_level(dht11.dht11_pin) != state) {
        if (count >= timeout_us) return -1;
        count += 2;
        ets_delay_us(2);
    }
    return count;
}

static void hold_low(dht11_t dht11, int hold_time_us)
{
    gpio_set_direction(dht11.dht11_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(dht11.dht11_pin, 0);
    ets_delay_us(hold_time_us);
    gpio_set_level(dht11.dht11_pin, 1);
}

int dht11_read(dht11_t *dht11, int connection_timeout)
{
    if (dht11 == NULL) return -1;

    int waited = 0;
    int one_duration = 0;
    int zero_duration = 0;
    int timeout_counter = 0;
    uint8_t received_data[5] = {0};

    // Aumenta timeouts para melhor tolerância a variações de timing
    const int phase1_timeout = 60;  // Aumentado de 40 para 60
    const int phase2_timeout = 120; // Aumentado de 90 para 120
    const int phase3_timeout = 120; // Aumentado de 90 para 120

    while (timeout_counter < connection_timeout) {
        timeout_counter++;
        
        // Garante que o pino está em estado correto antes de iniciar
        gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(dht11->dht11_pin, 1);
        ets_delay_us(100); // Pequeno delay para estabilizar
        
        gpio_set_direction(dht11->dht11_pin, GPIO_MODE_INPUT);
        hold_low(*dht11, 18000);

        waited = wait_for_state(*dht11, 0, phase1_timeout);
        if (waited == -1) {
            ESP_LOGD(TAG, "Failed at phase 1 (tentativa %d/%d)", timeout_counter, connection_timeout);
            ets_delay_us(25000); // Aumentado de 20000 para 25000
            continue;
        }

        waited = wait_for_state(*dht11, 1, phase2_timeout);
        if (waited == -1) {
            ESP_LOGD(TAG, "Failed at phase 2 (tentativa %d/%d)", timeout_counter, connection_timeout);
            ets_delay_us(25000);
            continue;
        }

        waited = wait_for_state(*dht11, 0, phase3_timeout);
        if (waited == -1) {
            ESP_LOGD(TAG, "Failed at phase 3 (tentativa %d/%d)", timeout_counter, connection_timeout);
            ets_delay_us(25000);
            continue;
        }

        break;
    }

    if (timeout_counter == connection_timeout) {
        ESP_LOGD(TAG, "Timeout após %d tentativas", connection_timeout);
        return -1;
    }

    // Aumenta timeouts para leitura de bits (mais tolerante a variações)
    const int bit_high_timeout = 70;  // Aumentado de 58 para 70
    const int bit_low_timeout = 85;   // Aumentado de 74 para 85
    
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 8; j++) {
            zero_duration = wait_for_state(*dht11, 1, bit_high_timeout);
            if (zero_duration == -1) {
                ESP_LOGD(TAG, "Timeout lendo bit %d do byte %d", j, i);
                return -1;
            }
            
            one_duration = wait_for_state(*dht11, 0, bit_low_timeout);
            if (one_duration == -1) {
                ESP_LOGD(TAG, "Timeout lendo bit %d do byte %d", j, i);
                return -1;
            }
            
            received_data[i] |= (one_duration > zero_duration) << (7 - j);
        }
    }

    // Valida checksum
    int crc = received_data[0] + received_data[1] + received_data[2] + received_data[3];
    crc = crc & 0xff;
    if (crc == received_data[4]) {
        dht11->humidity = received_data[0] + received_data[1] / 10.0f;
        dht11->temperature = received_data[2] + received_data[3] / 10.0f;
        return 0;
    } else {
        ESP_LOGD(TAG, "Wrong checksum: esperado %d, recebido %d", crc, received_data[4]);
        return -1;
    }
}


