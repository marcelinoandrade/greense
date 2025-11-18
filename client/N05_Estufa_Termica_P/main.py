import network
import time
import urequests
import machine
import uselect
import struct
import gc

# =========================
# CONFIG
# =========================
WIFI_SSID = "XXX"
WIFI_PASS = "XXX"

URL_POST = "XXX"

LINHAS = 24
COLS = 32
TOTAL = LINHAS * COLS  # 768

INTERVALO_ENVIO_S = 90  # segundos entre uploads

# UART do sensor térmico
uart_sensor = machine.UART(1, baudrate=115200, tx=5, rx=4)

poll = uselect.poll()
poll.register(uart_sensor, uselect.POLLIN)

# LED indicador
led = machine.Pin(8, machine.Pin.OUT)
led.off()

# buffer UART global persistente
uart_buf = bytearray()


# =========================
# WIFI COM SINALIZAÇÃO LED
# =========================
def wifi_conectar():
    """Conecta ao WiFi com sinalização visual no LED"""
    wlan = network.WLAN(network.STA_IF)
    if not wlan.active():
        wlan.active(True)

    if not wlan.isconnected():
        print("WIFI: conectando em", WIFI_SSID)
        
        # 🔴 LED PISCA RÁPIDO durante tentativa de conexão
        print("🔴 LED: Piscando rápido - Tentando conectar WiFi...")
        wlan.connect(WIFI_SSID, WIFI_PASS)

        t0 = time.ticks_ms()
        while not wlan.isconnected():
            # Pisca LED rápido (200ms ligado, 200ms desligado)
            led.on()
            time.sleep_ms(200)
            led.off()
            time.sleep_ms(200)
            
            if time.ticks_diff(time.ticks_ms(), t0) > 15000:
                print("WIFI: timeout na conexão")
                break
            time.sleep_ms(200)

    if wlan.isconnected():
        # 🟢 LED PISCA LENTO quando conectado
        print("🟢 LED: Piscando lento - WiFi conectado!")
        for _ in range(3):  # 3 piscadas de confirmação
            led.on()
            time.sleep_ms(500)
            led.off()
            time.sleep_ms(200)
        
        print("WIFI: OK ip", wlan.ifconfig()[0])
        return wlan
    else:
        print("WIFI: falhou")
        return wlan


# =========================
# helpers de serialização
# =========================
def extract_temperaturas(payload):
    """
    payload = frame_len bytes (sem 0x5A5A, sem campo length).
    Pode ter dois formatos:
      1538 bytes -> 1536 pixels + 2 bytes CRC
      1543 bytes -> 5 bytes header interno + 1536 pixels + 2 bytes CRC
    Retorna lista[768] float °C ou None.
    """

    L = len(payload)

    if L == 1538:
        px = payload[:-2]           # remove CRC
    elif L == 1543:
        px = payload[5:-2]          # remove header interno e CRC
    else:
        return None

    if len(px) != 1536:
        return None

    try:
        vals = struct.unpack('<768h', px)  # 768 valores int16 little-endian signed
    except Exception:
        return None

    temperaturas = [v / 100.0 for v in vals]  # escala para °C
    return temperaturas


def montar_json(temperaturas, timestamp):
    # temperaturas: lista[768] de floats °C
    partes = []
    addp = partes.append
    for t in temperaturas:
        addp("{:.2f}".format(t))
    lista_str = ",".join(partes)

    body = (
        '{"temperaturas":[' +
        lista_str +
        '],"timestamp":' +
        str(timestamp) +
        "}"
    )
    return body


# =========================
# captura UART robusta
# =========================
def ler_uart_into_buffer(max_buf=8192):
    """Lê bytes disponíveis da UART e coloca em uart_buf global."""
    global uart_buf
    events = poll.poll(5)
    for (stream, evt) in events:
        if evt & uselect.POLLIN:
            chunk = stream.read(256)
            if chunk:
                uart_buf.extend(chunk)
                # mantém buffer num tamanho controlado
                if len(uart_buf) > max_buf:
                    uart_buf = uart_buf[-max_buf:]


