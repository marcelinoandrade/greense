#ifndef GUI_LED_H
#define GUI_LED_H

// Funções de controle de estado (usadas pela task do LED)
void gui_led_set_state_wifi_disconnected(void);
void gui_led_set_state_wifi_connected_no_internet(void);
void gui_led_set_state_wifi_connected_with_internet(void);
void gui_led_flash_success(void);
void gui_led_flash_error(void);

// Task dedicada para controlar o LED (deve ser criada no app_main)
void gui_led_task(void *pvParameters);

// Funções auxiliares (mantidas para compatibilidade)
void gui_led_blink(int times, int on_ms, int off_ms);

#endif // GUI_LED_H




