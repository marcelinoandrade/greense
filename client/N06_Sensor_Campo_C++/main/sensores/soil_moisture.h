#ifndef SOIL_MOISTURE_H
#define SOIL_MOISTURE_H

#include <stdbool.h>

/**
 * @brief Inicializa o sensor de umidade do solo.
 * Configura o pino ADC e carrega os dados de calibração do SPIFFS.
 *
 * @param mount_path O ponto de montagem do SPIFFS (ex: "/spiffs").
 * @return true se a inicialização for bem-sucedida, false caso contrário.
 */
bool soil_moisture_init(const char* mount_path);

/**
 * @brief Lê o valor analógico bruto (0-4095) do sensor.
 *
 * @return O valor ADC bruto.
 */
int soil_moisture_leitura_bruta(void);

/**
 * @brief Calcula a umidade percentual (0-100%) com base na calibração.
 *
 * @return A umidade em percentual (0.0 a 100.0).
 */
float soil_moisture_umidade_percentual(void);

/**
 * @brief Salva novos valores de calibração na memória e no arquivo JSON.
 *
 * @param seco O valor ADC bruto lido com o sensor SECO.
 * @param molhado O valor ADC bruto lido com o sensor MOLHADO (imerso).
 * @return true se os dados foram salvos com sucesso, false caso contrário.
 */
bool soil_moisture_salvar_calibracao(float seco, float molhado);

#endif // SOIL_MOISTURE_H