def tentar_frame_do_buffer():
    """
    Examina uart_buf global e tenta extrair um frame completo.
    Se conseguir:
      - retorna (temperaturas, bytes_consumidos)
    Se não:
      - retorna (None, 0)
    """
    global uart_buf

    buf_len = len(uart_buf)
    if buf_len < 4:
        return None, 0

    # procurar header 0x5A 0x5A
    idx = -1
    for i in range(buf_len - 1):
        if uart_buf[i] == 0x5A and uart_buf[i+1] == 0x5A:
            idx = i
            break

    if idx < 0:
        # não achou header. manter só último byte (pode ser 0x5A parcial)
        if buf_len > 1:
            uart_buf = uart_buf[-1:]
        return None, 0

    # se header não está no início, descarta lixo inicial
    if idx > 0:
        uart_buf = uart_buf[idx:]
        buf_len = len(uart_buf)
        if buf_len < 4:
            return None, 0
        idx = 0

    # agora uart_buf[0:2] deve ser 0x5A 0x5A
    frame_len = uart_buf[2] | (uart_buf[3] << 8)
    total_frame_size = 4 + frame_len  # header(2)+len(2)+payload

    if buf_len < total_frame_size:
        # ainda não chegou tudo
        return None, 0

    # payload bruto exclui cabeçalho (4 bytes)
    payload = bytes(uart_buf[4:total_frame_size])

    temperaturas = extract_temperaturas(payload)
    if temperaturas is None:
        # frame ruim. descarta esse frame e segue leitura
        return None, total_frame_size

    # checagem física simples. evita mandar lixo absurdo
    tmin = min(temperaturas)
    tmax = max(temperaturas)
    if tmin < -40 or tmax > 200:
        # claramente quebrado
        return None, total_frame_size

    return temperaturas, total_frame_size


def capturar_frame_termico(timeout_ms=5000):
    """
    Tenta obter um frame consistente no período timeout_ms.
    Retorna lista[768] floats °C ou None se não conseguiu.
    Mantém o buffer UART entre tentativas.
    """
    t0 = time.ticks_ms()

    while time.ticks_diff(time.ticks_ms(), t0) < timeout_ms:
        # lê mais bytes da UART
        ler_uart_into_buffer()

        # tenta extrair um frame completo
        temps, consumidos = tentar_frame_do_buffer()

        if consumidos > 0:
            # consumimos esses bytes do buffer global
            global uart_buf
            uart_buf = uart_buf[consumidos:]

        if temps is not None:
            return temps

        # sem frame válido ainda, espera ligeiro e tenta de novo
        time.sleep_ms(10)

    return None


# =========================
# HTTP POST COM SINALIZAÇÃO
# =========================
def enviar_http(json_body):
    headers = {
        "Content-Type": "application/json",
        "Connection": "close"
    }

    try:
        print("HTTP: enviando {} bytes".format(len(json_body)))
        r = urequests.post(URL_POST, data=json_body, headers=headers)
        status = r.status_code
        print("HTTP: status =", status)
        try:
            _ = r.text
        except Exception:
            pass
        r.close()
        
        # ✅ UMA PISCADA para confirmar POST 200
        if status == 200:
            print("✅ POST 200 - Uma piscada de confirmação")
            led.on()
            time.sleep_ms(300)  # Piscada rápida
            led.off()
            
        return status == 200
    except Exception as e:
        print("HTTP: erro POST:", e)
        return False


# =========================
# LOOP PRINCIPAL
# =========================
def loop_principal():
    wlan = wifi_conectar()
    internet_ok = wlan.isconnected()

    while True:
        # 🔴 VERIFICA SE PERDEU INTERNET
        if not wlan.isconnected():
            print("🌐 PERDEU INTERNET - Reconectando...")
            internet_ok = False
            wlan = wifi_conectar()
            
            # Se reconectou, faz piscada de confirmação
            if wlan.isconnected() and not internet_ok:
                print("🌐 INTERNET RECUPERADA")
                internet_ok = True
                # 🟢 Piscada de confirmação de internet
                led.on()
                time.sleep_ms(500)
                led.off()
                time.sleep_ms(200)
                led.on()
                time.sleep_ms(500)
                led.off()

        print("Aguardando frame térmico completo da UART...")
        temperaturas = capturar_frame_termico(timeout_ms=5000)

        if temperaturas is None:
            print("SENSOR: nenhum frame válido no intervalo")
            time.sleep(2)
            continue

        tmin = min(temperaturas)
        tmax = max(temperaturas)
        print("Frame OK. min={:.2f}C max={:.2f}C".format(tmin, tmax))

        ts = int(time.time())
        body = montar_json(temperaturas, ts)

        # 🔵 Envia dados (LED fica desligado durante envio)
        ok = enviar_http(body)
        # ✅ A piscada de confirmação já acontece dentro de enviar_http() se status=200

        if ok:
            print("OK envio ts", ts)
        else:
            print("FALHA envio ts", ts)

        del temperaturas
        del body
        gc.collect()

        # Aguarda intervalo com LED desligado
        for i in range(INTERVALO_ENVIO_S):
            time.sleep(1)

# =========================
# START
# =========================
loop_principal()