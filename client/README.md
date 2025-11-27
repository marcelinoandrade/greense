# 🌿 GreenSe | Catálogo de Clientes e Aplicações

Este repositório reúne os firmwares e scripts entregues a diferentes clientes do Projeto GreenSe. Cada pasta `Nxx_*` representa um nó IoT completo – com hardware específico, pilha de software definida e assets (imagens das placas, certificados, scripts) já preparados para campo.

---

## 📋 Visão Geral dos Clientes

| Cliente / Nó | Diretório | Tecnologia Principal | Comunicação | Aplicação |
|--------------|-----------|----------------------|-------------|-----------|
| N01 · Estufa Germinar | `N01_Estufa_Germinar_C` | ESP-IDF (C) + componentes modulares | Wi-Fi (AP/STA), MQTT/TLS e HTTP local | Monitoramento e atuação na fase de germinação com sensores de clima e solo |
| N02 · Estufa Maturar | `N02_Estufa_Maturar_C` | ESP-IDF (C) | Wi-Fi STA, MQTT/TLS/WSS | Supervisão da fase de maturação com boias de nível, fotoperíodo e LED RGB de status |
| N03 · Estufa (MicroPython) | `N03_Estufa_P` | MicroPython (ESP32) | Wi-Fi STA + MQTT | Nó híbrido com sensores de pH/EC, boias e atuadores (bomba + LED) |
| N04 · Estufa Câmera | `N04_Estufa_Camera_C` | ESP-IDF (C) + esp32-camera | Wi-Fi STA, HTTPS POST, SD Card | Captura periódica de imagens (ESP32-CAM) com backup em SD |
| N05 · Estufa Térmica (C) | `N05_Estufa_Termica_C` | ESP-IDF (C) | Wi-Fi STA, HTTP JSON, NTP | Termovisor MLX90640 em ESP32-C3 com agendador e LED de estados |
| N05 · Estufa Térmica (MicroPython) | `N05_Estufa_Termica_P` | MicroPython | Wi-Fi STA, HTTP JSON | Versão ágil em MicroPython para o mesmo kit térmico ESP32-C3 |
| N06 · Sensor de Campo (C) | `N06_Sensor_Campo_C` | ESP-IDF (C) | Wi-Fi AP, HTTP Server, SPIFFS | Data logger de solo/ambiente com dashboard embarcado |
| N06 · Sensor de Campo (MicroPython) | `N06_Sensor_Campo_P` | MicroPython | Wi-Fi AP, HTTP Server | Variante MicroPython com logger CSV e calibração via browser |
| N07 · Estufa Artigo (Visual + Térmica) | `N07_Estufa_Artigo_C` | ESP-IDF (C) + BSP/APP/GUI | Wi-Fi STA, HTTPS, NTP, SD, LED WS2812 | Plataforma premium no ESP32-S3 com câmera visual + MLX90640 e pipeline de arquivamento |

---

## 🖼️ Galeria das Placas (imagens originais dos diretórios)

| Cliente | Hardware |
|---------|----------|
| N01 · Estufa Germinar | ![N01 ESP32](./N01_Estufa_Germinar_C/esp32_Freenove.png) |
| N02 · Estufa Maturar | ![N02 ESP32](./N02_Estufa_Maturar_C/esp32_Freenove.png) |
| N04 · Estufa Câmera | ![ESP32-CAM](./N04_Estufa_Camera_C/esp32_cam.png) |
| N05 · Estufa Térmica (C) | ![MLX90640](./N05_Estufa_Termica_C/camera_termica.png) ![ESP32-C3](./N05_Estufa_Termica_C/esp32_c3.png) ![Mapa Térmico](./N05_Estufa_Termica_C/imagensTermicas.png) |
| N05 · Estufa Térmica (MicroPython) | ![MLX90640](./N05_Estufa_Termica_P/camera_termica.png) ![ESP32-C3 Mini](./N05_Estufa_Termica_P/esp32_c3.png) |
| N06 · Sensor de Campo (C) | ![ESP32 Battery](./N06_Sensor_Campo_C/esp32_battery.png) |
| N06 · Sensor de Campo (MicroPython) | ![ESP32 Battery](./N06_Sensor_Campo_P/esp32_battery.png) |
| N07 · Estufa Artigo | ![ESP32-S3 + MLX90640](./N07_Estufa_Artigo_C/camera_termica.png) ![Placa ESP32-S3](./N07_Estufa_Artigo_C/esp32s3.jpg) |

> ℹ️ O nó `N03_Estufa_P` não possui imagem de placa depositada no diretório. Use este espaço para anexar fotos futuras, se necessário.

---

## 🔍 Detalhes por Cliente

### N01 · Estufa Germinar (`N01_Estufa_Germinar_C`)
- **Stack:** ESP-IDF 5.x modular (conexões, sensores, atuadores).
- **Hardware:** ESP32 (Freenove DevKit) com sensores AHT20, ENS160, DS18B20/DHT22 e relés configuráveis.
- **Comunicação:** Wi-Fi STA ou AP, MQTT sobre TLS (`mqtt.greense.com.br:8883`) e servidor HTTP local.
- **Aplicação:** Automação da fase de germinação com monitoração ambiental, armazenamento local (SPIFFS/NVS) e interface web.
- **Documentação detalhada:** `./N01_Estufa_Germinar_C/README.md`.

