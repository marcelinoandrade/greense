#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi
#include "secrets.h"

// MQTT - Configurações da câmera térmica
#define MQTT_BROKER     "mqtt.greense.com.br"
#define MQTT_TOPIC      "estufa/termica"
#define MQTT_CLIENT_ID  "Camera_Termica"
#define MQTT_KEEPALIVE  60

// UART - Configuração da câmera térmica
#define UART_NUM        UART_NUM_1
#define TX_PIN          5
#define RX_PIN          4
#define BUF_SIZE        1600
#define BAUD_RATE       115200

// Intervalo de envio
#define SEND_INTERVAL_MS    60000  // 1 minuto

// LED de status Wi-Fi
#define LED_WIFI_GPIO_NUM 2

#endif // CONFIG_H