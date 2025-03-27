from machine import Pin
import neopixel

class ActuatorManager:
    def __init__(self, config):
        """Configura os atuadores com base nos pinos de controle."""
        self.config = config
        self.last_state_min = 1.0
        self.last_state_max = 1.0
        self.pump_on = False

        # Inicializa o LED RGB no pino 21
        self.led = neopixel.NeoPixel(Pin(16), 1)
        self.set_led_color((0, 0, 10))  # Azul (inicial)

    def set_led_color(self, color_tuple):
        """Atualiza a cor do LED RGB."""
        self.led[0] = color_tuple
        self.led.write()

    def control_actuators(self, data):
        """Decide a ativação/desativação da bomba com base no nível da solução nutritiva."""
        actuators = {
            "bomba_agua": 0
        }

        if self.last_state_min == 1.0 and data["agua_min"] == 0.0:
            self.pump_on = True

        if self.last_state_max == 0.0 and data["agua_max"] == 1.0:
            self.pump_on = False

        actuators["bomba_agua"] = 1 if self.pump_on else 0

        # Atualiza o LED com base no estado da bomba
        if self.pump_on:
            self.set_led_color((10, 0, 0))   # Vermelho (bomba ligada)
        else:
            self.set_led_color((0, 10, 0))   # Verde (bomba desligada)

        self.last_state_min = data["agua_min"]
        self.last_state_max = data["agua_max"]

        return actuators
