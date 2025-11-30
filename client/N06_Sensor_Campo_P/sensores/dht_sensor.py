from machine import Pin
import dht
import time


class DHT11Sensor:
    """
    Leitor simples para DHT11 com limitação mínima entre medições.
    Armazena última leitura válida para reutilizar em caso de falha momentânea.
    """

    _MIN_INTERVAL_MS = 2000  # datasheet: mínimo 1 s; mantemos folga de 2 s

    def __init__(self, data_pin=19):
        self._sensor = dht.DHT11(Pin(data_pin))
        self._last_temp = None
        self._last_umid = None
        # força primeira medição permitida imediatamente
        self._last_measure_ms = time.ticks_ms() - self._MIN_INTERVAL_MS

    def _maybe_measure(self):
        now = time.ticks_ms()
        if time.ticks_diff(now, self._last_measure_ms) < self._MIN_INTERVAL_MS:
            return
        self._sensor.measure()
        self._last_temp = self._sensor.temperature()
        self._last_umid = self._sensor.humidity()
        self._last_measure_ms = now

    def ler(self):
        """
        Retorna (temp_c, umid_pct). Pode devolver última leitura válida.
        """
        try:
            self._maybe_measure()
        except OSError:
            # retorna último valor conhecido se a leitura atual falhar
            pass
        return self._last_temp, self._last_umid

    def ler_celsius(self):
        temp, _ = self.ler()
        return temp

    def ler_umidade(self):
        _, umid = self.ler()
        return umid

