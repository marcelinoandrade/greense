#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi
#include "secrets.h"

// MQTT (futuramente)
#define MQTT_BROKER     "mqtt.greense.com.br"
#define MQTT_TOPIC      "estufa/artigo"
#define MQTT_CLIENT_ID  "Estufa_Artigo"
#define MQTT_KEEPALIVE  60

// HTTP/HTTPS Upload (definido em secrets.h)
// CAMERA_UPLOAD_URL está definido em secrets.h

// Sensores
#define SENSOR_READ_INTERVAL 5  // segundos

// Atuadores (definido em bsp/bsp_pins.h)
#include "bsp/bsp_pins.h"
// ACTUATOR_PIN já está definido em bsp_pins.h

#endif // CONFIG_H

