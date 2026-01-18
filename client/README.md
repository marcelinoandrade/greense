# GreenSe · Catálogo de Nós Cliente

Cada pasta `Nxx_*` representa uma entrega do Projeto GreenSe, com firmware pronto para campo, sensores definidos e assets de hardware. Use esta página como índice para localizar o nó adequado.

---

## Tabela de Nós

| Nó | Diretório | Stack | Hardware Principal | Comunicação |
|----|-----------|-------|-------------------|-------------|
| **N01** | `N01_Estufa_Germinar_C` | ESP-IDF 5.x (C) | ESP32 DevKit + AHT20/ENS160/DS18B20 | Wi-Fi AP/STA, MQTT/TLS, HTTP local |
| **N02** | `N02_Estufa_Maturar_C` | ESP-IDF 5.x (C) | ESP32 DevKit + boias + LED RGB | Wi-Fi STA, MQTT/TLS-WSS |
| **N03** | `N03_Estufa_P` | MicroPython | ESP32 + AHT20/ENS160/pH/EC | Wi-Fi STA, MQTT |
| **N04** | `N04_Estufa_Camera_C` | ESP-IDF (C) + esp32-camera | ESP32-CAM AI Thinker | Wi-Fi STA, HTTPS POST, SD card |
| **N05-C** | `N05_Estufa_Termica_C` | ESP-IDF (C) BSP/APP/GUI | ESP32-C3 + MLX90640 | Wi-Fi STA, HTTP JSON, NTP |
| **N05-P** | `N05_Estufa_Termica_P` | MicroPython | ESP32-C3 + MLX90640 | Wi-Fi STA, HTTP JSON |
| **N06-C** | `N06_Sensor_Campo_C` | ESP-IDF (C) + servidor HTTP | ESP32 Battery Kit + sensores solo | Wi-Fi AP `ESP32_TEMP`, HTTP |
| **N06-C2** | `N06_Sensor_Campo_2_C` | ESP-IDF (C) app/bsp/gui | ESP32 módulo único + solo + DHT11 + DS18B20 + BH1750 | Wi-Fi AP `greenSe_Campo`, HTTP, mDNS |
| **N06-P** | `N06_Sensor_Campo_P` | MicroPython + webserver | ESP32 Battery Kit | Wi-Fi AP `ESP32_TEMP`, HTTP |
| **N07** | `N07_Estufa_Artigo_C` | ESP-IDF (C) p/ ESP32-S3 com PSRAM | ESP32-S3 + câmera visual + MLX90640 + SD | Wi-Fi STA, HTTPS, NTP, SD |

---

## Notas por Nó

### N01 · Estufa Germinar
- **Objetivo**: Monitorar clima/solo na fase de germinação e acionar relés
- **Arquitetura**: Módulos `conexoes`, `sensores`, `atuadores` + certificados TLS
- **Protocolos**: Wi-Fi AP/STA, MQTT (`mqtt.greense.com.br:8883`), HTTP embarcado
- **Doc**: `N01_Estufa_Germinar_C/README.md`

### N02 · Estufa Maturar
- **Objetivo**: Acompanhar reservatórios, claridade e status visual (LED RGB) na fase de maturação
- **Destaques**: Boias, DHT22 externo, DS18B20 interno/externo, JSON periódico via MQTT seguro
- **Doc**: `N02_Estufa_Maturar_C/README.md`

### N03 · Estufa MicroPython
- **Objetivo**: Nó híbrido com sensores AHT20/ENS160, pH, EC e controle de bomba
- **Componentes**: Classes `Config`, `Conexao`, `SensorManager`, `ActuatorManager`
- **Doc**: Código comentado em `main.py` e arquivos auxiliares

### N04 · Estufa Câmera
- **Objetivo**: Capturar JPEGs (OV2640), salvar em SD e enviar via HTTPS
- **Extras**: Flash GPIO4, LED status GPIO33, certificado TLS embutido
- **Doc**: `N04_Estufa_Camera_C/README.md`

### N05-C · Estufa Térmica (ESP-IDF)
- **Objetivo**: Adquirir frames do MLX90640, converter para °C e enviar JSON agendado
- **Arquitetura**: BSP/APP/GUI, sincronização NTP, LED GPIO8, pipeline robusto de retries
- **Doc**: `N05_Estufa_Termica_C/README.md`

### N05-P · Estufa Térmica (MicroPython)
- **Objetivo**: Versão leve em MicroPython do mesmo termovisor
- **Configuração**: Ajustes diretamente no topo de `main.py` (SSID, URL, intervalo)
- **Doc**: `N05_Estufa_Termica_P/README.md`

### N06-C · Sensor de Campo ESP-IDF
- **Objetivo**: Nó autônomo com AP próprio, dashboard em HTTP e logger SPIFFS
- **Sensores**: Temperatura/umidade ar, DS18B20 (solo), sensor resistivo/capacitivo de umidade
- **Doc**: `N06_Sensor_Campo_C/README.md`

### N06-C2 · Sensor de Campo 2 (ESP-IDF)
- **Objetivo**: Módulo compacto de solo totalmente offline, AP `greenSe_Campo`, dashboard interativo e log em SPIFFS
- **Sensores**: Solo capacitivo (ADC32), DS18B20 (solo), DHT11 (ar), BH1750 (luminosidade) e LED de status (GPIO2)
- **Recursos**: mDNS `greense.local`, calibração de solo, tolerâncias configuráveis, gráficos em tempo real e download de CSV
- **Doc**: `N06_Sensor_Campo_2_C/README.md`

### N06-P · Sensor de Campo MicroPython
- **Objetivo**: Logger rápido com AP `ESP32_TEMP`, interface HTML e calibração via browser
- **Arquivos-chave**: `main.py`, `sensores/`, `webserver/http_server.py`

### N07 · Estufa Artigo
- **Objetivo**: Plataforma premium (ESP32-S3) com câmera visual + MLX90640, SD card e LED WS2812
- **Recursos**: Agendamentos independentes, SPIFFS→SD, scripts Python (`visualize_thermal.py`)
- **Doc**: `N07_Estufa_Artigo_C/README.md`

---

## Como Usar

1. Use a tabela para identificar o nó adequado
2. Leia a nota rápida para entender sensores, protocolos e particularidades
3. Entre no diretório correspondente e siga o README local para build/flash

---

## Licença

Este projeto faz parte do Projeto GreenSe da Universidade de Brasília.

**Autoria**: Prof. Marcelino Monteiro de Andrade  
**Instituição**: Faculdade de Ciências e Tecnologias em Engenharia (FCTE) – Universidade de Brasília  
**Website**: [https://greense.com.br](https://greense.com.br)
