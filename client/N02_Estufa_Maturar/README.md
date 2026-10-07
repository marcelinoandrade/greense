# N02 · Estufa Maturar

O nó de maturação publica o clima, o solo, a luz e o nível das boias a cada 5 segundos em `estufa/maturar`. O módulo é o ESP32-S3 Zero.

O firmware de leitura, Wi-Fi e MQTT fica em [`../greense/`](../greense/), compartilhado com a N01. Esta pasta guarda a identidade do nó: tópico, calibração do HD-38 e boias ligadas, em [`main/config.h`](main/config.h). O `main.c` só chama a biblioteca. Os pinos saem de [`../greense/include/greense.h`](../greense/include/greense.h) conforme o alvo desta pasta, que já é `esp32s3`.

---

## Placa

![ESP32-S3 Zero](imagens/esp32s3.jpg)

- **Módulo:** Waveshare ESP32-S3 Zero, USB-C, Wi-Fi e Bluetooth 5 LE
- **Chip:** ESP32-S3FH4R2, flash de 4 MB e PSRAM de 2 MB
- **LED:** WS2812 no GPIO21, ordem de cor RGB
- **Porta serial:** `/dev/ttyACM0`

O conector traz 5V, GND, 3V3 e os GPIOs usados pelos sensores. O LED já vem soldado na placa.

| Função | Sensor | GPIO | Ligação |
|--------|--------|------|---------|
| Temperatura e umidade do ar | DHT11 | GPIO4 | Dados com pull-up de 4,7 kΩ para 3,3 V |
| Temperatura do solo | DS18B20 | GPIO5 | Dados com pull-up para 3,3 V |
| Luminosidade | HW-072, saída DO | GPIO6 | Nível baixo = claro |
| Umidade do solo | HD-38, saída AO | GPIO1 | Analógico, ADC1 canal 0 |
| Boia baixa | contato para GND | GPIO7 | Aberto = 1, fechado = 0. Campo `agua_min` |
| Boia alta | contato para GND | GPIO8 | Aberto = 1, fechado = 0. Campo `agua_max` |
| Status | LED da placa | GPIO21 | Azul publicando, vermelho sem Wi-Fi ou MQTT |

Alimente os sensores em **3,3 V**. O pull-up do DHT11 e o do DS18B20 ficam entre o fio de dados e o 3,3 V.

O GPIO0 é o botão BOOT. O GPIO19 e o GPIO20 são o USB. O GPIO43 e o GPIO44 são o UART de log. Nenhum desses entra na fiação dos sensores.

| Cor do LED | Estado |
|------------|--------|
| Azul | Wi-Fi conectado e publicando |
| Vermelho | Wi-Fi ou MQTT desconectado |

---

## Como gravar

Crie `main/secrets.h`. Esse arquivo não entra no Git.

```c
#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID "sua_rede_wifi"
#define WIFI_PASS "sua_senha_wifi"

#endif
```

```bash
cd client/N02_Estufa_Maturar
. $HOME/esp/esp-idf/export.sh
idf.py -p /dev/ttyACM0 build flash monitor
```

Para sair do monitor: `Ctrl+]`.

O alvo `esp32s3` já está no `sdkconfig`. `idf.py set-target esp32s3` só entra se essa pasta for recriada. Se a gravação não começar, segure BOOT, aperte RESET e solte BOOT.

No log, a sequência esperada é NVS, Wi-Fi, MQTT e uma publicação a cada 5 segundos.

---

## MQTT

- **Broker:** `wss://mqtt.greense.com.br`
- **Cliente:** `Estufa_Maturar`
- **Tópico:** `estufa/maturar`
- **Certificado:** [`../greense/certs/greense_cert.pem`](../greense/certs/greense_cert.pem)

```json
{
  "temp": 28.50,
  "umid": 53.10,
  "co2": 0.00,
  "luz": 1.00,
  "agua_min": 1,
  "agua_max": 0,
  "temp_reserv_int": 26.44,
  "ph": 0.00,
  "ec": 0.00,
  "temp_reserv_ext": 0.00,
  "umid_solo_raw": 3267,
  "umid_solo_pct": 26.65
}
```

| Campo | Origem |
|-------|--------|
| `temp`, `umid` | DHT11, em °C e % |
| `luz` | HW-072. 1 = claro, 0 = escuro |
| `agua_min`, `agua_max` | Boias. 1 = contato aberto, 0 = fechado em GND |
| `temp_reserv_int` | DS18B20, em °C. `-127` significa sensor ausente |
| `umid_solo_raw`, `umid_solo_pct` | HD-38. A calibração fica em `main/config.h` |
| `co2`, `ph`, `ec`, `temp_reserv_ext` | Reservados, publicados em 0 |

---

## O que há no repositório

```
N02_Estufa_Maturar/
├── imagens/
│   └── esp32s3.jpg             # ESP32-S3 Zero
├── main/
│   ├── main.c                  # Chama a biblioteca greense
│   ├── config.h                # Tópico, calibração, boias ligadas
│   └── secrets.h
├── sdkconfig                   # Alvo esp32s3
└── sdkconfig.defaults
```

O código comum está em [`../greense/`](../greense/). Para compilar: ESP-IDF 5.2, Python 3, alvo `esp32s3`.

---

## Licença

Este projeto faz parte do Projeto GreenSe da Universidade de Brasília.

**Autoria**: Prof. Marcelino Monteiro de Andrade  
**Instituição**: Faculdade de Ciências e Tecnologias em Engenharia (FCTE) – Universidade de Brasília  
**Email**: [andrade@unb.br](mailto:andrade@unb.br)  
**Website**: [https://greense.com.br](https://greense.com.br)
