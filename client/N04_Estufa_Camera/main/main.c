#include <stdint.h>
#include <sys/stat.h> // Necessário para mkpath
#include <dirent.h>   // Necessário para opendir, readdir, closedir

// Certificado embutido
extern const uint8_t greense_cert_pem_start[] asm("_binary_greense_cert_pem_start");
extern const uint8_t greense_cert_pem_end[]   asm("_binary_greense_cert_pem_end");

#include <stdio.h>
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "driver/gpio.h"
#include "secrets.h"

// --- Novas Inclusões para SD Card ---
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"

// === DEFINIÇÕES ===
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define TAG "CAMERA"
#define TAG_WIFI "WIFI"
#define TAG_SD "SD_CARD" // Novo TAG para logs do SD

static EventGroupHandle_t s_wifi_event_group;

// === Pinos da ESP32-CAM (AI Thinker) ===
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define FLASH_GPIO_NUM     4

// === LED Wi-Fi ===
#define LED_WIFI_GPIO_NUM 33  // LED no lado oposto da câmera

// --- Definições para SD Card ---
#define MOUNT_POINT "/sdcard"

// === Handlers ===
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        gpio_set_level(LED_WIFI_GPIO_NUM, 0);  // Desliga o LED
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG_WIFI, "IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        gpio_set_level(LED_WIFI_GPIO_NUM, 1);  // Acende o LED
    }
}

void conexao_wifi_init(void) {
    s_wifi_event_group = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI(TAG_WIFI, "Wi-Fi conectando...");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG_WIFI, "Wi-Fi conectado");
    } else {
        ESP_LOGE(TAG_WIFI, "Falha na conexão Wi-Fi");
    }
}

bool conexao_wifi_is_connected(void) {
    wifi_ap_record_t ap_info;
    return (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
}

esp_err_t enviar_foto_para_raspberry(camera_fb_t *fb) {
    esp_http_client_config_t config = {
        .url = "https://camera.greense.com.br/upload",
        .method = HTTP_METHOD_POST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = (const char *)greense_cert_pem_start,
        .timeout_ms = 10000
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "image/jpeg");
    esp_http_client_set_post_field(client, (const char *)fb->buf, fb->len);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Imagem enviada. Status = %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "Erro ao enviar imagem: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

// --- Nova Função: Inicializa o Cartão SD ---
static esp_err_t init_sd_card(void) {
    esp_err_t ret;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    slot_config.width = 1; // 1-bit mode for compatibility (you can try 4 if your board supports it)
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP; // Habilita pull-ups internos

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, // Não formatar se falhar a montagem
        .max_files = 5,                  // Número máximo de arquivos que podem ser abertos simultaneamente
        .allocation_unit_size = 16 * 1024 // Tamanho da unidade de alocação
    };

    ESP_LOGI(TAG_SD, "Montando cartão SD...");
    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, NULL);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG_SD, "Falha ao montar sistema de arquivos. Você pode precisar formatar o cartão.");
        } else {
            ESP_LOGE(TAG_SD, "Falha ao inicializar cartão SD (%s).", esp_err_to_name(ret));
        }
        return ret;
    }
    ESP_LOGI(TAG_SD, "Cartão SD montado com sucesso.");

    // Opcional: Lista arquivos no diretório raiz para verificar
    ESP_LOGI(TAG_SD, "Listando arquivos em %s:", MOUNT_POINT);
    DIR *dir = opendir(MOUNT_POINT);
    if (dir == NULL) {
        ESP_LOGE(TAG_SD, "Falha ao abrir diretório");
        return ESP_FAIL;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI(TAG_SD, "%s", entry->d_name);
    }
    closedir(dir);
    return ESP_OK;
}

// --- Nova Função: Salva a Imagem no Cartão SD ---
static esp_err_t salvar_foto_no_sd(camera_fb_t *fb) {
    char file_name[64];
    struct tm timeinfo;
    time_t now;

    time(&now);
    if (now < 1609459200) {  // antes de 2021-01-01
    snprintf(file_name, sizeof(file_name), MOUNT_POINT "/img_%lu.jpg", esp_log_timestamp());
    } else {
    localtime_r(&now, &timeinfo);
    strftime(file_name, sizeof(file_name), MOUNT_POINT "/%Y%m%d_%H%M%S.jpg", &timeinfo);
    }

    ESP_LOGI(TAG_SD, "Salvando imagem em: %s", file_name);

    FILE *f = fopen(file_name, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG_SD, "Falha ao abrir arquivo para escrita.");
        return ESP_FAIL;
    }
    size_t bytes_written = fwrite(fb->buf, 1, fb->len, f);
    fclose(f);

    if (bytes_written == fb->len) {
        ESP_LOGI(TAG_SD, "Imagem salva com sucesso! (%d bytes)", bytes_written);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG_SD, "Erro ao escrever todos os bytes da imagem. Escritos: %d, Esperados: %d", bytes_written, fb->len);
        return ESP_FAIL;
    }
}

