# N01 · Estufa Germinar · Monitoramento Ambiental (ESP32-C6)

Firmware baseado em **ESP-IDF v5.2** para a Placa Mini Estufa, com **ESP32-C6** (USB-C, Wi-Fi 6) e os sensores DHT11, DS18B20, HW-072 e HD-38. Os dados seguem por **MQTT sobre WSS**.

---

## Descrição Geral

Nó IoT da fase de germinação. Lê temperatura e umidade do ar, temperatura e umidade do solo e luminosidade, e publica o conjunto no broker GreenSe. O LED RGB da própria placa indica se o Wi-Fi e o MQTT estão ativos.

### Recursos Principais

- Conexão Wi-Fi (modo STA) com reconexão automática
- Comunicação **MQTT sobre WSS** com certificado embutido
- Sensores:
  - **DHT11** – temperatura e umidade do ar
  - **DS18B20** – temperatura do solo (1-Wire, sonda à prova d’água)
  - **HW-072** – luminosidade (LDR com LM393, saída digital)
  - **HD-38** – umidade do solo (saída analógica)
- **LED RGB** da placa (GPIO 8) como indicador de status
- Arquitetura modular: conexões, sensores e atuadores
- Leitura e publicação a cada 5 segundos, depois que o MQTT conecta

---

## Hardware de Referência

### ESP32-C6

![ESP32-C6](imagens/esp32c6.jpg)

![ESP32-C6, pinos e rádios](imagens/esp32c6_pinos.jpg)

Placa usada neste nó:

- **Chip:** ESP32-C6FH8 (revisão v0.2), um núcleo RISC-V a 160 MHz e núcleo de baixo consumo
- **Rádio:** Wi-Fi 6 (2,4 GHz), Bluetooth 5 LE e 802.15.4
- **Flash:** 8 MB na placa testada; a imagem do firmware usa 4 MB
- **USB:** USB-C direto no chip (`/dev/ttyACM0`)
- **LED:** RGB endereçável no GPIO 8

O projeto da placa carrier está em [`PlacaMiniEstufa/`](PlacaMiniEstufa/).

### Sensores

| Função | Sensor | Pino | Observação |
|--------|--------|------|------------|
| Temperatura e umidade do ar | DHT11 | GPIO 18 | Pull-up para 3,3 V |
| Temperatura do solo | DS18B20 | GPIO 19 | 1-Wire, pull-up para 3,3 V |
| Luminosidade | HW-072 (DO) | GPIO 20 | Nível baixo = claro |
| Umidade do solo | HD-38 (AO) | GPIO 0 (A0) | ADC1, canal 0 |
| Status | LED RGB da placa | GPIO 8 | — |

Alimente o DHT11 e o DS18B20 em 3,3 V. O HW-072 e o HD-38 também devem ficar em **3,3 V**: a saída de um módulo de 5 V pode danificar o ESP32-C6.

Os GPIOs 12 e 13 são o USB e não entram na fiação. O GPIO 9 é pino de boot.

### Indicadores do LED

| Cor | Estado |
|-----|--------|
| Vermelho | Wi-Fi ou MQTT desconectado |
| Azul | Wi-Fi conectado e publicando |

---

## Estrutura de Diretórios

```
N01_Estufa_Germinar/
├── imagens/
│   ├── esp32c6.jpg             # Foto da placa
│   └── esp32c6_pinos.jpg       # Pinos, Wi-Fi 6 e BLE
├── PlacaMiniEstufa/            # Esquema e PCB (KiCad)
├── main/
│   ├── main.c                  # Inicialização e loop principal
│   ├── config.h                # MQTT, intervalo e pinos
│   ├── secrets.h               # Credenciais Wi-Fi (não versionado)
│   ├── conexoes/
│   │   └── conexoes.c/.h       # Wi-Fi e MQTT
│   ├── sensores/
│   │   ├── sensores.c/.h       # Leitura conjunta
│   │   ├── dht11.c/.h          # Ar
│   │   └── ds18b20.c/.h        # Solo
│   ├── atuadores/
│   │   └── atuadores.c/.h      # LED RGB
│   ├── certs/
│   │   └── greense_cert.pem    # Certificado do broker
│   └── CMakeLists.txt
├── sdkconfig                   # Alvo esp32c6, flash 4 MB
└── sdkconfig.defaults
```

---

## Comunicação MQTT

### Configuração

- **Broker:** `wss://mqtt.greense.com.br`
- **Biblioteca:** `esp-mqtt`
- **Certificado:** `main/certs/greense_cert.pem`
- **Cliente:** `ESP32_E3`
- **Tópico:** `estufa3/esp32`

### Dados publicados

```json
{
  "temp": 25.50,
  "umid": 60.00,
  "co2": 0.00,
  "luz": 1.00,
  "agua_min": 0,
  "agua_max": 0,
  "temp_reserv_int": 22.30,
  "ph": 0.00,
  "ec": 0.00,
  "temp_reserv_ext": 0.00,
  "umid_solo_raw": 2400,
  "umid_solo_pct": 70.00
}
```

| Campo | Origem |
|-------|--------|
| `temp`, `umid` | DHT11 (°C e %) |
| `luz` | HW-072 (1 = claro, 0 = escuro) |
| `temp_reserv_int` | DS18B20 (°C). `-127` indica sensor ausente |
| `umid_solo_raw`, `umid_solo_pct` | HD-38 |
| `co2`, `ph`, `ec`, `agua_min`, `agua_max`, `temp_reserv_ext` | Reservados, publicados em 0 |

Sem os sensores ligados, o log mostra `DHT11: sem resposta no GPIO 18`, solo em `-127` e a umidade do solo oscila porque o GPIO 0 está solto.

---

## Configuração

### `secrets.h`

Crie `main/secrets.h` (este arquivo não entra no Git):

```c
#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID "sua_rede_wifi"
#define WIFI_PASS "sua_senha_wifi"

#endif
```

### `config.h`

Pinos, tópico MQTT e identificador do cliente ficam em `main/config.h`.

---

## Como Executar

```bash
cd client/N01_Estufa_Germinar
. $HOME/esp/esp-idf/export.sh
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Para sair do monitor: `Ctrl+]`.

No log, a sequência esperada é NVS, Wi-Fi, MQTT e, em seguida, uma publicação a cada 5 segundos.

---

## Requisitos de Build

- **ESP-IDF 5.2** (testado com 5.2.2)
- **Python 3**
- Alvo `esp32c6`
- Componentes: `esp_wifi`, `esp_event`, `mqtt`, `nvs_flash`, `driver`, `esp_adc`, `esp_timer`, `espressif/led_strip`

---

## Testes

- Gravado e executado no **ESP32-C6FH8** ligado por USB-C
- Wi-Fi e MQTT confirmados, com publicação em `estufa3/esp32`
- Leitura dos sensores depende da fiação descrita acima

---

## Licença

Este projeto faz parte do Projeto GreenSe da Universidade de Brasília.

**Autoria**: Prof. Marcelino Monteiro de Andrade  
**Instituição**: Faculdade de Ciências e Tecnologias em Engenharia (FCTE) – Universidade de Brasília  
**Email**: [andrade@unb.br](mailto:andrade@unb.br)  
**Website**: [https://greense.com.br](https://greense.com.br)
