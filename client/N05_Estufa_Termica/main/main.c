#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"

#define TAG "SENSOR_READER"
#define BAUD 115200

#define UART_PORT   UART_NUM_1
#define PIN_RX1     4
#define BUF_SIZE    (2048)

// A "fila" que o driver da UART usará para nos enviar eventos (como "dados chegaram!")
static QueueHandle_t uart_queue;

// Esta é uma "task" dedicada que ficará esperando por eventos da UART
static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t* dtmp = (uint8_t*) malloc(BUF_SIZE);
    while(1) {
        // Espera por um evento na fila. O código fica "dormindo" aqui até algo acontecer.
        if(xQueueReceive(uart_queue, (void * )&event, (TickType_t)portMAX_DELAY)) {
            bzero(dtmp, BUF_SIZE);
            
            if (event.type == UART_DATA) {
                // Evento de dados recebidos!
                // Lemos os dados que chegaram no buffer da UART
                int len = uart_read_bytes(UART_PORT, dtmp, event.size, pdMS_TO_TICKS(20));
                if (len > 0) {
                    // E escrevemos na USB para o PC ver
                    usb_serial_jtag_write_bytes(dtmp, len, 0);
                }
            } else {
                // Lidar com outros eventos da UART aqui (erro, buffer cheio, etc)
                ESP_LOGI(TAG, "uart event type: %d", event.type);
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

// Função para configurar e iniciar a USB nativa
static void usb_init(void) {
    usb_serial_jtag_driver_config_t usb_config = {
        .tx_buffer_size = 2048,
        .rx_buffer_size = 256
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_config));
}

void app_main(void)
{
    usb_init();

    // Configuração da UART
    uart_config_t uart_config = {
        .baud_rate = BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT, // Pode usar DEFAULT, XTAL ou REF_TICK
    };

    // NOVO: Na instalação do driver, passamos a fila de eventos.
    uart_driver_install(UART_PORT, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart_queue, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, UART_PIN_NO_CHANGE, PIN_RX1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    ESP_LOGI(TAG, "Leitor de sensor iniciado. Escutando em UART1 (RX: GPIO%d)", PIN_RX1);

    // NOVO: Cria a task que vai processar os eventos da UART
    xTaskCreate(uart_event_task, "uart_event_task", 4096, NULL, 12, NULL);
}