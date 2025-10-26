import sys
import time

sys.path.append('/')
sys.path.append('/libs')
sys.path.append('/sensores')
sys.path.append('/webserver')

from sensores.temp_sensor import TempSensorDS18B20
from data_logger import LoggerInterno
from webserver.http_server import TempWebServer

def main():
    sensor = TempSensorDS18B20(data_pin=4)
    logger = LoggerInterno(path="/log_temp.csv")

    web = TempWebServer(logger, ap_ssid="ESP32_TEMP", ap_password="12345678")
    print("Conecte no Wi-Fi ESP32_TEMP (senha 12345678) e abra http://{}/".format(web.ip))

    ultimo_log = time.time()

    while True:
        web.poll_once()

        agora = time.time()
        if agora - ultimo_log >= 10:
            temp_ar = sensor.ler_celsius()
            umid_ar = -1.0
            temp_solo = -1.0
            umid_solo = -1.0

            if temp_ar is not None:
                logger.append(agora, temp_ar, umid_ar, temp_solo, umid_solo)
                print("log:", agora, temp_ar)

            ultimo_log = agora

        time.sleep_ms(50)

main()
