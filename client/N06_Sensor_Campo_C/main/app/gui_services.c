#include "gui_services.h"
#include "esp_log.h"

static const char *TAG = "APP_GUI_SERVICES";

static const gui_services_t *registered_services = NULL;

void gui_services_register(const gui_services_t *services)
{
    if (services == NULL) {
        ESP_LOGE(TAG, "Tentativa de registrar serviços NULL");
        return;
    }
    
    registered_services = services;
    ESP_LOGI(TAG, "Serviços GUI registrados");
}

const gui_services_t* gui_services_get(void)
{
    if (registered_services == NULL) {
        ESP_LOGW(TAG, "Serviços GUI não registrados");
    }
    return registered_services;
}

