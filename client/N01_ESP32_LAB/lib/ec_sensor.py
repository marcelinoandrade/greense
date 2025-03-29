from machine import ADC, Pin
import time
import json
import os

class ECSensor:
    def __init__(self, pin=34, temperature_sensor=None):
        """
        Inicializa o sensor de EC
        
        Args:
            pin (int): Pino ADC conectado ao sensor
            temperature_sensor (object): Sensor de temperatura para compensação (opcional)
        """
        self.adc = ADC(Pin(pin))
        self.adc.atten(ADC.ATTN_11DB)  # Full range 0-3.3V
        self.temp_sensor = temperature_sensor
        
        # Parâmetros de calibração padrão
        self.calibration = {
            'adc_low': 1500,
            'adc_high': 3000,
            'ec_low': 1413.0,   # µS/cm
            'ec_high': 12880.0,  # µS/cm
            'k_value': 1.0,      # Fator de correção adicional
            'temp_comp': 0.019   # Coeficiente de compensação de temperatura (2%/°C)
        }
        
        # Carrega calibração salva, se existir
        self._load_calibration()

    def _load_calibration(self):
        """Tenta carregar calibração do arquivo"""
        try:
            with open('ec_calibration.json', 'r') as f:
                saved_cal = json.load(f)
                self.calibration.update(saved_cal)
            print("Calibração EC carregada com sucesso")
        except:
            print("Usando calibração padrão EC")

    def _save_calibration(self):
        """Salva calibração atual em arquivo"""
        with open('ec_calibration.json', 'w') as f:
            json.dump(self.calibration, f)

    def read_raw(self, samples=10, delay_ms=50):
        """Lê valor bruto do ADC"""
        readings = []
        for _ in range(samples):
            readings.append(self.adc.read())
            time.sleep_ms(delay_ms)
        return sum(readings) / len(readings)

    def calculate_ec(self, raw_value, temperature=25.0):
        """
        Calcula EC com base no valor ADC e temperatura
        
        Args:
            raw_value (float): Valor lido do ADC
            temperature (float): Temperatura em °C (para compensação)
        
        Returns:
            float: Valor de EC em µS/cm
        """
        cal = self.calibration
        
        # Interpolação linear entre pontos de calibração
        if raw_value <= cal['adc_low']:
            ec = cal['ec_low'] * (raw_value / cal['adc_low'])
        elif raw_value >= cal['adc_high']:
            ec = cal['ec_high'] * (raw_value / cal['adc_high'])
        else:
            ec = ((raw_value - cal['adc_low']) * 
                 (cal['ec_high'] - cal['ec_low']) / 
                 (cal['adc_high'] - cal['adc_low']) + 
                 cal['ec_low'])
        
        # Aplica fator de correção
        ec *= cal['k_value']
        
        # Compensação de temperatura (se disponível)
        if self.temp_sensor or temperature:
            temp = temperature if temperature else self.temp_sensor.temperature
            ec /= (1 + cal['temp_comp'] * (temp - 25.0))
        
        return max(0, ec)  # EC não pode ser negativo

    def calibrate_low(self, reference_ec):
        """
        Calibração com solução de baixo EC
        
        Args:
            reference_ec (float): Valor de EC conhecido da solução (µS/cm)
        """
        input(f"Coloque o sensor na solução de {reference_ec} µS/cm e pressione Enter")
        raw = self.read_raw()
        self.calibration['adc_low'] = raw
        self.calibration['ec_low'] = reference_ec
        self._save_calibration()
        print(f"Calibração baixa: ADC={raw} para {reference_ec} µS/cm")

    def calibrate_high(self, reference_ec):
        """
        Calibração com solução de alto EC
        
        Args:
            reference_ec (float): Valor de EC conhecido da solução (µS/cm)
        """
        input(f"Coloque o sensor na solução de {reference_ec} µS/cm e pressione Enter")
        raw = self.read_raw()
        self.calibration['adc_high'] = raw
        self.calibration['ec_high'] = reference_ec
        self._save_calibration()
        print(f"Calibração alta: ADC={raw} para {reference_ec} µS/cm")

    def set_k_value(self, k_value):
        """Ajusta fator de correção adicional"""
        self.calibration['k_value'] = k_value
        self._save_calibration()

    def read_ec(self, temperature=None):
        """
        Lê EC com compensação de temperatura
        
        Args:
            temperature (float): Temperatura em °C (opcional, usa sensor se disponível)
        
        Returns:
            dict: {'raw': valor_adc, 'ec': valor_ec, 'temp': temperatura_usada}
        """
        raw = self.read_raw()
        temp = temperature if temperature is not None else (
            self.temp_sensor.temperature if self.temp_sensor else 25.0)
        ec = self.calculate_ec(raw, temp)
        
        return {
            'raw': raw,
            'ec': ec,
            'temp': temp
        }

    def interactive_calibration(self):
        """Modo interativo para calibração"""
        print("\n=== CALIBRAÇÃO DO SENSOR EC ===")
        print("1. Calibrar solução de baixo EC")
        print("2. Calibrar solução de alto EC")
        print("3. Ajustar fator K")
        print("4. Ver calibração atual")
        print("5. Sair")
        
        choice = input("Escolha: ")
        
        if choice == '1':
            ref_ec = float(input("Digite o valor EC conhecido (µS/cm): "))
            self.calibrate_low(ref_ec)
        elif choice == '2':
            ref_ec = float(input("Digite o valor EC conhecido (µS/cm): "))
            self.calibrate_high(ref_ec)
        elif choice == '3':
            k_val = float(input("Digite o fator K (atual={:.2f}): ".format(
                self.calibration['k_value'])))
            self.set_k_value(k_val)
        elif choice == '4':
            print("\nCalibração atual:")
            print(f"- Baixa: ADC {self.calibration['adc_low']} => {self.calibration['ec_low']} µS/cm")
            print(f"- Alta: ADC {self.calibration['adc_high']} => {self.calibration['ec_high']} µS/cm")
            print(f"- Fator K: {self.calibration['k_value']:.2f}")
            print(f"- Comp. temp: {self.calibration['temp_comp']*100:.1f}%/°C")
        elif choice == '5':
            return
        
        self.interactive_calibration()