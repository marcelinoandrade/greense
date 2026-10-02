#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi
#include "secrets.h"

// MQTT (futuramente)
#define MQTT_BROKER     "mqtt.greense.com.br" //"10.42.0.1"
#define MQTT_TOPIC      "estufa/germinar"
#define MQTT_CLIENT_ID  "Estufa_Germinar"
#define MQTT_KEEPALIVE  60

// Sensores na Placa Mini Estufa (ESP32-C6 Zero)
#define SENSOR_READ_INTERVAL 5  // segundos
#define GPIO_DHT11          18  // umidade e temperatura do ar
#define GPIO_DS18B20        19  // temperatura do solo
#define GPIO_SENSOR_LUZ     20  // HW-072, saida digital
#define GPIO_UMID_SOLO      0   // HD-38, saida analogica (ADC1 CH0)

#endif // CONFIG_H
