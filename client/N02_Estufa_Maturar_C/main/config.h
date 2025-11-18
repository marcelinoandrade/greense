#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi
#include "secrets.h"

// MQTT (futuramente)
#define MQTT_BROKER     "mqtt.greense.com.br" //"10.42.0.1"
#define MQTT_TOPIC      "estufa/maturar"
#define MQTT_CLIENT_ID  "Estufa_Maturar"
#define MQTT_KEEPALIVE  60

// Sensores
#define SENSOR_READ_INTERVAL 5  // segundos

// Atuadores
#define ACTUATOR_PIN    25

#endif // CONFIG_H
