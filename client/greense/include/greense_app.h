#ifndef GREENSE_APP_H
#define GREENSE_APP_H

/*
 * Arranque comum da N01 e da N02.
 * O nó preenche esta estrutura no config.h e chama greense_app_start().
 * Os ponteiros precisam continuar válidos: use literais do config.h.
 */

typedef struct {
    const char *wifi_ssid;
    const char *wifi_pass;
    const char *mqtt_broker;
    const char *mqtt_topic;
    const char *mqtt_client_id;
    int mqtt_keepalive;
    int intervalo_s;
    int umid_solo_adc_seco;
    int umid_solo_adc_umido;
    int boias;
} greense_app_config_t;

void greense_app_start(const greense_app_config_t *cfg);

#endif
