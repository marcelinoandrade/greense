#include "app_thermal.h"
#include "../bsp/bsp_uart.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

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
    uint8_t *buf = malloc(BSP_UART_BUF_MAX);
    if (!buf) return false;
    
    size_t used = 0;
    TickType_t t0 = xTaskGetTickCount();

    while ((xTaskGetTickCount() - t0) < timeout_ticks) {
        int n = bsp_uart_read(buf + used, BSP_UART_BUF_MAX - used, pdMS_TO_TICKS(50));
        if (n > 0) used += n;
        
        if (used < 4) continue;

        size_t idx = find_header_5A5A(buf, used);
        if (idx == used) {
            if (used > 1) {
                buf[0] = buf[used - 1];
                used = 1;
            }
            continue;
        }
        
        if (idx > 0) {
            memmove(buf, buf + idx, used - idx);
            used -= idx;
            if (used < 4) continue;
        }

        uint16_t flen = buf[2] | (buf[3] << 8);
        size_t need = 4 + flen;
        
        if (used < need) continue;

        bool ok = parse_frame(buf + 4, flen, out);
        memmove(buf, buf + need, used - need);
        used -= need;
        
        if (ok) {
            free(buf);
            return true;
        }
    }
    
    free(buf);
    return false;
}

