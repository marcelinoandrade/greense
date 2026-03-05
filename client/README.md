# GreenSe · Catálogo de Nós Cliente

A pasta `client` contém as entregas do Projeto GreenSe de firmware para microcontroladores ESP32, prontos para uso em campo com sensores definidos e assets de hardware. Use esta página como índice para localizar o nó adequado.

---

## Tabela de Nós

| Nó | Diretório | Stack | Hardware Principal | Comunicação |
|----|-----------|-------|-------------------|-------------|
| **N01** | `N01_Estufa_Germinar` | ESP-IDF 5.x (C) | ESP32 + AHT20/ENS160/DS18B20 | Wi-Fi, MQTT/TLS, HTTP local |
| **N02** | `N02_Estufa_Maturar` | ESP-IDF 5.x (C) | ESP32 + Sensores + Boias + LED RGB | Wi-Fi, MQTT/TLS-WSS |
| **N03** | `N03_Estufa_Camera` | ESP-IDF 5.x (C) | ESP32-CAM AI Thinker (OV2640) | Wi-Fi, HTTPS POST, SD Card |
| **N04** | `N04_Estufa_Termica` | ESP-IDF 5.x (C) | ESP32-S3 WROOM (N16R8) + MLX90640 | Wi-Fi, HTTPS POST, SD Card, NTP |

---

## Notas por Nó

### N01 · Estufa Germinar
- **Objetivo**: Monitorar clima e solo na fase de germinação e acionar relés.
- **Sensores**: AHT20 (ar), ENS160 (qualidade ar), DS18B20 (solo).
- **Protocolos**: Wi-Fi AP/STA, MQTT seguro (`mqtt.greense.com.br:8883`), HTTP embarcado.
- **Doc**: [`N01_Estufa_Germinar/README.md`](N01_Estufa_Germinar/README.md)

### N02 · Estufa Maturar
- **Objetivo**: Acompanhar reservatórios, claridade e status visual na fase de maturação.
- **Destaques**: Boias de nível, DHT22 externo, DS18B20, LED RGB (GPIO 16), MQTT via WSS.
- **Doc**: [`N02_Estufa_Maturar/README.md`](N02_Estufa_Maturar/README.md)

### N03 · Estufa Câmera (Visual)
- **Objetivo**: Capturar e enviar imagens JPEG periodicamente via HTTPS para um servidor remoto.
- **Recursos**: Salva backup no SD, Flash LED (GPIO4), LED status (GPIO33), ESP32-CAM padrão.
- **Doc**: [`N03_Estufa_Camera/README.md`](N03_Estufa_Camera/README.md)

### N04 · Estufa Térmica (Avançado)
- **Objetivo**: Plataforma premium para aquisição periódica de imagens visuais (OV2640) e térmicas (MLX90640).
- **Características**: Placa ESP32-S3 com PSRAM (N16R8), dois agendamentos independentes baseados em NTP, acúmulo temporário em SPIFFS e migração p/ SD, views com script Python (`visualize_thermal.py`). 
- **Doc**: [`N04_Estufa_Termica/README.md`](N04_Estufa_Termica/README.md)

---

## Como Usar

1. Use a tabela para identificar o nó adequado.
2. Leia a nota rápida para entender sensores, protocolos e particularidades.
3. Entre no diretório correspondente e siga o `README.md` local para detalhamento do hardware, build e flash usando o ESP-IDF.

---

## Licença

Este projeto faz parte do Projeto GreenSe da Universidade de Brasília.

**Autoria**: Prof. Marcelino Monteiro de Andrade  
**Instituição**: Faculdade de Ciências e Tecnologias em Engenharia (FCTE) – Universidade de Brasília  
**Website**: [https://greense.com.br](https://greense.com.br)
