#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <stdbool.h>

/**
 * @brief Estrutura para manter os dados de uma única leitura.
 * Corresponde às colunas do CSV.
 */
typedef struct {
    float temp_ar;
    float umid_ar;
    float temp_solo;
    float umid_solo;
} log_data_t;

/**
 * @brief Inicializa o logger e o sistema de arquivos SPIFFS.
 * * Monta a partição SPIFFS, verifica se o arquivo de log existe,
 * cria o cabeçalho se for novo, e lê o último índice.
 * * @param mount_path Ponto de montagem no VFS (ex: "/spiffs")
 * @param log_file_name Nome do arquivo de log (ex: "/log_temp.csv")
 * @return true se a inicialização for bem-sucedida, false caso contrário.
 */
bool data_logger_init(const char* mount_path, const char* log_file_name);

/**
 * @brief Adiciona uma nova entrada de log (linha) ao arquivo CSV.
 * * @param data A estrutura com os 4 valores dos sensores.
 * @return true se a escrita for bem-sucedida, false caso contrário.
 */
bool data_logger_append(log_data_t data);

/**
 * @brief Lê todo o arquivo de log e imprime no monitor serial.
 * (Equivalente ao seu 'ler_linhas' e depois imprimir)
 */
void data_logger_read_all(void);

/**
 * @brief Desmonta o sistema de arquivos SPIFFS.
 */
void data_logger_deinit(void);


#endif // DATA_LOGGER_H
