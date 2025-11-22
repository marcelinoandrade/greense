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
// CAMERA_THERMAL_UPLOAD_URL para envio de dados térmicos
#ifndef CAMERA_THERMAL_UPLOAD_URL
#define CAMERA_THERMAL_UPLOAD_URL "https://camera02.greense.com.br/termica"
#endif

// Sensores
#define SENSOR_READ_INTERVAL 5  // segundos

// Câmera Térmica
#define THERMAL_SAVE_INTERVAL 3  // Número de registros térmicos para acumular antes de salvar (padrão: 10)

// Limite do arquivo acumulativo na SPIFFS (em bytes)
// Quando atingir este limite, os dados são migrados para o SD card
// APP_THERMAL_TOTAL = 24 * 32 = 768, sizeof(float) = 4, então 768 * 4 = 3072 bytes/frame
// Com THERMAL_SAVE_INTERVAL = 3: 3 frames * 3072 bytes = 9216 bytes (~9 KB)
// Para 100 frames, defina THERMAL_SAVE_INTERVAL = 100 (resultaria em ~300 KB)
#define THERMAL_SPIFFS_MAX_SIZE (THERMAL_SAVE_INTERVAL * 768 * sizeof(float))

// Nomes dos arquivos acumulativos
#define THERMAL_ACCUM_FILE_LOCAL "THERML.BIN"  // Arquivo acumulativo local no SD card
#define THERMAL_ACCUM_FILE_META_LOCAL "THERMLM.TXT"  // Arquivo de metadados local no SD card

// Tamanho do chunk para migração (em bytes) - reduz uso de memória
// Migra dados em pedaços menores ao invés de carregar tudo na memória
#define THERMAL_MIGRATION_CHUNK_SIZE (10 * 768 * sizeof(float))  // 10 frames por vez (~30 KB)

// Nomes dos arquivos de controle de envio
#define THERMAL_INDEX_FILE "THERML.idx"  // Arquivo de índice de progresso (4 bytes: uint32_t)
#define THERMAL_SENT_FILE "THERMS.BIN"   // Arquivo renomeado após envio completo
#define THERMAL_SENT_META_FILE "THERMSM.TXT"  // Metadados renomeado após envio completo

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
#define CAMERA_VISUAL_SCHEDULE_SIZE 45
static const schedule_time_t camera_visual_schedule[CAMERA_VISUAL_SCHEDULE_SIZE] = {
    {11, 30},    {11, 32},   {11, 34},    {11, 36},    {11, 38},
    {11, 40},    {11, 42},   {11, 44},    {11, 46},    {11, 48},
    {11, 50},    {11, 52},   {11, 54},    {11, 56},    {11, 58},
    {12, 0},     {12, 2},    {12, 4},     {12, 6},     {12, 8},
    {12, 10},    {12, 12},   {12, 14},    {12, 16},    {12, 18},
    {12, 20},    {12, 22},   {12, 24},    {12, 26},    {12, 28},
    {12, 30},    {12, 32},   {12, 34},    {12, 36},    {12, 38},
    {12, 40},    {12, 42},   {12, 44},    {12, 46},    {12, 48},
    {12, 50},    {12, 52},   {12, 54},    {12, 56},    {12, 58},

};

// Agendamento para câmera térmica (foto a cada 30 minutos)
// Formato: [H1:M1, H2:M2, ..., Hn:Mn] - 48 horários (24h * 2)
#define CAMERA_THERMAL_SCHEDULE_SIZE 45  // Total de elementos no array (corrigido)
static const schedule_time_t camera_thermal_schedule[CAMERA_THERMAL_SCHEDULE_SIZE] = {
 
    {11, 31},    {11, 33},   {11, 35},    {11, 37},    {11, 39},
    {11, 41},    {11, 43},   {11, 45},    {11, 47},    {11, 49},
    {11, 51},    {11, 53},   {11, 55},    {11, 57},    {11, 59},
    {12, 1},     {12, 3},    {12, 5},     {12, 7},     {12, 9},
    {12, 11},    {12, 13},   {12, 15},    {12, 17},    {12, 19},
    {12, 21},    {12, 23},   {12, 25},    {12, 27},    {12, 29},
    {12, 31},    {12, 33},   {12, 35},    {12, 37},    {12, 39},
    {12, 41},    {12, 43},   {12, 45},    {12, 47},    {12, 49},
    {12, 51},    {12, 53},   {12, 55},    {12, 57},    {12, 59},


};

// Atuadores (definido em bsp/bsp_pins.h)
#include "bsp/bsp_pins.h"
// ACTUATOR_PIN já está definido em bsp_pins.h

#endif // CONFIG_H

