#pragma once

#include <stdint.h>
#include <driver/rmt_tx.h>  // Atualizado para a nova API
#include <led_strip.h>


#ifdef __cplusplus
extern "C" {
#endif

// Definições públicas
#define LED_STRIP_PIN 16
#define LED_STRIP_NUM_LEDS 1

/**
 * @brief Inicializa todos os atuadores do sistema
 */
void atuadores_init(void);

/**
 * @brief Define a cor do LED RGB
 * @param red Valor do vermelho (0-255)
 * @param green Valor do verde (0-255)
 * @param blue Valor do azul (0-255)
 */
void led_set_color(uint8_t red, uint8_t green, uint8_t blue);

#ifdef __cplusplus
}
#endif