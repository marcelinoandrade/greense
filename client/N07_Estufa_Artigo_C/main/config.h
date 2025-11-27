#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi
#include "secrets.h"

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
#define CAMERA_VISUAL_SCHEDULE_SIZE 48
static const schedule_time_t camera_visual_schedule[CAMERA_VISUAL_SCHEDULE_SIZE] = {
    {0, 0}, {0, 30}, {1, 0}, {1, 30}, {2, 0}, {2, 30}, {3, 0}, {3, 30},
    {4, 0}, {4, 30}, {5, 0}, {5, 30}, {6, 0}, {6, 30}, {7, 0}, {7, 30},
    {8, 0}, {8, 30}, {9, 0}, {9, 30}, {10, 0}, {10, 30}, {11, 0}, {11, 30},
    {12, 0}, {12, 30}, {13, 0}, {13, 30}, {14, 0}, {14, 30}, {15, 0}, {15, 30},
    {16, 0}, {16, 30}, {17, 0}, {17, 30}, {18, 0}, {18, 30}, {19, 0}, {19, 30},
    {20, 0}, {20, 30}, {21, 0}, {21, 30}, {22, 0}, {22, 30}, {23, 0}, {23, 30}
};

// Agendamento para câmera térmica (foto a cada 30 minutos)
// Formato: [H1:M1, H2:M2, ..., Hn:Mn] - 48 horários (24h * 2)
#define CAMERA_THERMAL_SCHEDULE_SIZE 48  // Total de elementos no array (corrigido)
static const schedule_time_t camera_thermal_schedule[CAMERA_THERMAL_SCHEDULE_SIZE] = {
 
    {0, 0}, {0, 30}, {1, 0}, {1, 30}, {2, 0}, {2, 30}, {3, 0}, {3, 30}, 
    {4, 0}, {4, 30}, {5, 0}, {5, 30}, {6, 0}, {6, 30}, {7, 0}, {7, 30},
    {8, 0}, {8, 30}, {9, 0}, {9, 30}, {10, 0}, {10, 30}, {11, 0}, {11, 30},
    {12, 0}, {12, 30}, {13, 0}, {13, 30}, {14, 0}, {14, 30}, {15, 0}, {15, 30},
    {16, 0}, {16, 30}, {17, 0}, {17, 30}, {18, 0}, {18, 30}, {19, 0}, {19, 30},
    {20, 0}, {20, 30}, {21, 0}, {21, 30}, {22, 0}, {22, 30}, {23, 0}, {23, 30}

};

// Atuadores (definido em bsp/bsp_pins.h)
#include "bsp/bsp_pins.h"
// ACTUATOR_PIN já está definido em bsp_pins.h

#endif // CONFIG_H

