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

// Câmera Térmica
#define THERMAL_SAVE_INTERVAL 2  // Número de registros térmicos para acumular antes de salvar (padrão: 10)

// Agendamento de capturas de imagens
// Formato: pares de {hora, minuto} representando [H1:M1, H2:M2, ..., Hn:Mn]
// Exemplo: {0, 0, 0, 30, 1, 0, ...} representa [00:00, 00:30, 01:00, ...]

// Estrutura para representar um horário
typedef struct {
    int hour;    // Hora (0-23)
    int minute;  // Minuto (0-59)
} schedule_time_t;

// Agendamento para câmera espectro visual (foto a cada 30 minutos)
// Formato: [H1:M1, H2:M2, ..., Hn:Mn] - 48 horários (24h * 2)
#define CAMERA_VISUAL_SCHEDULE_SIZE 3
static const schedule_time_t camera_visual_schedule[CAMERA_VISUAL_SCHEDULE_SIZE] = {
    {14, 19},   {14, 20},  {14, 21}
};

// Agendamento para câmera térmica (foto a cada 30 minutos)
// Formato: [H1:M1, H2:M2, ..., Hn:Mn] - 48 horários (24h * 2)
#define CAMERA_THERMAL_SCHEDULE_SIZE 3
static const schedule_time_t camera_thermal_schedule[CAMERA_THERMAL_SCHEDULE_SIZE] = {
    {14, 33},   {14, 34},  {14, 35}
};

// Atuadores (definido em bsp/bsp_pins.h)
#include "bsp/bsp_pins.h"
// ACTUATOR_PIN já está definido em bsp_pins.h

#endif // CONFIG_H

