# 🔥 Câmera Térmica MLX90640 com ESP32-C3 (ESP-IDF)

Sistema embarcado em **C (ESP-IDF)** para aquisição de imagens térmicas usando o sensor **MLX90640** (módulo GY-MCU90640) e envio automático via **HTTP POST** para um servidor remoto, com sincronização NTP e aquisições agendadas por horários.

---

## ⚙️ Descrição Geral

O firmware executa em uma **placa ESP32-C3 SuperMini** conectada ao módulo **MLX90640BAB/BAA**, capturando quadros térmicos (24 × 32 pixels) via UART e enviando os dados como JSON para um endpoint HTTP configurável em horários pré-definidos.

O sistema realiza:
- 🧠 Captura e decodificação de frames (0x5A 0x5A)  
- 🌡️ Conversão binária → temperatura (°C)  
- 🌐 Conexão Wi-Fi com reconexão automática  
- ⏰ Sincronização NTP para horário real  
- 📅 Aquisições agendadas por horários configuráveis  
- 🔄 Envio de dados em JSON via HTTP POST  
- 💡 Sinalização por LED em diferentes estados de operação  

---

## 🧩 Hardware Utilizado

| Componente | Função | Interface |
|-------------|---------|-----------|
| **MLX90640BAB/BAA** | Câmera térmica 24 × 32 px | UART |
| **ESP32-C3 SuperMini** | Microcontrolador principal | USB-C, Wi-Fi, GPIO |
| **LED GPIO 8** | Indicador de status | Digital |
| **UART TX/RX (5/4)** | Comunicação com MLX90640 | UART1 |

| Sensor MLX90640 | ESP32-C3 SuperMini | Testes de Aquisição |
|-----------------|-------------------|-------------------|
| ![MLX90640](camera_termica.png) | ![ESP32-C3](esp32_c3.png) |![ESP32-C3](imagensTermicas.png) |

### Conexões

| MLX90640 | ESP32-C3 |
|-----------|-----------|
| VIN | 5 V |
| GND | G |
| RX | GPIO 5 |
| TX | GPIO 4 |

---

## 🧠 Arquitetura de Software

O projeto segue uma arquitetura modular em 3 camadas:

```
main/
├── main.c                    # Ponto de entrada
├── config.h                  # Configurações gerais
├── secrets.h                  # Credenciais Wi-Fi
│
├── bsp/                      # Board Support Package (Hardware)
│   ├── bsp_gpio.c/h          # Controle de GPIO (LED)
│   ├── bsp_uart.c/h          # Comunicação UART com MLX90640
│   └── bsp_wifi.c/h          # Conexão Wi-Fi e eventos
│
├── app/                      # Camada de Aplicação
│   ├── app_main.c/h          # Loop principal e orquestração
│   ├── app_thermal.c/h       # Captura e processamento térmico
│   ├── app_http.c/h          # Cliente HTTP para envio de dados
│   └── app_time.c/h          # Sincronização NTP e agendamento
│
└── gui/                      # Interface Gráfica (Feedback)
    └── gui_led.c/h           # Controle de LED e estados visuais
```

### Fluxo de Operação

1. **Inicialização**: GPIO, NVS, Wi-Fi, LED task
2. **Conexão Wi-Fi**: Reconexão automática em caso de falha
3. **Verificação de Internet**: Teste de conectividade HTTP
4. **Sincronização NTP**: Obtenção de horário real
5. **Loop Principal**:
   - Calcula próximo horário de aquisição
   - Aguarda até o horário programado
   - Captura frame térmico (24×32 = 768 pontos)
   - Envia dados via HTTP POST
   - Feedback visual via LED

---

## ⚙️ Configuração

### Credenciais Wi-Fi (`main/secrets.h`)

```c
#define WIFI_SSID "sua_rede"
#define WIFI_PASS "sua_senha"
```

### Configurações Gerais (`main/config.h`)

