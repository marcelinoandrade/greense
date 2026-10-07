#ifndef CONFIG_H
#define CONFIG_H

#include "secrets.h"
#include "greense.h"

#define MQTT_BROKER     "mqtt.greense.com.br"
#define MQTT_TOPIC      "estufa/maturar"
#define MQTT_CLIENT_ID  "Estufa_Maturar"
#define MQTT_KEEPALIVE  60

#define SENSOR_READ_INTERVAL 5

/* Mesma espuma medida na N01 em 5 out 2026. Recalibrar no S3 se o bruto divergir. */
#define UMID_SOLO_ADC_SECO   2620
#define UMID_SOLO_ADC_UMIDO  1180

#define GREENSE_BOIAS 1

#endif
