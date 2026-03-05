#ifndef GUI_LED_H
#define GUI_LED_H

#include <stdint.h>

/**
 * @brief Inicializa o LED RGB WS2812
 */
void gui_led_init(void);

/**
 * @brief Define a cor do LED RGB
 * @param red Valor do vermelho (0-255)
 * @param green Valor do verde (0-255)
 * @param blue Valor do azul (0-255)
 */
void gui_led_set_color(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Define o estado do LED quando Wi-Fi está desconectado
 */
void gui_led_set_state_wifi_disconnected(void);

/**
 * @brief Define o estado do LED quando Wi-Fi está conectado
 */
void gui_led_set_state_wifi_connected(void);

/**
 * @brief Pisca o LED indicando sucesso (verde)
 */
void gui_led_flash_success(void);

/**
 * @brief Pisca o LED indicando erro (vermelho)
 */
void gui_led_flash_error(void);

/**
 * @brief Pisca o LED n vezes
 * @param times Número de vezes para piscar
 * @param on_ms Tempo em ms que o LED fica aceso
 * @param off_ms Tempo em ms que o LED fica apagado
 */
void gui_led_blink(int times, int on_ms, int off_ms);

#endif // GUI_LED_H
