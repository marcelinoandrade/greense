# N02 · Estufa Maturar · Monitoramento Completo (ESP32)

Firmware baseado em **ESP-IDF v5.x** para monitoramento e controle ambiental, integrando sensores múltiplos (AHT20, ENS160, DHT22, DS18B20) e atuadores, com comunicação segura via **MQTT sobre TLS**.

---

## Descrição Geral

Nó IoT completo para agricultura inteligente, capaz de coletar dados ambientais em tempo real, acionar dispositivos e enviar informações a um servidor remoto via MQTT seguro. Sistema monitora temperatura, umidade, qualidade do ar, níveis de água e outras variáveis ambientais críticas para cultivo em estufas.

### Recursos Principais

- Conexão Wi-Fi (modo STA) com reconexão automática
- Comunicação **MQTT segura (TLS/WSS)** usando certificado embutido
- Sensores integrados:
  - **AHT20** – temperatura e umidade do ar (I2C)
  - **ENS160** – qualidade do ar e eCO₂ (I2C)
  - **DS18B20** – temperatura do reservatório interno (OneWire)
  - **DHT22** – temperatura e umidade externas (GPIO)
  - **Boias de nível** – detecção de água mínima/máxima (GPIO)
  - **Sensor de luz** – detecção de claridade (GPIO)
- Atuadores:
  - **LED RGB** – indicador visual de status do sistema (GPIO 16)
- Armazenamento local (NVS) para configurações
- Arquitetura modular: conexões, sensores e atuadores independentes
- Loop de leitura automático a cada 5 segundos

---

## Estrutura de Diretórios

```
main/
├── main.c                  # Inicialização e loop principal
├── config.h                # Configurações gerais (MQTT, intervalos)
├── secrets.h               # Credenciais Wi-Fi (criar este arquivo)
├── conexoes/
│   ├── conexoes.c/.h       # Configuração de Wi-Fi e MQTT
├── sensores/
│   ├── sensores.c/.h       # Integração geral dos sensores
│   ├── aht20.c/.h          # Sensor de temperatura e umidade
│   ├── ens160.c/.h         # Sensor de qualidade do ar
│   ├── ds18b20.c/.h        # Sensor de temperatura do solo
│   ├── dht.c/.h            # Sensor DHT22 (temperatura/umidade externa)
├── atuadores/
│   ├── atuadores.c/.h      # Controle de LED RGB
├── CMakeLists.txt          # Configuração de build e dependências
└── idf_component.yml       # Dependências de componentes
```

---

## Comunicação MQTT

### Configuração

- **Broker**: `mqtt.greense.com.br`
- **Porta**: `8883` (TLS/WSS)
- **Biblioteca**: `esp-mqtt`
- **Certificado**: embutido no firmware (referenciado como binário)
- **Protocolo**: WSS (WebSocket Secure)

### Tópicos

| Tópico | Direção | Descrição |
|--------|---------|-----------|
| `estufa/maturar` | → broker | Publicação de dados ambientais (a cada 5 segundos) |

### Formato dos Dados Publicados

```json
{
  "temp": 25.50,              // Temperatura do ar (°C) - AHT20
  "umid": 65.20,             // Umidade do ar (%) - AHT20
  "co2": 420.00,             // eCO₂ (ppm) - ENS160
  "luz": 1.00,               // Sensor de luz (0=escuro, 1=claro)
  "agua_min": 0,             // Boia nível mínimo (0=baixo, 1=ok)
  "agua_max": 1,             // Boia nível máximo (0=ok, 1=cheio)
  "temp_reserv_int": 22.30,  // Temperatura reservatório interno (°C) - DS18B20
  "ph": 0.00,                // pH (simulado, não implementado)
  "ec": 0.00,                // Condutividade elétrica (simulado, não implementado)
  "temp_reserv_ext": 20.15,  // Temperatura reservatório externo (°C) - simulado
  "temp_externa": 24.80,     // Temperatura externa (°C) - DHT22
  "umid_externa": 70.50      // Umidade externa (%) - DHT22
}
```

---

## Configuração

### 1. Arquivo `secrets.h`

Crie o arquivo `main/secrets.h` com suas credenciais Wi-Fi:

```c
#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID "sua_rede_wifi"
#define WIFI_PASS "sua_senha_wifi"

#endif // SECRETS_H
```

⚠️ **Importante**: Este arquivo não deve ser commitado no repositório. Adicione `secrets.h` ao `.gitignore`.

### 2. Configurações em `config.h`

Principais configurações em `main/config.h`:

- `MQTT_BROKER`: Endereço do broker MQTT
- `MQTT_TOPIC`: Tópico para publicação
- `MQTT_CLIENT_ID`: Identificador do cliente
- `SENSOR_READ_INTERVAL`: Intervalo de leitura (em segundos)

---

## Indicadores Visuais

O sistema utiliza um **LED RGB** (GPIO 16) para indicar o status:

