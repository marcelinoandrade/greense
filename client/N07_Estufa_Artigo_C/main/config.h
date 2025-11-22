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
#define CAMERA_VISUAL_SCHEDULE_SIZE 55
static const schedule_time_t camera_visual_schedule[CAMERA_VISUAL_SCHEDULE_SIZE] = {
    {10, 0},    {10, 1},   {10, 2},    {10, 2},    {10, 4},
    {10, 5},    {10, 6},   {10, 7},    {10, 8},    {10, 9},
    {10, 10},   {10, 11},  {10, 12},   {10, 13},   {10, 14},
    {10, 15},   {10, 16},  {10, 17},   {10, 18},   {10, 19},
    {10, 20},   {10, 21},  {10, 22},   {10, 23},   {10, 24},
    {10, 25},   {10, 26},  {10, 27},   {10, 28},   {10, 29},
    {10, 30},   {10, 31},  {10, 32},   {10, 33},   {10, 34},
    {10, 35},   {10, 36},  {10, 37},   {10, 38},   {10, 39},
    {10, 40},   {10, 41},  {10, 42},   {10, 43},   {10, 44},
    {10, 45},   {10, 46},  {10, 47},   {10, 48},   {10, 49},
    {10, 50},   {10, 51},  {10, 52},   {10, 53},   {10, 54}

};

// Agendamento para câmera térmica (foto a cada 30 minutos)
// Formato: [H1:M1, H2:M2, ..., Hn:Mn] - 48 horários (24h * 2)
#define CAMERA_THERMAL_SCHEDULE_SIZE 55  // Total de elementos no array (corrigido)
static const schedule_time_t camera_thermal_schedule[CAMERA_THERMAL_SCHEDULE_SIZE] = {
 
    {10, 0},    {10, 1},   {10, 2},    {10, 2},    {10, 4},
    {10, 5},    {10, 6},   {10, 7},    {10, 8},    {10, 9},
    {10, 10},   {10, 11},  {10, 12},   {10, 13},   {10, 14},
    {10, 15},   {10, 16},  {10, 17},   {10, 18},   {10, 19},
    {10, 20},   {10, 21},  {10, 22},   {10, 23},   {10, 24},
    {10, 25},   {10, 26},  {10, 27},   {10, 28},   {10, 29},
    {10, 30},   {10, 31},  {10, 32},   {10, 33},   {10, 34},
    {10, 35},   {10, 36},  {10, 37},   {10, 38},   {10, 39},
    {10, 40},   {10, 41},  {10, 42},   {10, 43},   {10, 44},
    {10, 45},   {10, 46},  {10, 47},   {10, 48},   {10, 49},
    {10, 50},   {10, 51},  {10, 52},   {10, 53},   {10, 54}


};

// Atuadores (definido em bsp/bsp_pins.h)
#include "bsp/bsp_pins.h"
// ACTUATOR_PIN já está definido em bsp_pins.h

#endif // CONFIG_H

