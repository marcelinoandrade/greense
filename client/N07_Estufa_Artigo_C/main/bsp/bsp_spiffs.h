#ifndef BSP_SPIFFS_H
#define BSP_SPIFFS_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <stddef.h>

/**
 * @brief Inicializa o sistema de arquivos SPIFFS
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_spiffs_init(void);

/**
 * @brief Verifica se o SPIFFS está montado
 * @return true se montado, false caso contrário
 */
bool bsp_spiffs_is_mounted(void);

/**
 * @brief Calcula checksum CRC32 simples para validação de integridade
 * @param data Dados para calcular checksum
 * @param len Tamanho dos dados em bytes
 * @return Checksum CRC32 calculado
 */
uint32_t bsp_spiffs_calculate_checksum(const void *data, size_t len);

/**
 * @brief Salva frame parcial no SPIFFS com validação de integridade (sobrescreve arquivo anterior)
 * @param frame_index Índice do frame no buffer (0 a THERMAL_SAVE_INTERVAL-1)
 * @param frame Dados do frame térmico com timestamp
 * @param frame_size Tamanho do frame em bytes
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_spiffs_save_partial_frame(int frame_index, const void *frame, size_t frame_size);

/**
 * @brief Carrega todos os frames parciais do SPIFFS com validação de integridade
 * @param buffer Buffer para armazenar os frames
 * @param buffer_size Tamanho total do buffer em bytes
 * @param frame_size Tamanho esperado de cada frame em bytes (para validação)
 * @return Número de frames carregados e validados (0 se não houver dados), -1 em caso de erro
 */
int bsp_spiffs_load_partial_frames(void *buffer, size_t buffer_size, size_t frame_size);

/**
 * @brief Verifica se existem frames parciais no SPIFFS
 * @return true se existem dados parciais, false caso contrário
 */
bool bsp_spiffs_has_partial_data(void);

/**
 * @brief Remove arquivo de dados parciais (limpa após salvar no SD Card)
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_spiffs_clear_partial_data(void);

// ========== Funções para arquivo acumulativo ==========

/**
 * @brief Adiciona frame térmico ao arquivo acumulativo na SPIFFS
 * @param frame_data Dados do frame térmico
 * @param frame_size Tamanho do frame em bytes
 * @param timestamp Timestamp Unix do frame
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_spiffs_append_thermal_frame(const void *frame_data, size_t frame_size, time_t timestamp);

/**
 * @brief Obtém o tamanho atual do arquivo acumulativo na SPIFFS
 * @return Tamanho do arquivo em bytes (0 se não existir)
 */
size_t bsp_spiffs_get_thermal_file_size(void);

/**
 * @brief Lê todo o conteúdo do arquivo acumulativo
 * @param buffer Buffer para armazenar os dados
 * @param buffer_size Tamanho do buffer
 * @param bytes_read Ponteiro para retornar quantidade de bytes lidos
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_spiffs_read_thermal_file(void *buffer, size_t buffer_size, size_t *bytes_read);

/**
 * @brief Lê timestamps do arquivo de metadados acumulativo
 * @param timestamps Buffer para armazenar os timestamps
 * @param max_count Número máximo de timestamps que cabem no buffer
 * @param count_read Ponteiro para retornar quantidade de timestamps lidos
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_spiffs_read_thermal_metadata(time_t *timestamps, size_t max_count, size_t *count_read);

/**
 * @brief Limpa o arquivo acumulativo (remove dados e metadados)
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_spiffs_clear_thermal_file(void);

#endif // BSP_SPIFFS_H

