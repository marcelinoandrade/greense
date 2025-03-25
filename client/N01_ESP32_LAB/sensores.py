import random
from machine import Pin, ADC

class SensorManager:
    def __init__(self):
        self.boia_min_pin = Pin(32, Pin.IN, Pin.PULL_UP)  # GPIO 32 para a boia de nível mínimo
        self.boia_max_pin = Pin(33, Pin.IN, Pin.PULL_UP)  # GPIO 33 para a boia de nível máximo
        self.ec_sensor_pin = ADC(Pin(34))  # GPIO 34 para o sensor de EC
        self.ec_sensor_pin.atten(ADC.ATTN_11DB)  # Configura a atenuação para faixa de até 3.3V
    
    def read_sensors(self):
        # Sensores ambientais na estufa
        temp = round(random.uniform(18, 30), 2)  # °C
        umid = round(random.uniform(50, 90), 2)  # %
        co2 = round(random.uniform(300, 1000), 2)  # ppm (partes por milhão)
        luz = round(random.uniform(100, 2000), 2)  # Lux

        # Sensores da solução nutritiva (reservatório interno na estufa)
        agua_min = float(self.boia_min_pin.value())  # Estado da boia de nível mínimo (0 ou 1)
        agua_max = float(self.boia_max_pin.value())  # Estado da boia de nível máximo (0 ou 1)
        temp_reserv_int = round(random.uniform(15, 25), 2)  # Temperatura do reservatório interno

        # Sensores da solução nutritiva (reservatório externo)
        ph = round(random.uniform(5.5, 6.5), 2)  # pH ideal para hidroponia
        raw_ec = self.ec_sensor_pin.read()  # Leitura bruta do sensor de EC
        ec = raw_ec * (3.3 / 4095)  # Convertendo de valor ADC para tensão
        temp_reserv_ext = round(random.uniform(15, 25), 2)  # Temperatura do reservatório externo

        dados = {
            "temp": temp,
            "umid": umid,
            "co2": co2,
            "luz": luz,
            "agua_min": agua_min,
            "agua_max": agua_max,
            "temp_reserv_int": temp_reserv_int,
            "ph": ph,
            "ec": ec,
            "temp_reserv_ext": temp_reserv_ext,
        }
        
        return dados