### N02 · Estufa Maturar (`N02_Estufa_Maturar_C`)
- **Stack:** ESP-IDF (C) com componentes reaproveitados e LED RGB para status.
- **Sensores adicionais:** Boias de nível, sensor de luz, DS18B20 para reservatórios interno/externo, DHT22 externo.
- **Comunicação:** Wi-Fi STA com reconexão automática, MQTT seguro (TLS/WSS) e publicação JSON a cada 5s.
- **Aplicação:** Supervisão e atuação da fase de maturação com indicadores visuais e próximos passos já mapeados.

### N03 · Estufa (MicroPython) (`N03_Estufa_P`)
- **Stack:** MicroPython orientado a objetos (`main.py`, `conexao.py`, `sensores.py`).
- **Sensores:** AHT20 (ar), ENS160 (qualidade do ar), pH e EC com compensação de temperatura, boias de nível, entradas analógicas.
- **Atuadores:** Bomba d'água e LED NeoPixel (GPIO16) controlados via `ActuatorManager`.
- **Comunicação:** Wi-Fi STA + MQTT configurável; watchdogs via reconexão automática e resets por `machine`.
- **Aplicação:** Nó multi-parâmetro para cultivos com solução nutritiva (controle de reservatório).

### N04 · Estufa Câmera (`N04_Estufa_Camera_C`)
- **Stack:** ESP-IDF com componente `esp32-camera`, SDMMC e `esp_http_client`.
- **Funções:** Captura JPEG (XGA), uso de flash GPIO4, LED GPIO33 para status, armazenamento em SD e upload HTTPS com certificado embarcado.
- **Aplicação:** Supervisão visual remota de estufas com backup local das imagens.

### N05 · Estufa Térmica (C) (`N05_Estufa_Termica_C`)
- **Stack:** ESP-IDF no ESP32-C3 SuperMini com arquitetura BSP → APP → GUI.
- **Sensor principal:** MLX90640 (24×32) via UART, com conversão para °C, agendamento via NTP e envio HTTP JSON.
- **Aplicação:** Monitorar gradientes térmicos de estufas, com LED de estados, reconexão Wi-Fi e histórico de tempos.

### N05 · Estufa Térmica (MicroPython) (`N05_Estufa_Termica_P`)
- **Stack:** MicroPython para ESP32-C3, extraindo frames MLX90640 diretamente via UART.
- **Funções:** Upload HTTP periódico, sinalização por LED, configuração rápida no próprio `main.py`.
- **Aplicação:** Deploy ágil do termovisor em campo com menor complexidade de build.

### N06 · Sensor de Campo (C) (`N06_Sensor_Campo_C`)
- **Stack:** ESP-IDF com servidor HTTP embarcado, SPIFFS e data logger CSV.
- **Sensores:** Temperatura/umidade do ar, DS18B20 (solo) e umidade de solo via ADC com calibração.
- **Aplicação:** Nó de campo autônomo que cria seu próprio AP (`ESP32_TEMP`) e disponibiliza dashboard com gráficos e download de logs.

### N06 · Sensor de Campo (MicroPython) (`N06_Sensor_Campo_P`)
- **Stack:** MicroPython com módulos `sensores/`, `webserver/` e logger interno.
- **Recursos:** AP `ESP32_TEMP`, dashboard HTML/JS, calibração de solo via `/calibra`, log periódico para `log_temp.csv`.
- **Aplicação:** Alternativa rápida para coletar e calibrar dados de solo sem dependências do ESP-IDF.

### N07 · Estufa Artigo (`N07_Estufa_Artigo_C`)
- **Stack:** ESP-IDF para ESP32-S3 com camadas BSP/APP/GUI, PSRAM habilitada e partição customizada.
- **Sensores:** Câmera visual integrada + MLX90640 térmica, além de SD card e LED WS2812.
- **Recursos-chave:** Agendamentos independentes, sincronização NTP, HTTPS com retry/backoff, SPIFFS como buffer térmico, migração para SD com CRC e watchdog.
- **Aplicação:** Plataforma premium para pesquisas (artigo científico) combinando imagem visual e térmica, com scripts Python (`visualize_thermal.py`) para pós-processamento.

---

## ✅ Como usar este catálogo
1. Escolha o diretório do cliente desejado usando a tabela “Visão Geral”.
2. Consulte a seção de detalhes para entender sensores, protocolo de comunicação e fluxo de operação.
3. Abra o README específico ou o firmware correspondente dentro da pasta para instruções de build/flash.
4. Utilize as imagens desta galeria em relatórios, propostas ou documentação para o cliente.

Sinta-se à vontade para complementar este arquivo quando novos clientes (N08, N09, …) forem adicionados ou quando fotos atualizadas das placas estiverem disponíveis. 

