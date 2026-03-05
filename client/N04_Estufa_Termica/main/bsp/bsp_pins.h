#ifndef BSP_PINS_H
#define BSP_PINS_H

/**
 * @file bsp_pins.h
 * @brief Definição de pinagem para ESP32-S3 WROOM (N16R8)
 * 
 * Baseado no diagrama de pinout da ESP32-S3 WROOM development board
 */

// ===== LEDs =====
// Na ESP32-S3, os LEDs indicadores são controlados internamente
// Para LED externo ou WS2812, usar GPIO48 (WS2812) ou GPIO disponível
#define LED_STATUS_GPIO        48  // WS2812 ou LED externo
#define LED_ON_GPIO            38  // LED ON (se disponível na placa)

// ===== Atuadores =====
#define ACTUATOR_PIN           21  // GPIO21 - disponível e seguro
#define ACTUATOR_PIN_ALT       20  // GPIO20 - alternativa

// ===== Sensores I2C =====
// NOTA: GPIO8 e GPIO9 são usados pela câmera (CAM_Y4 e CAM_Y3)
// Se precisar de I2C para sensores externos, usar outros pinos (ex: GPIO1/2, GPIO10/11)
#define I2C_SDA_GPIO           1   // GPIO1 - I2C Data (alternativa, não conflita com câmera)
#define I2C_SCL_GPIO           2   // GPIO2 - I2C Clock (alternativa, não conflita com câmera)

// ===== UART =====
#define UART_TX_GPIO           43  // U0TXD (Console/Debug)
#define UART_RX_GPIO           44  // U0RXD (Console/Debug)

// ===== UART para MLX90640 (Câmera Térmica) =====
#define UART_THERMAL_PORT       UART_NUM_1
#define UART_THERMAL_RX_GPIO    14  // GPIO14 - RX (recebe dados do MLX90640)
#define UART_THERMAL_TX_GPIO    3   // GPIO3 - TX (configurado mas não usado fisicamente)
#define UART_THERMAL_BAUD       115200
#define UART_THERMAL_BUF_MAX    8192

// ===== ADC (Analog-to-Digital Converter) =====
// Canais ADC disponíveis para sensores analógicos
#define ADC_SENSOR_1           ADC1_CH0  // GPIO1
#define ADC_SENSOR_2           ADC1_CH1  // GPIO2
#define ADC_SENSOR_3           ADC1_CH2  // GPIO3
#define ADC_SENSOR_4           ADC1_CH3  // GPIO4

// ===== Câmera (ESP32-S3 integrada) =====
#define CAM_PWDN_GPIO          -1  // Power Down (não usado ou -1)
#define CAM_RESET_GPIO         47  // Reset (ou -1 se não usado)
#define CAM_SIOD_GPIO          4   // CAM_SIOD (I2C Data)
#define CAM_SIOC_GPIO          5   // CAM_SIOC (I2C Clock)
#define CAM_VSYNC_GPIO         6   // CAM_VSYNC
#define CAM_HREF_GPIO          7   // CAM_HREF
#define CAM_XCLK_GPIO          15  // CAM_XCLK
#define CAM_PCLK_GPIO          13  // CAM_PCLK
// Pinos de dados da câmera (8 bits) - Padrão ESP32-S3 WROOM
#define CAM_Y9_GPIO            16  // CAM_Y9 (D7)
#define CAM_Y8_GPIO            17  // CAM_Y8 (D6)
#define CAM_Y7_GPIO            18  // CAM_Y7 (D5)
#define CAM_Y6_GPIO            12  // CAM_Y6 (D4) - corrigido: era 9
#define CAM_Y5_GPIO            10  // CAM_Y5 (D3) - corrigido: era 12
#define CAM_Y4_GPIO            8   // CAM_Y4 (D2) - corrigido: era 11
#define CAM_Y3_GPIO            9   // CAM_Y3 (D1) - corrigido: era 10
#define CAM_Y2_GPIO            11  // CAM_Y2 (D0) - corrigido: era 8
// Flash LED para iluminação
#define CAM_FLASH_GPIO         21  // Flash LED (ou ACTUATOR_PIN)

// ===== SD Card (ESP32-S3 integrado) =====
#define SD_DATA_GPIO           40  // SD_DATA (SDMMC Data)
#define SD_CLK_GPIO             39  // SD_CLK (SDMMC Clock)
#define SD_CMD_GPIO             38  // SD_CMD (SDMMC Command)
#define SD_MOUNT_POINT          "/sdcard"  // Ponto de montagem

// ===== Pinos de Alimentação =====
// 3V3, 5V, GND - não são GPIOs, apenas referência

// ===== Pinos Especiais =====
#define BOOT_GPIO              0   // GPIO0 - Boot button
#define RST_GPIO               -1  // Reset não é GPIO

// ===== Notas =====
// - GPIO37, GPIO36, GPIO35 são usados para PSRAM - evitar uso
// - GPIO43, GPIO44 são UART0 (USB) - usar com cuidado
// - GPIO0 é usado para boot - usar com cuidado
// - GPIOs 14, 19-21 são seguros para uso geral
// - GPIO14 é usado para RX da câmera térmica (UART1)

#endif // BSP_PINS_H