```c
// URL do servidor para envio de dados
#define URL_POST "http://greense.com.br/termica"

// Intervalo de leitura do sensor (segundos)
#define SENSOR_READ_INTERVAL 5

// Horários de aquisição (HH:MM)
#define ACQUISITION_TIMES \
    { \
        {12, 10},   \
        {12, 12},   \
        {12, 14},   \
        {12, 16}    \
    }
```

**Nota**: Os horários devem estar em ordem crescente e no formato 24 horas.

---

## 💡 Sistema de LED

O LED GPIO 8 fornece feedback visual do estado do sistema:

| Estado | Comportamento | Significado |
|--------|---------------|-------------|
| **Apagado** | LED desligado | Wi-Fi desconectado |
| **Piscando lento** | 1s ligado, 1s desligado | Wi-Fi conectado, sem internet |
| **Aceso** | LED ligado (fixo) | Wi-Fi conectado, com internet |
| **1 piscada rápida** | 200ms ligado/desligado | Envio de dados bem-sucedido |
| **3 piscadas rápidas** | 100ms cada, 3x | Erro no envio de dados |

---

## 🚀 Compilação e Execução

### Pré-requisitos

- ESP-IDF v5.0 ou superior
- Python 3.11+
- Ferramentas de compilação (GCC, CMake, Ninja)

### Compilação

```bash
# Ativar ambiente ESP-IDF
source $HOME/esp/esp-idf/export.sh

# Compilar o projeto
idf.py build

# Gravar na placa
idf.py flash -b 921600

# Monitorar logs
idf.py monitor
```

### Primeira Execução

1. Configure as credenciais Wi-Fi em `main/secrets.h`
2. Ajuste os horários de aquisição em `main/config.h`
3. Compile e grave o firmware
4. Observe o LED:
   - Apagado → Conectando ao Wi-Fi
   - Piscando lento → Conectado, verificando internet
   - Aceso → Tudo funcionando

---

## 🧾 Estrutura de Dados Enviada

O sistema envia dados em formato JSON via HTTP POST:

```json
{
  "temperaturas": [23.45, 23.60, 24.12, ..., 26.12],
  "timestamp": 1730269802
}
```

- **768 valores** de temperatura em °C (24 × 32 pixels)
- **Timestamp Unix** sincronizado via NTP (precisão de segundos)

### Exemplo de Requisição HTTP

```
POST http://greense.com.br/termica
Content-Type: application/json

{
  "temperaturas": [23.45, 23.60, ...],
  "timestamp": 1730269802
}
```

---

## ⏰ Sincronização NTP e Agendamento

O sistema utiliza **NTP (Network Time Protocol)** para sincronização de horário:

- **Servidores NTP**: `pool.ntp.org`, `a.st1.ntp.br`, `b.st1.ntp.br`
- **Timezone**: BRT3 (Brasília, UTC-3)
- **Timeout de sincronização**: 30 segundos
- **Validação**: Timestamp deve ser posterior a 2020-01-01

### Horários de Aquisição

As aquisições são agendadas conforme a tabela `ACQUISITION_TIMES` em `config.h`. O sistema:

1. Calcula o próximo horário de aquisição baseado na hora atual
2. Aguarda até o horário programado (verifica a cada minuto)
3. Executa a aquisição quando o horário é atingido
4. Envia os dados e aguarda o próximo horário

**Janela de aquisição**: 30 segundos após o horário programado.

---

## 🔄 Reconexão e Robustez

### Wi-Fi

- **Reconexão automática** em caso de desconexão
- **Até 15 tentativas** com delay progressivo (2s a 5s)
- **Reinicialização periódica** do Wi-Fi a cada 10 tentativas
- **Logs detalhados** dos motivos de desconexão

### HTTP

- **Até 3 tentativas** de envio em caso de falha
- **Delay de 3 segundos** entre tentativas
- **Timeout de 30 segundos** por requisição
- **Validação de status HTTP** (2xx = sucesso)

### NTP

- **Retry automático** se sincronização falhar
- **Validação de timestamp** antes de calcular horários
- **Re-sincronização** quando detecta timestamp inválido

