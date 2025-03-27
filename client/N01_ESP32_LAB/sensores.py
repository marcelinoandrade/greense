import random
from machine import Pin, ADC, I2C
from lib.ahtx0 import AHT20
from lib.ens160 import ENS160
from lib.ph_sensor import PHSensor
import time

class SensorManager:
    def __init__(self):
        # Configuração do I2C
        self.i2c = I2C(0, scl=Pin(22), sda=Pin(21))
        
        # Sensores I2C
        self.aht20 = AHT20(self.i2c)
        self.ens160 = ENS160(self.i2c)
        
        # Sensores GPIO/ADC
        self.boia_min_pin = Pin(32, Pin.IN, Pin.PULL_UP)
        self.boia_max_pin = Pin(33, Pin.IN, Pin.PULL_UP)
        self.ec_sensor_pin = ADC(Pin(34))
        self.ec_sensor_pin.atten(ADC.ATTN_11DB)
        
        # Sensor de pH com calibração integrada
        self.ph_sensor = PHSensor(pin=35)
        
        # Para calibrar na primeira execução:
        # self.ph_sensor.calibrate_auto()

    def read_sensors(self):
        # Leitura do AHT20
        temp = round(self.aht20.temperature, 2)
        umid = round(self.aht20.relative_humidity, 2)
        
        # Atualiza compensação de temperatura
        self.ph_sensor.set_temperature(temp)
        
        # Leitura dos sensores
        co2 = float(self.ens160.get_eco2())
        ph_value = self.ph_sensor.read_ph()
        
        # Outros sensores (simulados)
        luz = round(random.uniform(100, 2000), 2)
        agua_min = float(self.boia_min_pin.value())
        agua_max = float(self.boia_max_pin.value())
        temp_reserv_int = round(random.uniform(15, 25), 2)
        raw_ec = self.ec_sensor_pin.read()
        ec = raw_ec * (3.3 / 4095)
        temp_reserv_ext = round(random.uniform(15, 25), 2)

        return {
            "temp": temp,
            "umid": umid,
            "co2": co2,
            "luz": luz,
            "agua_min": agua_min,
            "agua_max": agua_max,
            "temp_reserv_int": temp_reserv_int,
            "ph": ph_value,
            "ec": ec,
            "temp_reserv_ext": temp_reserv_ext,
        }