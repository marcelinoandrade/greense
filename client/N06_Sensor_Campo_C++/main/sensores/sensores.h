#ifndef SENSORES_H
#define SENSORES_H

/*
 * Camada de alto nível dos sensores.
 * Fornece leituras individuais usadas pelo restante do sistema.
 *
 * Unidades:
 *  - sensores_get_temp_ar()      -> °C (float)
 *  - sensores_get_umid_ar()      -> % UR (float 0..100)
 *  - sensores_get_temp_solo()    -> °C (float)
 *  - sensores_get_umid_solo_raw()-> leitura ADC bruta (int 0..4095)
 *
 * Caso uma leitura falhe:
 *  - funções que retornam float devolvem NAN
 *  - leitura bruta do solo devolve -1
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa todos os sensores físicos.
 *  - DS18B20 (temp solo)
 *  - ADC umidade solo
 *  - (futuro) sensor ar
 */
void sensores_init(void);

/* Temperatura do ar em °C.
 * Atualmente não há sensor físico de ar no seu código.
 * Retorna valor fixo (25.0) até você integrar sensor real.
 */
float sensores_get_temp_ar(void);

/* Umidade relativa do ar em %.
 * Atualmente não há sensor físico de UR no seu código.
 * Retorna valor fixo (50.0) até você integrar sensor real.
 */
float sensores_get_umid_ar(void);

/* Temperatura do solo em °C lida do DS18B20.
 * Retorna NAN se leitura falhar.
 */
float sensores_get_temp_solo(void);

/* Leitura bruta de umidade do solo (ADC).
 * Retorna -1 se leitura falhar.
 */
int sensores_get_umid_solo_raw(void);

#ifdef __cplusplus
}
#endif

#endif // SENSORES_H
