#include "app_thermal.h"
#include "../bsp/bsp_uart.h"
#include "../bsp/bsp_pins.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define TAG "APP_THERMAL"

static size_t find_header_5A5A(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i + 1 < len; i++) {
        if (buf[i] == 0x5A && buf[i + 1] == 0x5A) return i;
    }
    return len;
}

static bool parse_frame(const uint8_t *payload, size_t plen, float out[APP_THERMAL_TOTAL]) {
    const uint8_t *px = NULL;
    
    if (plen == 1538) {
        px = payload;
        plen = 1536;
    } else if (plen == 1543) {
        px = payload + 5;
        plen = 1536;
    } else {
        return false;
    }

    if (plen != 1536) return false;
    
    for (int i = 0; i < APP_THERMAL_TOTAL; i++) {
        int16_t v = (int16_t)(px[2 * i] | (px[2 * i + 1] << 8));
        out[i] = ((float)v) / 100.0f;
    }
    
    float tmin = out[0], tmax = out[0];
    for (int i = 1; i < APP_THERMAL_TOTAL; i++) {
        if (out[i] < tmin) tmin = out[i];
        if (out[i] > tmax) tmax = out[i];
    }
    
    return !(tmin < -40.0f || tmax > 200.0f);
}

bool app_thermal_capture_frame(float out[APP_THERMAL_TOTAL], TickType_t timeout_ticks) {
    uint8_t *buf = malloc(UART_THERMAL_BUF_MAX);
    if (!buf) {
        ESP_LOGE(TAG, "Falha ao alocar buffer para UART");
        return false;
    }
    
    size_t used = 0;
    TickType_t t0 = xTaskGetTickCount();
    bool first_read = true;

    ESP_LOGI(TAG, "Iniciando captura de frame térmico (timeout: %lu ms)", 
             (unsigned long)(timeout_ticks * portTICK_PERIOD_MS));

    while ((xTaskGetTickCount() - t0) < timeout_ticks) {
        int n = bsp_uart_read(buf + used, UART_THERMAL_BUF_MAX - used, pdMS_TO_TICKS(50));
        if (n > 0) {
            used += n;
            if (first_read) {
                ESP_LOGI(TAG, "✅ Primeiros %d bytes recebidos do UART (total acumulado: %d)", n, used);
                // Mostra os primeiros bytes em hex para debug
                if (used >= 4) {
                    ESP_LOGI(TAG, "Primeiros 4 bytes: 0x%02X 0x%02X 0x%02X 0x%02X", 
                             buf[0], buf[1], buf[2], buf[3]);
                }
                first_read = false;
            } else {
                ESP_LOGD(TAG, "Lidos %d bytes do UART (total: %d)", n, used);
            }
        } else if (n == 0 && used == 0) {
            // Timeout sem receber nada ainda
            TickType_t elapsed = xTaskGetTickCount() - t0;
            if (elapsed % pdMS_TO_TICKS(1000) < pdMS_TO_TICKS(100)) {
                ESP_LOGD(TAG, "Aguardando dados... (elapsed: %lu ms)", 
                         (unsigned long)(elapsed * portTICK_PERIOD_MS));
            }
        }
        
        if (used < 4) continue;

        size_t idx = find_header_5A5A(buf, used);
        if (idx == used) {
            // Header 0x5A5A não encontrado ainda
            if (used >= 16) {
                // Se já recebeu bastante dados mas não encontrou header, mostra para debug
                ESP_LOGW(TAG, "Header 0x5A5A não encontrado. Primeiros 16 bytes recebidos:");
                for (int i = 0; i < 16 && i < used; i++) {
                    ESP_LOGW(TAG, "  [%d] = 0x%02X", i, buf[i]);
                }
                // Mantém apenas o último byte (pode ser parte do próximo header)
                if (used > 1) {
                    buf[0] = buf[used - 1];
                    used = 1;
                }
            } else if (used > 1) {
                buf[0] = buf[used - 1];
                used = 1;
            }
            continue;
        }
        
        if (idx > 0) {
            ESP_LOGI(TAG, "Header 0x5A5A encontrado na posição %d (descartando %d bytes)", idx, idx);
            memmove(buf, buf + idx, used - idx);
            used -= idx;
            if (used < 4) continue;
        }

        uint16_t flen = buf[2] | (buf[3] << 8);
        size_t need = 4 + flen;
        
        ESP_LOGI(TAG, "Frame detectado: tamanho payload = %d bytes, total necessário = %d bytes (recebido: %d)", 
                 flen, need, used);
        
        if (used < need) {
            ESP_LOGD(TAG, "Aguardando mais dados: necessário %d, recebido %d", need, used);
            continue;
        }

        bool ok = parse_frame(buf + 4, flen, out);
        memmove(buf, buf + need, used - need);
        used -= need;
        
        if (ok) {
            ESP_LOGI(TAG, "✅ Frame térmico parseado com sucesso!");
            free(buf);
            return true;
        } else {
            ESP_LOGW(TAG, "❌ Frame inválido detectado (tamanho: %d). Tentando próximo...", flen);
        }
    }
    
    if (used > 0) {
        ESP_LOGW(TAG, "Timeout ao capturar frame térmico. Recebidos %d bytes, mas frame incompleto ou inválido.", used);
        if (used >= 4) {
            ESP_LOGW(TAG, "Primeiros 4 bytes: 0x%02X 0x%02X 0x%02X 0x%02X", 
                     buf[0], buf[1], buf[2], buf[3]);
        }
    } else {
        ESP_LOGW(TAG, "Timeout ao capturar frame térmico. Nenhum dado recebido (0 bytes).");
        ESP_LOGW(TAG, "Verifique: 1) Câmera térmica conectada ao GPIO%d? 2) Alimentação OK? 3) Baudrate correto?", 
                 UART_THERMAL_RX_GPIO);
    }
    free(buf);
    return false;
}

