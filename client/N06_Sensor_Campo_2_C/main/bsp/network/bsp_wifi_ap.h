#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Callbacks para eventos Wi-Fi */
typedef void (*wifi_ap_client_connected_cb_t)(void);
typedef void (*wifi_ap_client_disconnected_cb_t)(void);

/**
 * @brief Inicializa Wi-Fi em modo Access Point
 * 
 * Usa configuração do board.h (BSP_WIFI_AP_*)
 * Não registra callbacks - use wifi_ap_register_callbacks() depois
 */
esp_err_t wifi_ap_init(void);

/**
 * @brief Registra callbacks para eventos Wi-Fi
 * 
 * Pode ser chamado antes ou depois de wifi_ap_init()
 * 
 * @param on_client_connected Callback chamado quando cliente conecta (pode ser NULL)
 * @param on_client_disconnected Callback chamado quando cliente desconecta (pode ser NULL)
 */
esp_err_t wifi_ap_register_callbacks(wifi_ap_client_connected_cb_t on_client_connected,
                                     wifi_ap_client_disconnected_cb_t on_client_disconnected);

/**
 * @brief Para o Wi-Fi AP
 */
esp_err_t wifi_ap_stop(void);

#ifdef __cplusplus
}
#endif
