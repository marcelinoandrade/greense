#include "atuadores.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

/*
Estados requeridos:

1) AP ativo && clientes_conectados == 0  -> LED piscando
2) AP ativo && clientes_conectados > 0   -> LED apagado
3) Evento de gravação de log             -> flash curto (acende e apaga rápido)
4) AP inativo                            -> LED apagado

Estratégia:
- Variáveis globais protegidas por mutex leve (na prática acesso simples, só int/flags).
- atuadores_task roda em loop infinito e atualiza o LED.
- Sinal de gravação cria um pulso imediato (flash_pending = true).
*/

static volatile bool ap_ativo = false;
static volatile int clientes_conectados = 0;
static volatile bool flash_pending = false;

// controla hardware
static inline void led_on(void)  { gpio_set_level(LED_GPIO, 1); }
static inline void led_off(void) { gpio_set_level(LED_GPIO, 0); }

void atuadores_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    ap_ativo = false;
    clientes_conectados = 0;
    flash_pending = false;

    led_off();
}

void atuadores_set_ap_status(bool ativo)
{
    ap_ativo = ativo;
    // se AP caiu e ninguém conectado, LED deverá ficar apagado via task
    // se AP subiu, task decide piscar ou apagar dependendo de clientes_conectados
}

void atuadores_cliente_conectou(void)
{
    clientes_conectados++;
    if (clientes_conectados < 0) clientes_conectados = 0; // sanity
}

void atuadores_cliente_desconectou(void)
{
    clientes_conectados--;
    if (clientes_conectados < 0) clientes_conectados = 0;
}

// chamada no momento em que houve gravação no SPIFFS
void atuadores_sinalizar_gravacao(void)
{
    flash_pending = true;
}

/*
Loop principal do LED:

ordem de prioridade:
- Se flash_pending == true:
    faz flash curto: liga 100ms, desliga
    limpa flash_pending
    depois continua
- else estado normal:
    - se !ap_ativo                -> LED OFF fixo
    - else if clientes_conectados > 0 -> LED OFF fixo
    - else (ap_ativo && 0 clientes)   -> piscar (toggle ~300ms ON / 300ms OFF)

Importante: quando tem cliente conectado, o LED deve ficar apagado fixo
(então não pisca).

Vamos implementar com um pequeno state local.
*/

void atuadores_task(void *pvParameter)
{
    bool blink_phase_on = false;
    const TickType_t delay_tick = pdMS_TO_TICKS(700);

    for (;;)
    {
        if (flash_pending)
        {
            // flash curto
            led_on();
            vTaskDelay(pdMS_TO_TICKS(100));
            led_off();

            flash_pending = false;

            // pequena pausa antes de voltar ao controle normal
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // estado normal
        if (!ap_ativo)
        {
            // regra 4: AP inativo -> LED apagado contínuo
            led_off();
            vTaskDelay(delay_tick);
            continue;
        }

        // AP ativo
        if (clientes_conectados > 0)
        {
            // regra 2: alguém conectado -> LED apagado fixo
            led_off();
            vTaskDelay(delay_tick);
            continue;
        }

        // regra 1: AP ativo e ninguém conectado -> piscar
        blink_phase_on = !blink_phase_on;
        if (blink_phase_on)
        {
            led_on();
        }
        else
        {
            led_off();
        }

        vTaskDelay(delay_tick);
    }
}
