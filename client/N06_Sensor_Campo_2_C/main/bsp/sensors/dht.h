#pragma once

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t dht11_pin;
    float humidity;
    float temperature;
} dht11_t;

/**
 * @brief Lê temperatura e umidade do DHT11.
 * @param dht11 Estrutura com o pino configurado.
 * @param connection_timeout Quantas tentativas antes de falhar.
 * @return 0 em sucesso, -1 em erro de comunicação.
 */
int dht11_read(dht11_t *dht11, int connection_timeout);

#ifdef __cplusplus
}
#endif


