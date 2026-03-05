#ifndef BSP_CAMERA_H
#define BSP_CAMERA_H

#include "esp_err.h"
#include "esp_camera.h"

/**
 * @brief Inicializa a câmera ESP32-S3
 * @return ESP_OK em caso de sucesso, erro caso contrário
 */
esp_err_t bsp_camera_init(void);

/**
 * @brief Captura uma imagem da câmera
 * @return Ponteiro para o frame buffer ou NULL em caso de erro
 */
camera_fb_t* bsp_camera_capture(void);

/**
 * @brief Libera o frame buffer da câmera
 * @param fb Ponteiro para o frame buffer
 */
void bsp_camera_release(camera_fb_t* fb);

#endif // BSP_CAMERA_H

