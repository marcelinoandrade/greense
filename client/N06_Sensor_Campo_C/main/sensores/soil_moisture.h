#ifndef SOIL_MOISTURE_H
#define SOIL_MOISTURE_H

#include "esp_err.h"

/**
 * Driver mínimo do sensor de umidade do solo.
 * Faz só leitura ADC bruta.
 * A conversão para porcentagem e a calibração
 * ficam no data_logger.c.
 *
 * Hardware:
 *  - Sensor analógico no GPIO34 (ADC1_CH6)
 *  - 3V3
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Inicializa o ADC oneshot no canal de umidade do solo.
 * Deve ser chamado uma vez no boot.
 *
 * Retorna ESP_OK se sucesso.
 */
esp_err_t soil_moisture_init(void);

/**
 * Lê o valor ADC bruto atual.
 *
 * Retorna 0..4095 em sucesso.
 * Retorna -1 em erro.
 */
int soil_moisture_leitura_bruta(void);

#ifdef __cplusplus
}
#endif

#endif // SOIL_MOISTURE_H
