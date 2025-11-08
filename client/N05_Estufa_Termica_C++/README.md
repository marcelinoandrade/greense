# 🔥 Câmera Térmica MLX90640 com ESP32-C3 (ESP-IDF)

Sistema embarcado em **C (ESP-IDF)** para aquisição de imagens térmicas usando o sensor **MLX90640** (módulo GY-MCU90640) e envio automático via **HTTP POST** para um servidor remoto.

---

## ⚙️ Descrição Geral

O firmware executa em uma **placa ESP32-C3 SuperMini** conectada ao módulo **MLX90640BAB/BAA**, capturando quadros térmicos (24 × 32 pixels) via UART e enviando periodicamente os dados como JSON para um endpoint HTTP configurável.

O sistema realiza:
- 🧠 Captura e decodificação de frames (0x5A 0x5A)  
- 🌡️ Conversão binária → temperatura (°C)  
- 🌐 Conexão Wi-Fi com reconexão automática  
- 🔄 Envio periódico de dados em JSON via HTTP POST  
- 💡 Sinalização por LED em diferentes estados de operação  

---

## 🧩 Hardware Utilizado

| Componente | Função | Interface |
|-------------|---------|-----------|
| **MLX90640BAB/BAA** | Câmera térmica 24 × 32 px | UART |
| **ESP32-C3 SuperMini** | Microcontrolador principal | USB-C, Wi-Fi, GPIO |
| **LED GPIO 8** | Indicador de status | Digital |
| **UART TX/RX (5/4)** | Comunicação com MLX90640 | UART1 |

### Conexões

| MLX90640 | ESP32-C3 |
|-----------|-----------|
| VIN | 5 V |
| GND | G |
| RX | GPIO 5 |
| TX | GPIO 4 |

---

## 🧠 Arquitetura de Software

```
main.c
├── Inicialização de NVS e Wi-Fi (STA)
├── Loop principal de captura térmica
│   ├── Leitura UART
│   ├── Decodificação e conversão para °C
│   ├── Montagem de JSON (768 valores + timestamp)
│   ├── Envio HTTP POST
│   └── Feedback via LED
└── Reconexão automática em falhas
```

---

## ⚙️ Configuração

Defina as credenciais Wi-Fi e o endpoint no arquivo `secrets.h`:

```c
#define WIFI_SSID "sua_rede"
#define WIFI_PASS "sua_senha"
#define URL_POST  "http://seu-servidor:porta/endpoint"
```

Parâmetro de intervalo de envio (em segundos):

```c
#define ENVIO_MS (90*1000)
```

---

## 🚀 Compilação e Execução

1. Instale o **ESP-IDF v5+**  
2. Copie este diretório para o workspace  
3. Compile e grave na placa:  
   ```bash
   idf.py build
   idf.py flash -b 921600
   idf.py monitor
   ```
4. O LED indicará:
   - 🔴 piscando rápido → conectando ao Wi-Fi  
   - 🟢 piscando lento → conectado  
   - ✅ uma piscada → envio HTTP 200 OK  
   - 🌐 múltiplas piscadas → erro ou reconexão  

---

## 🧾 Estrutura de Dados Enviada

```json
{
  "temperaturas": [23.45, 23.60, ..., 26.12],
  "timestamp": 1730269802
}
```

- 768 valores de temperatura em °C  
- Timestamp Unix gerado por `esp_timer_get_time()`  

---

## 🧩 Componentes ESP-IDF

Declarados em `CMakeLists.txt`:

```
idf_component_register(
  SRCS "main.c"
  REQUIRES esp_wifi esp_http_client nvs_flash driver json esp_timer
)
```

Principais bibliotecas usadas:
- `esp_wifi.h` – conexão Wi-Fi STA  
- `esp_http_client.h` – envio HTTP POST  
- `uart.h` – comunicação serial com MLX90640  
- `esp_timer.h` – timestamp  
- `FreeRTOS` Tasks para loop principal e LED  

---

## 🔋 Requisitos e Considerações

- ESP-IDF v5.0 ou superior  
- UART 115200 bps  
- Alimentação 5 V para o sensor  
- Frame: 24×32 = 768 pontos float  
- Intervalo válido: –40 °C a 200 °C  
- Wi-Fi 2.4 GHz ativo  

---

## 🧪 Próximos Passos

- Armazenamento local em SDCard  
- Integração com Flask no Raspberry Pi  
- Visualização térmica em tempo real  
- IA para detecção de eventos térmicos  

---

## 📄 Licença

Licença **MIT**  
Desenvolvido por **Prof. Marcelino Monteiro de Andrade**  
**Universidade de Brasília (FCTE/UnB)**  
[https://github.com/marcelinoandrade/greense](https://github.com/marcelinoandrade/greense)