| Cor | Estado |
|-----|--------|
| Vermelho (10, 0, 0) | Wi-Fi ou MQTT desconectado |
| Azul (0, 0, 10) | Sistema conectado e operacional |

---

## Hardware de Referência

![ESP32](esp32_Freenove.png)

### Pinos Utilizados

- **I2C (AHT20, ENS160)**:
  - SDA: GPIO 21
  - SCL: GPIO 22
- **OneWire (DS18B20)**: GPIO 26
- **DHT22**: GPIO 4
- **Boias de nível**: GPIO 32 (mínimo), GPIO 33 (máximo)
- **Sensor de luz**: GPIO 25
- **LED RGB**: GPIO 16

---

## Requisitos de Build

### Ferramentas

- **ESP-IDF ≥ 5.0**
- **Python 3.x**
- `idf.py`, `esptool.py`, `menuconfig`

### Componentes Utilizados

- `esp_wifi` – Gerenciamento Wi-Fi
- `esp_event` – Sistema de eventos
- `mqtt` – Cliente MQTT
- `nvs_flash` – Armazenamento não volátil
- `driver` – Drivers de hardware
- `led_strip` – Controle de LED RGB (componente externo: `espressif/led_strip`)

---

## Como Executar

### 1. Configurar o ambiente

```bash
# Configure o ESP-IDF (se ainda não configurado)
. $HOME/esp/esp-idf/export.sh

# Navegue até o diretório do projeto
cd N02_Estufa_Maturar_C
```

### 2. Criar arquivo de credenciais

```bash
# Crie o arquivo secrets.h
cat > main/secrets.h << EOF
#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID "sua_rede_wifi"
#define WIFI_PASS "sua_senha_wifi"

#endif // SECRETS_H
EOF
```

### 3. Configurar e compilar

```bash
# Configure o alvo (ESP32)
idf.py set-target esp32

# (Opcional) Configure opções avançadas
idf.py menuconfig

# Compile o projeto
idf.py build
```

### 4. Gravar no dispositivo

```bash
# Grave o firmware no ESP32
idf.py flash

# Monitore a saída serial
idf.py monitor
```

### 5. Verificar funcionamento

Após a inicialização, você deve ver nos logs:

1. ✅ Inicialização do NVS
2. ✅ Conexão Wi-Fi estabelecida
3. ✅ Conexão MQTT estabelecida
4. ✅ Inicialização dos sensores
5. ✅ Publicação periódica de dados no tópico `estufa/maturar`

---

## Testes de Campo

- ✅ Testado em **ESP32-WROOM-32** e **ESP32-S3**
- ✅ Comunicação validada com broker MQTT seguro (TLS)
- ✅ Operação estável em Wi-Fi 2.4 GHz
- ✅ Reconexão automática em caso de falha de conexão
- ✅ Leitura contínua de sensores a cada 5 segundos

---

## Estrutura de Dados dos Sensores

```c
typedef struct {
    float temp;              // Temperatura do ar (°C)
    float umid;              // Umidade do ar (%)
    float co2;               // eCO₂ (ppm)
    float luz;               // Sensor de luz (0 ou 1)
    int agua_min;            // Boia nível mínimo
    int agua_max;            // Boia nível máximo
    float temp_reserv_int;   // Temperatura reservatório interno (°C)
    float ph;                // pH (atualmente simulado)
    float ec;                // Condutividade elétrica (atualmente simulado)
    float temp_reserv_ext;   // Temperatura reservatório externo (°C)
    float temp_externa;      // Temperatura externa (°C)
    float umid_externa;      // Umidade externa (%)
} sensor_data_t;
```

---

## Troubleshooting

### Wi-Fi não conecta

- Verifique se `secrets.h` existe e contém credenciais corretas
- Verifique se a rede Wi-Fi está no alcance e operacional
- Verifique os logs para mensagens de erro específicas

### MQTT não conecta

- Verifique se o broker `mqtt.greense.com.br` está acessível
- Verifique se o certificado está corretamente embutido
- Verifique a porta 8883 (TLS) não está bloqueada por firewall

### Sensores não leem dados

- Verifique as conexões I2C (AHT20, ENS160)
- Verifique a conexão OneWire (DS18B20)
- Verifique os pinos GPIO configurados corretamente
- Consulte os logs para erros de inicialização

### LED não acende

- Verifique se o LED RGB está conectado ao GPIO 16
- Verifique a alimentação do LED
- Verifique se `atuadores_init()` foi chamado

---

## Licença

Este projeto faz parte do Projeto GreenSe da Universidade de Brasília.

**Autoria**: Prof. Marcelino Monteiro de Andrade  
**Instituição**: Faculdade de Ciências e Tecnologias em Engenharia (FCTE) – Universidade de Brasília  
**Email**: [andrade@unb.br](mailto:andrade@unb.br)  
**Website**: [https://greense.com.br](https://greense.com.br)
