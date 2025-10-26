from machine import Pin
import onewire, ds18x20, time

class TempSensorDS18B20:
    def __init__(self, data_pin=4):
        self.pin = Pin(data_pin)
        self.bus = onewire.OneWire(self.pin)
        self.ds = ds18x20.DS18X20(self.bus)
        self.roms = self.ds.scan()
        if not self.roms:
            # se nenhum sensor encontrado, continua mas marcará None
            self.roms = []
        # primeira leitura para estabilizar
        self._primeira_leitura()

    def _primeira_leitura(self):
        if not self.roms:
            return
        self.ds.convert_temp()
        time.sleep_ms(750)

    def ler_celsius(self):
        if not self.roms:
            return None
        self.ds.convert_temp()
        time.sleep_ms(750)
        rom = self.roms[0]
        temp_c = self.ds.read_temp(rom)
        return temp_c

