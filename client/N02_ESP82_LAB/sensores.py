import random

class SensorManager:
    def __init__(self):
        pass

    def read_sensors(self):
        temperatura = round(random.uniform(20, 35), 2)
        umidade = round(random.uniform(40, 80), 2)
        umidade_solo = round(random.uniform(10, 90), 2)
        luz = round(random.uniform(100, 1000), 2)

        dados = {
            "temperatura": temperatura,
            "umidade": umidade,
            "umidade_solo": umidade_solo,
            "luz": luz
        }
        return dados