---

## 🧩 Componentes ESP-IDF

Declarados em `main/CMakeLists.txt`:

```cmake
idf_component_register(
  SRCS
    "main.c"
    "bsp/bsp_gpio.c"
    "bsp/bsp_uart.c"
    "bsp/bsp_wifi.c"
    "app/app_main.c"
    "app/app_thermal.c"
    "app/app_http.c"
    "app/app_time.c"
    "gui/gui_led.c"
  INCLUDE_DIRS "."
  REQUIRES esp_wifi esp_http_client nvs_flash driver json esp_timer lwip
)
```

### Principais Bibliotecas

- `esp_wifi.h` – Conexão Wi-Fi STA com eventos
- `esp_http_client.h` – Cliente HTTP para POST
- `driver/uart.h` – Comunicação serial com MLX90640
- `esp_sntp.h` – Sincronização NTP
- `FreeRTOS` – Tasks para loop principal e LED
- `lwip` – Stack TCP/IP

---

## 🔋 Requisitos e Considerações

### Hardware

- ESP-IDF v5.0 ou superior
- UART 115200 bps
- Alimentação 5 V para o sensor MLX90640
- Frame: 24×32 = 768 pontos float
- Intervalo válido: –40 °C a 200 °C
- Wi-Fi 2.4 GHz ativo

### Software

- Python 3.11+
- ESP-IDF v5.0+
- Acesso à internet para NTP e envio de dados
- Servidor HTTP configurado para receber POST

### Performance

- **Tempo de aquisição**: ~3 segundos
- **Tempo de envio HTTP**: ~5-15 segundos (depende da rede)
- **Ciclo completo**: ~1-1.5 minutos (incluindo delays)
- **Consumo**: Baixo (Wi-Fi em modo STA, sem power saving)

---

## 🐛 Troubleshooting

### LED apagado após conexão Wi-Fi

- Verifique os logs para ver o estado atual do LED
- Confirme que a task do LED está rodando
- Verifique se a verificação de conectividade está funcionando

### NTP não sincroniza

- Verifique conexão com internet
- Confirme que o servidor NTP está acessível
- Aumente o timeout se necessário (em `app_time.c`)

### Erro HTTP 500

- Verifique o formato do JSON enviado
- Confirme que o servidor está configurado corretamente
- Verifique logs do servidor para mais detalhes

### Horários de aquisição incorretos

- Confirme que o NTP está sincronizado
- Verifique o timezone configurado (BRT3)
- Valide os horários em `config.h`

---

## 📊 Logs e Debug

O sistema gera logs detalhados para facilitar o debug:

- **BSP_WIFI**: Eventos de conexão Wi-Fi
- **APP_HTTP**: Requisições HTTP e respostas
- **APP_TIME**: Sincronização NTP e cálculo de horários
- **APP_THERMAL**: Captura de frames térmicos
- **GUI_LED**: Mudanças de estado do LED

Use `idf.py monitor` para visualizar os logs em tempo real.

---

## 🧪 Próximos Passos

- [ ] Implementar HTTPS com certificado
- [ ] Adicionar modo de aquisição contínua (não agendada)
- [ ] Visualização térmica em tempo real
- [ ] IA para detecção de eventos térmicos
- [ ] Armazenamento local em caso de falha de envio
- [ ] Interface web para configuração remota

---

## 📄 Licença

Licença **MIT**  
Desenvolvido por **Prof. Marcelino Monteiro de Andrade**  
**Universidade de Brasília (FCTE/UnB)**  
[https://github.com/marcelinoandrade/greense](https://github.com/marcelinoandrade/greense)

---

## 📝 Changelog

### Versão Atual
- ✅ Arquitetura modular (BSP, APP, GUI)
- ✅ Sincronização NTP
- ✅ Aquisições agendadas por horários
- ✅ Sistema de LED com feedback visual
- ✅ Reconexão automática Wi-Fi
- ✅ Retry automático de envio HTTP
- ✅ Logs formatados e legíveis
