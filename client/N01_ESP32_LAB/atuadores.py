class ActuatorManager:
    def __init__(self, config):
        """Configura os atuadores com base nos pinos de controle."""
        self.config = config

    def control_actuators(self, data):
        """Decide a ativação/desativação da bomba de água com base no nível da solução nutritiva."""

        actuators = {
            "bomba_agua": 0  # 0 = Desligado, 1 = Ligado
        }

        # Controle da bomba de água do reservatório interno
        if data["agua_min"] <= 20:
            actuators["bomba_agua"] = 1  # Liga a bomba para reabastecer
        elif data["agua_max"] >= 80:
            actuators["bomba_agua"] = 0  # Desliga a bomba se nível adequado

        return actuators
