from machine import ADC, Pin
import time

class PHSensor:
    def __init__(self, pin=35):
        """
        Inicializa o sensor de pH com métodos de calibração integrados
        :param pin: Pino ADC (GPIO34 recomendado para ESP32)
        """
        self.adc = ADC(Pin(pin))
        self.adc.atten(ADC.ATTN_11DB)
        self.adc.width(ADC.WIDTH_12BIT)
        
        # Calibração padrão (deve ser ajustada)
        self.cal_points = {
            'acid': {'ph': 4.0, 'voltage': 1.5},
            'neutral': {'ph': 7.0, 'voltage': 2.0},
            'base': {'ph': 9.0, 'voltage': 2.5}
        }
        self.temperature = 25.0
        self.last_valid = 7.0

    def calibrate_point(self, point_name, known_ph):
        """
        Calibra um ponto específico do sensor
        :param point_name: 'acid', 'neutral' ou 'base'
        :param known_ph: Valor real do pH da solução
        """
        if point_name not in self.cal_points:
            raise ValueError("Ponto inválido. Use 'acid', 'neutral' ou 'base'")
            
        print(f"Calibrando ponto {point_name} (pH {known_ph})")
        print("Mergulhe o sensor na solução e aguarde 1 minuto...")
        
        voltage = self._read_stable_voltage()
        self.cal_points[point_name] = {
            'ph': known_ph,
            'voltage': voltage
        }
        
        print(f"Calibrado: pH {known_ph} = {voltage:.2f}V")

    def calibrate_auto(self):
        """Realiza calibração completa com os 3 pontos principais"""
        print("=== CALIBRAÇÃO DO SENSOR DE pH ===")
        self.calibrate_point('acid', 4.0)
        self.calibrate_point('neutral', 7.0)
        self.calibrate_point('base', 9.0)
        print("Calibração completa!")

    def _read_stable_voltage(self, samples=50, delay=100):
        """Lê a tensão estabilizada"""
        print("Aguardando estabilização...")
        readings = []
        for _ in range(samples):
            readings.append(self._read_voltage())
            time.sleep_ms(delay)
        return sum(readings) / len(readings)

    def _read_voltage(self):
        """Leitura simples da tensão"""
        return self.adc.read() * (3.3 / 4095)

    def set_temperature(self, temp):
        """Define a temperatura para compensação"""
        self.temperature = temp

    def read_ph(self, temp=None):
        """Lê o pH com compensação térmica"""
        if temp is not None:
            self.set_temperature(temp)
            
        try:
            voltage = self._read_voltage()
            
            # Interpolação entre pontos de calibração
            if voltage <= self.cal_points['acid']['voltage']:
                ph = self.cal_points['acid']['ph']
            elif voltage >= self.cal_points['base']['voltage']:
                ph = self.cal_points['base']['ph']
            elif voltage < self.cal_points['neutral']['voltage']:
                ph = self._interpolate(
                    voltage,
                    self.cal_points['acid']['voltage'],
                    self.cal_points['neutral']['voltage'],
                    self.cal_points['acid']['ph'],
                    self.cal_points['neutral']['ph']
                )
            else:
                ph = self._interpolate(
                    voltage,
                    self.cal_points['neutral']['voltage'],
                    self.cal_points['base']['voltage'],
                    self.cal_points['neutral']['ph'],
                    self.cal_points['base']['ph']
                )
            
            # Compensação térmica
            ph += (25.0 - self.temperature) * 0.003
            self.last_valid = round(ph, 2)
            return self.last_valid
            
        except Exception as e:
            print(f"Erro na leitura: {e}")
            return self.last_valid

    def _interpolate(self, x, x0, x1, y0, y1):
        """Interpolação linear entre dois pontos"""
        return y0 + (x - x0) * (y1 - y0) / (x1 - x0)