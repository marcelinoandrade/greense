#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

/* ============================================================
 * CONFIGURAÇÃO DE HARDWARE - ESP32-WROOM-32
 * ============================================================
 * Para portar para outra placa, altere apenas este arquivo.
 */

#define BOARD_NAME "ESP32_WROOM"

/* GPIOs */
#define BSP_GPIO_DS18B20        GPIO_NUM_4
#define BSP_GPIO_LED_STATUS     GPIO_NUM_2
/* DHT11 removido - aguardando AHT10 I2C */
// #define BSP_GPIO_DHT11          GPIO_NUM_5  /* GPIO5 liberada para uso futuro */

/* I2C para AHT10 e BH1750 */
#define BSP_I2C_NUM             I2C_NUM_0
#define BSP_I2C_SDA             GPIO_NUM_21
#define BSP_I2C_SCL             GPIO_NUM_22

/* ADC - Umidade do Solo */
#define BSP_ADC_SOIL_UNIT       ADC_UNIT_1
#define BSP_ADC_SOIL_CHANNEL    ADC_CHANNEL_6  // GPIO34
#define BSP_ADC_SOIL_BITWIDTH   ADC_BITWIDTH_12
#define BSP_ADC_SOIL_ATTEN      ADC_ATTEN_DB_12  // ADC_ATTEN_DB_11 deprecated, usa DB_12

/* Wi-Fi AP */
#define BSP_WIFI_AP_SSID        "ESP32_TEMP"
#define BSP_WIFI_AP_PASSWORD    "12345678"
#define BSP_WIFI_AP_CHANNEL     1
#define BSP_WIFI_AP_MAX_CONN    4

/* SPIFFS */
#define BSP_SPIFFS_LABEL        "spiffs"
#define BSP_SPIFFS_MOUNT        "/spiffs"
#define BSP_SPIFFS_MAX_FILES    5

/* Intervalo de amostragem dos sensores (em milissegundos) */
#define BSP_SENSOR_SAMPLE_INTERVAL_MS    10000  // 10 segundos (valor padrão)

/* Validação de configuração */
#if !defined(BSP_GPIO_DS18B20) || !defined(BSP_GPIO_LED_STATUS)
    #error "BSP: GPIOs não definidos"
#endif

