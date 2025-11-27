#pragma once

#include <stdbool.h>
#include "esp_err.h"

/* Estrutura de uma amostra registrada no CSV */
typedef struct {
    float temp_ar;     /* °C ar */
    float umid_ar;     /* % ar */
    float temp_solo;   /* °C solo */
    float umid_solo;   /* % solo convertida via calibração */
} log_entry_t;

/* Inicializa SPIFFS, carrega/calibra solo, prepara/analisa log_temp.csv.
 * - Monta /spiffs com label "spiffs".
 * - Cria cabeçalho CSV se não existir.
 * - Lê último índice N.
 * Retorna ESP_OK em caso de sucesso.
 */
esp_err_t data_logger_init(void);

/* Acrescenta uma linha (N, temp_ar, umid_ar, temp_solo, umid_solo) no CSV
 * e autoincrementa N interno.
 * Retorna true em caso de sucesso.
 */
bool data_logger_append(const log_entry_t *entry);

/* Imprime no log (ESP_LOGI) todo o conteúdo atual de /spiffs/log_temp.csv.
 * Usado apenas para debug no boot.
 */
void data_logger_dump_to_logcat(void);

/* Lê /spiffs/log_temp.csv e constrói um JSON compacto com os últimos pontos.
 * Formato:
 * {
 *   "temp_ar_points":   [ [idx, temp_ar_C], ... ],
 *   "umid_ar_points":   [ [idx, umid_ar_pct], ... ],
 *   "temp_solo_points": [ [idx, temp_solo_C], ... ],
 *   "umid_solo_points": [ [idx, umid_solo_pct], ... ]
 * }
 *
 * Retorna ponteiro malloc(). O chamador deve dar free().
 * Retorna NULL se falhar.
 */
char *data_logger_build_history_json(void);

/* Converte leitura ADC bruta de umidade do solo (solo seco = valor alto)
 * em % [0..100] usando a calibração em RAM.
 */
float data_logger_raw_to_pct(int leitura_raw);

/* Lê a calibração de solo atualmente carregada em RAM.
 * seco    = leitura ADC em solo "seco"
 * molhado = leitura ADC em solo "molhado"
 */
void data_logger_get_calibracao(float *seco, float *molhado);

/* Atualiza calibração em RAM e persiste em /spiffs/soil_calib.json.
 * Retorna ESP_OK se salvou.
 */
esp_err_t data_logger_set_calibracao(float seco, float molhado);

/* Apaga todos os dados armazenados (log CSV e calibração) e reinicia do zero.
 * - Remove /spiffs/log_temp.csv
 * - Remove /spiffs/soil_calib.json
 * - Reseta índice de linha para 1
 * - Reseta calibração para valores padrão
 * Retorna ESP_OK em caso de sucesso.
 */
esp_err_t data_logger_clear_all(void);

