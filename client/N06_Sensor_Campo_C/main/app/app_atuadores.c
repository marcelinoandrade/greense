#include "app_atuadores.h"
#include "../bsp/actuators/bsp_led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

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

void atuadores_init(void)
{
    // Inicializa LED via BSP
    led_bsp_init();
    
    ap_ativo = false;
    clientes_conectados = 0;
    flash_pending = false;

    led_bsp_off();
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
    - else (ap_ativo && 0 clientes)   -> piscar (toggle ~700ms ON / 700ms OFF)

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
            led_bsp_on();
            vTaskDelay(pdMS_TO_TICKS(100));
            led_bsp_off();

            flash_pending = false;

            // pequena pausa antes de voltar ao controle normal
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // estado normal
        if (!ap_ativo)
        {
            // regra 4: AP inativo -> LED apagado contínuo
            led_bsp_off();
            vTaskDelay(delay_tick);
            continue;
        }

        // AP ativo
        if (clientes_conectados > 0)
        {
            // regra 2: alguém conectado -> LED apagado fixo
            led_bsp_off();
            vTaskDelay(delay_tick);
            continue;
        }

        // regra 1: AP ativo e ninguém conectado -> piscar
        blink_phase_on = !blink_phase_on;
        if (blink_phase_on)
        {
            led_bsp_on();
        }
        else
        {
            led_bsp_off();
        }

        vTaskDelay(delay_tick);
    }
}