void task_envia_foto_periodicamente(void *pvParameter) {
    while (true) {
        // Assegura que o LED do Wi-Fi reflita o estado da conexão
        if (conexao_wifi_is_connected()) {
            gpio_set_level(LED_WIFI_GPIO_NUM, 0); // LED desligado quando conectado (ou ajuste sua lógica de LED)
        } else {
            gpio_set_level(LED_WIFI_GPIO_NUM, 1); // LED ligado quando desconectado
            ESP_LOGW(TAG, "Sem conexão Wi-Fi. Aguardando reconexão...");
            vTaskDelay(5000 / portTICK_PERIOD_MS); // Espera um pouco antes de tentar novamente
            continue; // Pula para a próxima iteração se não houver Wi-Fi
        }

        gpio_set_level(FLASH_GPIO_NUM, 1); // Acende o flash
        vTaskDelay(500 / portTICK_PERIOD_MS);

        camera_fb_t *fb = esp_camera_fb_get(); // Captura a imagem
        vTaskDelay(200 / portTICK_PERIOD_MS); // Pequeno atraso para estabilização, se necessário
        gpio_set_level(FLASH_GPIO_NUM, 0); // Desliga o flash

        if (fb) {
            // 1. Enviar a foto para o Raspberry Pi
            enviar_foto_para_raspberry(fb);

            // 2. Salvar a foto no cartão SD
            salvar_foto_no_sd(fb);

            esp_camera_fb_return(fb); // Libera o buffer da câmera
        } else {
            ESP_LOGE(TAG, "Erro ao capturar imagem");
        }

        vTaskDelay(2*60000 / portTICK_PERIOD_MS);  // Espera 1 minuto antes da próxima captura
    }
}

void start_camera() {
    camera_config_t config = {
        .pin_pwdn       = PWDN_GPIO_NUM,
        .pin_reset      = RESET_GPIO_NUM,
        .pin_xclk       = XCLK_GPIO_NUM,
        .pin_sscb_sda   = SIOD_GPIO_NUM,
        .pin_sscb_scl   = SIOC_GPIO_NUM,
        .pin_d7         = Y9_GPIO_NUM,
        .pin_d6         = Y8_GPIO_NUM,
        .pin_d5         = Y7_GPIO_NUM,
        .pin_d4         = Y6_GPIO_NUM,
        .pin_d3         = Y5_GPIO_NUM,
        .pin_d2         = Y4_GPIO_NUM,
        .pin_d1         = Y3_GPIO_NUM,
        .pin_d0         = Y2_GPIO_NUM,
        .pin_vsync      = VSYNC_GPIO_NUM,
        .pin_href       = HREF_GPIO_NUM,
        .pin_pclk       = PCLK_GPIO_NUM,
        .xclk_freq_hz   = 20000000,
        .ledc_timer     = LEDC_TIMER_0,
        .ledc_channel   = LEDC_CHANNEL_0,
        .pixel_format   = PIXFORMAT_JPEG,
        .frame_size     = FRAMESIZE_SVGA,
        .jpeg_quality   = 12,
        .fb_count       = 1
    };

    config.sccb_i2c_port = 0;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao iniciar câmera: 0x%x", err);
        return;
    }

    ESP_LOGI(TAG, "Câmera pronta");
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    // Inicializa GPIOs
    gpio_set_direction(FLASH_GPIO_NUM, GPIO_MODE_OUTPUT);
    gpio_set_level(FLASH_GPIO_NUM, 0);

    gpio_set_direction(LED_WIFI_GPIO_NUM, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_WIFI_GPIO_NUM, 1);  // Começa apagado ou como desejar

    // Inicializa o cartão SD
    if (init_sd_card() != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar o cartão SD. As imagens não serão salvas localmente.");
        // Se desejar, pode retornar ou continuar sem salvar imagens
    } else {
        // Teste simples de escrita
        FILE *f = fopen(MOUNT_POINT "/teste.txt", "w");
        if (f) {
            fprintf(f, "Teste de escrita no cartão SD.\n");
            fclose(f);
            ESP_LOGI(TAG_SD, "Teste de escrita: OK");
        } else {
            ESP_LOGE(TAG_SD, "Teste de escrita: FALHOU (verifique proteção ou sistema de arquivos)");
        }
    }


    start_camera();
    conexao_wifi_init();

    xTaskCreate(&task_envia_foto_periodicamente, "envia_foto_task", 8192, NULL, 5, NULL);
}
