#ifndef DS18B20_H
#define DS18B20_H

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o barramento 1-Wire do DS18B20 no pino indicado.
 *
 * Pode ser chamada uma vez no boot. Armazena internamente o pino.
 */
void ds18b20_init(gpio_num_t gpio);

/**
 * @brief Lê temperatura em °C do DS18B20.
 *
 * Bloqueia ~750 ms (tempo de conversão).
 * Retorna temperatura em °C.
 * Retorna -127.0 em erro.
 */
float ds18b20_read_temperature(gpio_num_t gpio);

#ifdef __cplusplus
}
#endif

#endif // DS18B20_H
