#ifndef ATUADORES_H
#define ATUADORES_H

#include "esp_err.h"
#include <stdbool.h>

// Inicializa GPIO e estado
void atuadores_init(void);

// Deve ser chamado quando o AP Wi-Fi sobe ou cai
void atuadores_set_ap_status(bool ap_ativo);

// Deve ser chamado quando um cliente entra no AP
void atuadores_cliente_conectou(void);

// Deve ser chamado quando um cliente sai do AP
void atuadores_cliente_desconectou(void);

// Deve ser chamado pela tarefa de LOG sempre que salva no SPIFFS
void atuadores_sinalizar_gravacao(void);

// Task que controla o LED conforme a regra de estados
void atuadores_task(void *pvParameter);

#endif // ATUADORES_H

