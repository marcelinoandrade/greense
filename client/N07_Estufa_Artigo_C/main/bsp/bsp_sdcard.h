#ifndef BSP_SDCARD_H
#define BSP_SDCARD_H

#include "esp_err.h"
#include <stdbool.h>

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

#endif // BSP_SDCARD_H

