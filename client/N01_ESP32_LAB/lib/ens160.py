import machine
import time

# Constantes do ENS160 (mantidas da sua implementação)
ENS160_ADDR = 0x53
ENS160_PART_ID = 0x160
ENS160_OPMODE_REG = 0x10
ENS160_TEMP_IN_REG = 0x13
ENS160_RH_IN_REG = 0x15
ENS160_DATA_ECO2_REG = 0x24
ENS160_STANDARD_MODE = 0x02

class ENS160:
    def __init__(self, i2c, scl_pin=22, sda_pin=21):
        self.i2c = i2c
        try:
            # Configura modo padrão
            buf = bytearray([ENS160_STANDARD_MODE])
            self.i2c.writeto_mem(ENS160_ADDR, ENS160_OPMODE_REG, buf)
            time.sleep(0.5)
        except OSError as e:
            print("Erro ao inicializar ENS160:", e)

    def calibrate(self, temp, humidity):
        """Compensação ambiental"""
        # Temperatura (convertida para Kelvin * 64)
        temp_cal = int((temp + 273.15) * 64)
        buf_temp = bytearray([
            temp_cal & 0xFF, 
            (temp_cal >> 8) & 0xFF
        ])
        self.i2c.writeto_mem(ENS160_ADDR, ENS160_TEMP_IN_REG, buf_temp)
        
        # Umidade (convertida para % * 512)
        hum_cal = int(humidity * 512)
        buf_hum = bytearray([
            hum_cal & 0xFF,
            (hum_cal >> 8) & 0xFF
        ])
        self.i2c.writeto_mem(ENS160_ADDR, ENS160_RH_IN_REG, buf_hum)
        time.sleep(0.1)

    def get_eco2(self):
        """Retorna CO2 em ppm"""
        buf = self.i2c.readfrom_mem(ENS160_ADDR, ENS160_DATA_ECO2_REG, 2)
        return (buf[1] << 8) | buf[0]