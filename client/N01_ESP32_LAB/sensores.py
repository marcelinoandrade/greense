import random

class SensorManager:
    def __init__(self):
        pass
    
    def read_sensors(self):
        # Sensores ambientais na estufa
        temp = round(random.uniform(18, 30), 2)  # °C
        umid = round(random.uniform(50, 90), 2)  # %
        co2 = round(random.uniform(300, 1000), 2)  # ppm (partes por milhão)
        luz = round(random.uniform(100, 2000), 2)  # Lux

        # Sensores da solução nutritiva (reservatório interno na estufa)
        agua_min = round(random.uniform(0, 60), 2)  # Boia inferior
        agua_max = round(random.uniform(agua_min, 100), 2)  # Boia superior
        temp_reserv_int = round(random.uniform(15, 25), 2)  # Temperatura do reservatório interno

        # Sensores da solução nutritiva (reservatório externo)
        ph = round(random.uniform(5.5, 6.5), 2)  # pH ideal para hidroponia
        ec = round(random.uniform(1.0, 2.5), 2)  # mS/cm (Condutividade elétrica)
        temp_reserv_ext = round(random.uniform(15, 25), 2)  # Temperatura do reservatório externo

        dados = {
            # Sensores ambientais da estufa
            "temp": temp,
            "umid": umid,
            "co2": co2,
            "luz": luz,

            # Sensores da solução nutritiva (reservatório interno)
            "agua_min": agua_min,
            "agua_max": agua_max,
            "temp_reserv_int": temp_reserv_int,

            # Sensores da solução nutritiva (reservatório externo)
            "ph": ph,
            "ec": ec,
            "temp_reserv_ext": temp_reserv_ext,
        }
        
        
        return dados
