#ifndef CONFIG_H
#define CONFIG_H

#include "secrets.h"
#include "greense.h"

#define MQTT_BROKER     "mqtt.greense.com.br"
#define MQTT_TOPIC      "estufa/germinar"
#define MQTT_CLIENT_ID  "Estufa_Germinar"
#define MQTT_KEEPALIVE  60

#define SENSOR_READ_INTERVAL 5

/* Fora da espuma e dentro da espuma, 5 out 2026. */
#define UMID_SOLO_ADC_SECO   2620
#define UMID_SOLO_ADC_UMIDO  1180

#define GREENSE_BOIAS 0
#define GREENSE_DHT_EXT 0

#endif
