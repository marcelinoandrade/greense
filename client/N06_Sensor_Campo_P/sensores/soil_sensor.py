import ujson as json
from machine import ADC, Pin

class SoilMoistureSensor:
    """
    Sensor de umidade de solo via saída analógica AO.
    - Lê ADC bruto (0..4095 no ESP32).
    - Usa calibração (seco_raw, molhado_raw) salva em /soil_calib.json.
    - Converte para %.
    """

    CALIB_PATH = "/soil_calib.json"

    def __init__(self, adc_pin=34, atenuacao=ADC.ATTN_11DB):
        self.adc = ADC(Pin(adc_pin))
        self.adc.atten(atenuacao)

        # valores padrão caso não exista calibração salva
        self.seco_raw = 3500.0
        self.molhado_raw = 1500.0

        # tenta carregar calibração do arquivo
        self._carregar_calibracao()

    def _carregar_calibracao(self):
        try:
            with open(self.CALIB_PATH, "r") as f:
                data = json.load(f)
            self.seco_raw = float(data.get("seco_raw", self.seco_raw))
            self.molhado_raw = float(data.get("molhado_raw", self.molhado_raw))
        except OSError:
            # arquivo não existe ainda
            pass
        except Exception:
            # arquivo corrompido etc
            pass

    def salvar_calibracao(self, seco, molhado):
        """
        Atualiza calibração em memória e persiste em flash.
        seco    = leitura bruta com sonda SECA
        molhado = leitura bruta com sonda MUITO ÚMIDA
        """
        self.seco_raw = float(seco)
        self.molhado_raw = float(molhado)
        try:
            with open(self.CALIB_PATH, "w") as f:
                json.dump({
                    "seco_raw": self.seco_raw,
                    "molhado_raw": self.molhado_raw
                }, f)
            return True
        except Exception:
            return False

    def leitura_bruta(self):
        return self.adc.read()

    def umidade_percentual(self):
        val = self.leitura_bruta()
        seco = self.seco_raw
        molh = self.molhado_raw

        # prevenção divisão por zero
        if seco == molh:
            return 0.0

        # mapeamento linear:
        # solo seco  -> leitura alta (seco_raw)
        # solo úmido -> leitura baixa (molhado_raw)
        pct = (seco - val) * 100.0 / (seco - molh)

        # limitar 0..100
        if pct < 0:
            pct = 0.0
        if pct > 100:
            pct = 100.0

        return pct
