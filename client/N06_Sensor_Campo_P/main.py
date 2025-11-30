import sys
import time
from machine import Pin

sys.path.append('/')
sys.path.append('/libs')
sys.path.append('/sensores')
sys.path.append('/webserver')

from sensores.temp_sensor import TempSensorDS18B20
from sensores.dht_sensor import DHT11Sensor
from sensores.soil_sensor import SoilMoistureSensor
from data_logger import LoggerInterno
from webserver.http_server import TempWebServer

# LED interno na ESP32 (GPIO2 em DevKit típico)
LED = Pin(2, Pin.OUT)

def blink_led():
    # pisca curto para indicar gravação de dado
    LED.on()
    time.sleep_ms(100)
    LED.off()

def main():
    temp_sensor = TempSensorDS18B20(data_pin=4)
    dht_sensor = DHT11Sensor(data_pin=19)
    solo_sensor = SoilMoistureSensor(adc_pin=34)
    logger = LoggerInterno(path="/log_temp.csv")

    # Sobe Wi-Fi AP e servidor HTTP
    web = TempWebServer(
        logger,
        solo_sensor,
        ap_ssid="ESP32_TEMP",
        ap_password="12345678"
    )

    # Se chegamos até aqui sem exceção, o AP está ativo
    # LED ligado fixo para indicar "sistema online / AP ativo"
    LED.on()

    print("Conecte no Wi-Fi ESP32_TEMP (senha 12345678) e abra http://{}/".format(web.ip))

    ultimo_log = time.time()

    while True:
        # atende requisição HTTP se houver
        web.poll_once()

        agora = time.time()
        sample_interval = web.get_sample_interval()
        if agora - ultimo_log >= sample_interval:
            # lê sensores
            temp_solo = temp_sensor.ler_celsius()
            temp_ar, umid_ar = dht_sensor.ler()
            umid_solo = solo_sensor.umidade_percentual()

            if temp_ar is not None and umid_ar is not None:
                temp_solo_val = temp_solo if temp_solo is not None else -1.0

                # grava no CSV interno (flash)
                logger.append(
                    temp_ar=temp_ar,
                    umid_ar=umid_ar,
                    temp_solo=temp_solo_val,
                    umid_solo=umid_solo
                )

                # pisca LED para indicar "amostra registrada"
                blink_led()

                # log serial para debug
                temp_solo_text = "{:.1f}".format(temp_solo) if temp_solo is not None else "n/a"
                print(
                    "log:", logger.idx,
                    "temp_ar_C=", "{:.1f}".format(temp_ar),
                    "umid_ar%=", "{:.1f}".format(umid_ar),
                    "temp_solo_C=", temp_solo_text,
                    "umid_solo%=", "{:.1f}".format(umid_solo)
                )

            ultimo_log = agora

        time.sleep_ms(50)

main()
