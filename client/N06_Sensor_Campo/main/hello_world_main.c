#include "esp_system.h"
#include "esp_log.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

static const char *TAG = "SDCARD";

void app_main(void)
{
    esp_err_t ret;

    // Configuração do barramento SPI
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 23,
        .miso_io_num = 19,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar o barramento SPI");
        return;
    }

    // Pino CS
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = 5;   // CS do cartão
    slot_config.host_id = host.slot;

    // Montagem do sistema de arquivos FAT
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t* card;
    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao montar cartão (%s)", esp_err_to_name(ret));
        return;
    }

    // Informações do cartão
    sdmmc_card_print_info(stdout, card);

    // Exemplo: criar um arquivo
    FILE* f = fopen("/sdcard/test.txt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo para escrita");
        return;
    }
    fprintf(f, "Teste de escrita no cartão SD com ESP32\n");
    fclose(f);
    ESP_LOGI(TAG, "Arquivo escrito com sucesso");

    // Desmontar se quiser
    // esp_vfs_fat_sdcard_unmount("/sdcard", card);
    // spi_bus_free(host.slot);
}
