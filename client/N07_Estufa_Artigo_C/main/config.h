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
#define CAMERA_VISUAL_SCHEDULE_SIZE 140
static const schedule_time_t camera_visual_schedule[CAMERA_VISUAL_SCHEDULE_SIZE] = {
    {6, 26},   {6, 27},  {6, 28},   {6, 29},   {6, 30},
    {6, 31},   {6, 32},  {6, 33},   {6, 34},   {6, 35},
    {6, 36},   {6, 37},  {6, 38},   {6, 39},   {6, 40},
    {6, 41},   {6, 42},  {6, 43},   {6, 44},   {6, 45},
    {6, 46},   {6, 47},  {6, 48},   {6, 49},   {6, 50},
    {6, 51},   {6, 52},  {6, 53},   {6, 54},   {6, 55},
    {6, 56},   {6, 57},  {6, 58},   {6, 59},   {7,  0},
    {7,  1},   {7,  2},  {7,  3},   {7,  4},   {7,  5},
    {7,  6},   {7,  7},  {7,  8},   {7,  9},   {7, 10},
    {7, 11},   {7, 12},  {7, 13},   {7, 14},   {7, 15},
    {7, 16},   {7, 17},  {7, 18},   {7, 19},   {7, 20},
    {7, 21},   {7, 22},  {7, 23},   {7, 24},   {7, 25},
    {7, 26},   {7, 27},  {7, 28},   {7, 29},   {7, 30},
    {7, 31},   {7, 32},  {7, 33},   {7, 34},   {7, 35},
    {7, 36},   {7, 37},  {7, 38},   {7, 39},   {7, 40},
    {7, 41},   {7, 42},  {7, 43},   {7, 44},   {7, 45},
    {7, 46},   {7, 47},  {7, 48},   {7, 49},   {7, 50},
    {7, 51},   {7, 52},  {7, 53},   {7, 54},   {7, 55},
    {7, 56},   {7, 57},  {7, 58},   {7, 59},   {8,  0},
    {8,  1},   {8,  2},  {8,  3},   {8,  4},   {8,  5},
    {8,  6},   {8,  7},  {8,  8},   {8,  9},   {8, 10},
    {8, 11},   {8, 12},  {8, 13},   {8, 14},   {8, 15},
    {8, 16},   {8, 17},  {8, 18},   {8, 19},   {8, 20},
    {8, 21},   {8, 22},  {8, 23},   {8, 24},   {8, 25},
    {8, 26},   {8, 27},  {8, 28},   {8, 29},   {8, 30},
    {8, 31},   {8, 32},  {8, 33},   {8, 34},   {8, 35},
    {8, 36},   {8, 37},  {8, 38},   {8, 39},   {8, 40},
    {8, 41},   {8, 42},  {8, 43},   {8, 44},   {8, 45}

};

// Agendamento para câmera térmica (foto a cada 30 minutos)
// Formato: [H1:M1, H2:M2, ..., Hn:Mn] - 48 horários (24h * 2)
#define CAMERA_THERMAL_SCHEDULE_SIZE 140  // Total de elementos no array (corrigido)
static const schedule_time_t camera_thermal_schedule[CAMERA_THERMAL_SCHEDULE_SIZE] = {
 
    {6, 26},   {6, 27},  {6, 28},   {6, 29},   {6, 30},
    {6, 31},   {6, 32},  {6, 33},   {6, 34},   {6, 35},
    {6, 36},   {6, 37},  {6, 38},   {6, 39},   {6, 40},
    {6, 41},   {6, 42},  {6, 43},   {6, 44},   {6, 45},
    {6, 46},   {6, 47},  {6, 48},   {6, 49},   {6, 50},
    {6, 51},   {6, 52},  {6, 53},   {6, 54},   {6, 55},
    {6, 56},   {6, 57},  {6, 58},   {6, 59},   {7,  0},
    {7,  1},   {7,  2},  {7,  3},   {7,  4},   {7,  5},
    {7,  6},   {7,  7},  {7,  8},   {7,  9},   {7, 10},
    {7, 11},   {7, 12},  {7, 13},   {7, 14},   {7, 15},
    {7, 16},   {7, 17},  {7, 18},   {7, 19},   {7, 20},
    {7, 21},   {7, 22},  {7, 23},   {7, 24},   {7, 25},
    {7, 26},   {7, 27},  {7, 28},   {7, 29},   {7, 30},
    {7, 31},   {7, 32},  {7, 33},   {7, 34},   {7, 35},
    {7, 36},   {7, 37},  {7, 38},   {7, 39},   {7, 40},
    {7, 41},   {7, 42},  {7, 43},   {7, 44},   {7, 45},
    {7, 46},   {7, 47},  {7, 48},   {7, 49},   {7, 50},
    {7, 51},   {7, 52},  {7, 53},   {7, 54},   {7, 55},
    {7, 56},   {7, 57},  {7, 58},   {7, 59},   {8,  0},
    {8,  1},   {8,  2},  {8,  3},   {8,  4},   {8,  5},
    {8,  6},   {8,  7},  {8,  8},   {8,  9},   {8, 10},
    {8, 11},   {8, 12},  {8, 13},   {8, 14},   {8, 15},
    {8, 16},   {8, 17},  {8, 18},   {8, 19},   {8, 20},
    {8, 21},   {8, 22},  {8, 23},   {8, 24},   {8, 25},
    {8, 26},   {8, 27},  {8, 28},   {8, 29},   {8, 30},
    {8, 31},   {8, 32},  {8, 33},   {8, 34},   {8, 35},
    {8, 36},   {8, 37},  {8, 38},   {8, 39},   {8, 40},
    {8, 41},   {8, 42},  {8, 43},   {8, 44},   {8, 45}


};

// Atuadores (definido em bsp/bsp_pins.h)
#include "bsp/bsp_pins.h"
// ACTUATOR_PIN já está definido em bsp_pins.h

#endif // CONFIG_H

