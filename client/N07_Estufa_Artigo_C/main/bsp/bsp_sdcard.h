#ifndef BSP_SDCARD_H
#define BSP_SDCARD_H

#include "esp_err.h"
#include <stdbool.h>
#include <time.h>
#include <stddef.h>

/**
 * @brief Inicializa o cartão SD
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_sdcard_init(void);

/**
 * @brief Salva dados no cartão SD
 * @param filename Nome do arquivo
 * @param data Dados a serem salvos
 * @param data_len Tamanho dos dados
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_sdcard_save_file(const char* filename, const uint8_t* data, size_t data_len);

/**
 * @brief Verifica se o SD Card está montado
 * @return true se montado, false caso contrário
 */
bool bsp_sdcard_is_mounted(void);

/**
 * @brief Adiciona dados ao arquivo acumulativo no SD card (append)
 * @param filename Nome do arquivo
 * @param data Dados a serem adicionados
 * @param data_len Tamanho dos dados
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_sdcard_append_file(const char* filename, const uint8_t* data, size_t data_len);

/**
 * @brief Obtém o tamanho atual do arquivo no SD card
 * @param filename Nome do arquivo
 * @return Tamanho do arquivo em bytes (0 se não existir ou erro)
 */
size_t bsp_sdcard_get_file_size(const char* filename);

/**
 * @brief Verifica integridade dos dados escritos no SD card (read-after-write)
 * @param filename Nome do arquivo
 * @param expected_data Dados esperados
 * @param expected_len Tamanho dos dados esperados
 * @param offset Offset no arquivo para verificar (0 = verificar últimos bytes escritos)
 * @return ESP_OK se dados estão corretos, erro caso contrário
 */
esp_err_t bsp_sdcard_verify_write(const char* filename, const uint8_t* expected_data, size_t expected_len, size_t offset);

/**
 * @brief Lê frame térmico do arquivo acumulativo no SD card
 * @param filename Nome do arquivo acumulativo
 * @param frame_index Índice do frame (0-based)
 * @param frame_data Buffer para armazenar o frame (deve ter pelo menos 3072 bytes)
 * @param frame_size Tamanho esperado do frame (3072 bytes)
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_sdcard_read_thermal_frame(const char* filename, uint32_t frame_index, uint8_t* frame_data, size_t frame_size);

/**
 * @brief Lê timestamps do arquivo de metadados
 * @param filename Nome do arquivo de metadados
 * @param timestamps Buffer para armazenar timestamps
 * @param max_frames Número máximo de frames que cabem no buffer
 * @param frames_read Ponteiro para retornar quantidade de timestamps lidos
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_sdcard_read_thermal_timestamps(const char* filename, time_t* timestamps, size_t max_frames, size_t* frames_read);

/**
 * @brief Salva índice de progresso de envio (quantos frames foram enviados)
 * @param filename Nome do arquivo de índice
 * @param frames_sent Número de frames enviados
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_sdcard_save_send_index(const char* filename, uint32_t frames_sent);

/**
 * @brief Lê índice de progresso de envio
 * @param filename Nome do arquivo de índice
 * @param frames_sent Ponteiro para retornar quantidade de frames enviados
 * @return ESP_OK se arquivo existe, ESP_ERR_NOT_FOUND se não existe, outro erro caso contrário
 */
esp_err_t bsp_sdcard_read_send_index(const char* filename, uint32_t* frames_sent);

/**
 * @brief Renomeia arquivo no SD card
 * @param old_filename Nome antigo
 * @param new_filename Nome novo
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_sdcard_rename_file(const char* old_filename, const char* new_filename);

#endif // BSP_SDCARD_H

