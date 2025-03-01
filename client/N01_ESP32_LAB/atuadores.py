class ActuatorManager:
    def __init__(self, config):
        self.config = config

    def control_actuators(self, data):
        # Exemplo: Aciona um atuador se a temperatura estiver acima de um limite
        if data["temperatura"] > 30:
            print(f"Temperatura alta! Acionando atuador no pino {self.config.actuator_pin}.")
        else:
            print("Temperatura dentro do limite. Atuador desligado.")