#ifndef GREENSE_H
#define GREENSE_H

/*
 * Mapa de pinos das placas Zero usadas no GreenSe.
 * O firmware fala em função (ar, solo, luz, boia, LED).
 * O mapa segue o alvo do projeto (sdkconfig): N02 é esp32s3, N01 é esp32c6.
 * Quem define isso é o idf.py set-target, uma vez em cada pasta.
 */

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3)

/*
 * ESP32-S3 Zero.
 * LED onboard no GPIO21. USB nos GPIO19 e GPIO20. BOOT no GPIO0.
 * UART de log nos GPIO43 (TX) e GPIO44 (RX). PSRAM nos GPIO33 a GPIO37.
 * Umidade do solo no GPIO1, que e o ADC1 canal 0. Nao usar o ADC2 com Wi-Fi.
 */
#define GREENSE_PLACA_NOME          "ESP32-S3 Zero"
#define GREENSE_PIN_AR              GPIO_NUM_4
#define GREENSE_PIN_SOLO_TEMP       GPIO_NUM_5
#define GREENSE_PIN_LUZ             GPIO_NUM_6
#define GREENSE_PIN_SOLO_UMID       GPIO_NUM_1
#define GREENSE_ADC_SOLO_UNIT       ADC_UNIT_1
#define GREENSE_ADC_SOLO_CHANNEL    ADC_CHANNEL_0
#define GREENSE_PIN_BOIA_MIN        GPIO_NUM_7
#define GREENSE_PIN_BOIA_MAX        GPIO_NUM_8
#define GREENSE_LED_GPIO            GPIO_NUM_21
#define GREENSE_LED_COUNT           1

#elif defined(CONFIG_IDF_TARGET_ESP32C6)

/*
 * ESP32-C6 Zero, o mesmo fio da N01.
 * LED onboard no GPIO8. USB nos GPIO12 e GPIO13. BOOT no GPIO9.
 * Umidade do solo no GPIO0, que e o ADC1 canal 0.
 */
#define GREENSE_PLACA_NOME          "ESP32-C6 Zero"
#define GREENSE_PIN_AR              GPIO_NUM_18
#define GREENSE_PIN_SOLO_TEMP       GPIO_NUM_19
#define GREENSE_PIN_LUZ             GPIO_NUM_20
#define GREENSE_PIN_SOLO_UMID       GPIO_NUM_0
#define GREENSE_ADC_SOLO_UNIT       ADC_UNIT_1
#define GREENSE_ADC_SOLO_CHANNEL    ADC_CHANNEL_0
#define GREENSE_PIN_BOIA_MIN        GPIO_NUM_1
#define GREENSE_PIN_BOIA_MAX        GPIO_NUM_2
#define GREENSE_LED_GPIO            GPIO_NUM_8
#define GREENSE_LED_COUNT           1

#else
#error "Mapa de pinos apenas para esp32s3 e esp32c6. Rode: idf.py set-target esp32s3 ou esp32c6"
#endif

#endif
