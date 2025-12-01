# 📷 Câmera ESP32-S3 com Envio Automático e Câmera Térmica (ESP-IDF)

Sistema embarcado em **C (ESP-IDF)** para captura de imagens usando **ESP32-S3 WROOM (N16R8)** com câmera visual, câmera térmica MLX90640, slot SD integrado, e envio automático via **HTTPS POST** para um servidor remoto, com armazenamento local em cartão SD. Sistema de agendamento independente para ambas as câmeras com sincronização NTP.

![ESP32-S3](imagens/esp32s3.jpg)

---

## ⚙️ Descrição Geral

O firmware executa em uma placa **ESP32-S3 WROOM (N16R8)** com câmera e slot SD integrados, capturando imagens JPEG periodicamente e enviando-as via HTTPS para um endpoint configurável. As imagens são enviadas diretamente para o servidor (salvamento local no SD card desativado).

O sistema realiza:
- 📸 Captura de imagens JPEG (XGA - 1024×768) da câmera visual
- 🌡️ Captura de dados térmicos (24×32 pixels) da câmera MLX90640
- ⏰ Agendamento independente para câmera visual e térmica
- 🕐 Sincronização NTP para timestamps precisos e agendamento baseado em horários
- 📊 Logs informativos com próxima aquisição agendada
- 💾 Armazenamento local em cartão SD (apenas dados térmicos binários - imagens JPG não são salvas)
- 🌐 Conexão Wi-Fi com reconexão automática  
- 🔒 Envio seguro via HTTPS com certificado SSL  
- 💡 Sinalização por LED RGB (WS2812) para indicar estado do Wi-Fi  
- ⚡ Flash LED para iluminação durante captura visual
- 🧠 Arquitetura modular (BSP/APP/GUI)
- 🐍 Script Python para visualização de dados térmicos
- 🔄 **Sistema de acumulação térmica em SPIFFS com migração automática para SD card**
- 📤 **Envio imediato de dados térmicos após captura (sem duplicação ou reenvio)**
- ✅ **Verificação de integridade de dados (checksums CRC32 e read-after-write)**
- 📝 **Metadados com timestamps separados dos dados binários**
- 📦 **Preservação de histórico completo (todos os frames enviados acumulados)**
- 🔐 **Thread safety completo com mutexes para operações de SD card**
- 🛡️ **Watchdog timer configurado para monitoramento de tasks críticas**
- 🚫 **Proteção contra buffer overflow com alocação dinâmica**
- 🔁 **Retry HTTP com backoff exponencial para robustez de rede**

---

## 🧩 Hardware Utilizado

| Componente | Função | Interface |
|-------------|---------|-----------|
| **ESP32-S3 WROOM (N16R8)** | Microcontrolador + Câmera integrada | USB-C, Wi-Fi, GPIO |
| **Câmera Térmica MLX90640** | Sensor térmico 24×32 pixels | UART (GPIO14) |
| **Cartão SD** | Armazenamento local | SDMMC (slot integrado) |
| **LED RGB WS2812 (GPIO48)** | Indicador de status Wi-Fi | Digital (SPI/RMT) |
| **Flash LED (GPIO21)** | Iluminação para fotos | Digital |

### Especificações da Placa

- **Chip:** ESP32-S3 (Dual-core Xtensa LX7, 240MHz)
- **Flash:** 16MB (N16R8)
- **PSRAM:** 8MB (OCT SPI PSRAM)
- **Câmera:** Integrada (OV2640 ou similar)
- **SD Card:** Slot integrado (SDMMC)

### Câmera Visual OV2640

![Câmera Visual OV2640](imagens/camera_visual.png)

A câmera visual OV2640 integrada na placa ESP32-S3 possui:
- **Resolução:** XGA (1024×768 pixels)
- **Campo de visão:** 120° (ângulo amplo)
- **Formato:** JPEG comprimido
- **Qualidade:** Configurável (padrão: 12)
- **Interface:** DVP (Digital Video Port) integrada

### Pinos da ESP32-S3 WROOM

| Função | GPIO | Descrição |
|--------|------|-----------|
| **Câmera** | | |
| PWDN | -1 | Power Down (não usado) |
| RESET | 47 | Reset da câmera |
| XCLK | 15 | Clock da câmera |
| SIOD | 4 | I2C Data |
| SIOC | 5 | I2C Clock |
| Y9 | 16 | Dados da câmera (D7) |
| Y8 | 17 | Dados da câmera (D6) |
| Y7 | 18 | Dados da câmera (D5) |
| Y6 | 12 | Dados da câmera (D4) |
| Y5 | 10 | Dados da câmera (D3) |
| Y4 | 8 | Dados da câmera (D2) |
| Y3 | 9 | Dados da câmera (D1) |
| Y2 | 11 | Dados da câmera (D0) |
| VSYNC | 6 | Sincronização vertical |
| HREF | 7 | Horizontal Reference |
| PCLK | 13 | Pixel Clock |
| **SD Card** | | |
| SD_DATA | 40 | SDMMC Data |
| SD_CLK | 39 | SDMMC Clock |
| SD_CMD | 38 | SDMMC Command |
| **LEDs** | | |
| LED Status (WS2812) | 48 | LED RGB de status |
| Flash LED | 21 | LED de iluminação |
| **I2C (Sensores)** | | |
| I2C SDA | 1 | I2C Data (alternativa) |
| I2C SCL | 2 | I2C Clock (alternativa) |
| **UART (Câmera Térmica)** | | |
| UART RX | 14 | Recebe dados do MLX90640 |
| UART TX | 3 | Transmissão (configurado, não usado) |

---

## 🧠 Arquitetura de Software

O projeto utiliza uma arquitetura modular em três camadas:

```
main/
├── app/                        # Camada de Aplicação
│   ├── app_main.c             # Lógica principal e tasks periódicas
│   ├── app_http.c             # Cliente HTTPS para envio de imagens
│   ├── app_time.c/.h          # Sincronização NTP e agendamento
│   └── app_thermal.c/.h       # Captura de dados térmicos MLX90640
│
├── bsp/                        # Board Support Package (Hardware)
│   ├── bsp_pins.h             # Definição de pinagem
│   ├── bsp_gpio.c/.h          # Controle de GPIO
│   ├── bsp_wifi.c/.h          # Configuração Wi-Fi
│   ├── bsp_camera.c/.h        # Inicialização da câmera visual
│   ├── bsp_sdcard.c/.h        # Sistema de arquivos SD Card
│   ├── bsp_spiffs.c/.h        # Sistema de arquivos SPIFFS (buffer térmico)
│   └── bsp_uart.c/.h          # Comunicação UART com MLX90640
│
└── gui/                        # Interface Gráfica/Usuário
    └── gui_led.c/.h           # Controle de LED RGB (WS2812)
```

### Fluxo de Execução

```
app_main()
├── Inicialização de NVS
├── Inicialização do Wi-Fi (STA) - bloqueia até conectar
├── Sincronização NTP para tempo real
├── Inicialização do cartão SD
├── Inicialização do SPIFFS (buffer térmico)
├── Inicialização da câmera visual (OV2640)
├── Inicialização da câmera térmica (MLX90640 via UART)
├── Inicialização do LED RGB (WS2812)
├── Task de captura visual (agendamento independente)
│   ├── Verificação de horário agendado
│   ├── Log de próxima aquisição
│   ├── Ativação do flash LED
│   ├── Captura de imagem JPEG
│   ├── Envio HTTPS POST
│   └── Liberação do buffer
├── Task de captura térmica (agendamento independente)
│   ├── Verificação de horário agendado
│   ├── Log de próxima aquisição
│   ├── Captura de frame térmico (24×32)
│   ├── Cálculo de estatísticas (min/max/média)
│   ├── Acumulação em SPIFFS (com timestamp)
│   ├── Envio imediato para servidor (se Wi-Fi conectado)
│   ├── Migração para SD Card (quando atinge limite, apenas arquivamento)
│   └── Verificação de integridade (checksums)
└── Task de arquivamento térmico (sem reenvio HTTP)
    ├── Verifica arquivos acumulativos no SD card
    ├── Move para histórico (THERMS.BIN)
    └── Remove arquivos temporários
└── Loop principal (monitoramento Wi-Fi e reconexão)
```

---

## ⚙️ Configuração

Defina as credenciais Wi-Fi, endpoints HTTP e configurações MQTT no arquivo `main/secrets.h`:

```c
#define WIFI_SSID "sua_rede"
#define WIFI_PASS "sua_senha"

// MQTT (futuramente)
#define MQTT_BROKER "mqtt.greense.com.br"
#define MQTT_TOPIC "estufa/artigo"
#define MQTT_CLIENT_ID "Estufa_Artigo"
#define MQTT_KEEPALIVE 60

// HTTP/HTTPS Upload
#define CAMERA_UPLOAD_URL "https://camera02.greense.com.br/upload"
#define CAMERA_THERMAL_UPLOAD_URL "https://camera02.greense.com.br/termica"
```

**Importante:** O certificado SSL do servidor deve estar em `main/certs/greense_cert.pem` (ou ajuste o caminho no `CMakeLists.txt`).

### Sistema de Agendamento

O sistema utiliza agendamento baseado em horários reais (sincronizado via NTP) com vetores independentes para cada câmera. Isso permite configurar horários diferentes para capturas visuais e térmicas.

**Características:**
- ✅ Agendamento independente para câmera visual e térmica
- ✅ Sincronização NTP para precisão temporal
- ✅ Logs informativos com próxima aquisição
- ✅ Suporte a múltiplos horários por dia
- ✅ Cálculo automático do próximo horário (incluindo próximo dia)

Configure os horários em `main/config.h`:

```c
// Agendamento para câmera espectro visual
#define CAMERA_VISUAL_SCHEDULE_SIZE 48
static const schedule_time_t camera_visual_schedule[CAMERA_VISUAL_SCHEDULE_SIZE] = {
    {0, 0},   {0, 30},  {1, 0},   {1, 30},  // [00:00, 00:30, 01:00, 01:30, ...]
    // ... até 23:30
};

// Agendamento para câmera térmica (independente)
#define CAMERA_THERMAL_SCHEDULE_SIZE 48
static const schedule_time_t camera_thermal_schedule[CAMERA_THERMAL_SCHEDULE_SIZE] = {
    {0, 0},   {0, 30},  {1, 0},   {1, 30},  // [00:00, 00:30, 01:00, 01:30, ...]
    // ... até 23:30
};
```

**Formato:** `{hora, minuto}` onde hora é 0-23 e minuto é 0-59.

### Parâmetros de Captura Visual

Configurados em `main/bsp/bsp_camera.c`:
- **Resolução:** XGA (1024×768)
- **Qualidade JPEG:** 12 (0-63, menor = melhor qualidade)
- **Formato:** JPEG
- **Frame Size:** PSRAM (buffer alocado em PSRAM)
- **Agendamento:** Baseado em horários definidos em `config.h`

### Parâmetros de Captura Térmica

Configurados em `main/config.h`:
- **Resolução:** 24 linhas × 32 colunas = 768 pixels
- **Sensor:** MLX90640 via UART (115200 baud)
- **Formato:** Floats (temperatura em °C)
- **THERMAL_SAVE_INTERVAL:** Número de frames para acumular antes de migrar para SD (padrão: 3)
- **THERMAL_SPIFFS_MAX_SIZE:** Tamanho máximo em SPIFFS antes de migração (calculado automaticamente)
- **THERMAL_MIGRATION_CHUNK_SIZE:** Tamanho dos chunks para migração (padrão: 10 frames = ~30 KB)
- **CAMERA_THERMAL_UPLOAD_URL:** Endpoint para envio de dados térmicos (definido em `secrets.h`, padrão: `https://camera02.greense.com.br/termica`)
- **Agendamento:** Baseado em horários definidos em `config.h` (independente da câmera visual)

---

## 🚀 Compilação e Execução

1. Instale o **ESP-IDF v5.0+**  
2. Configure o alvo para ESP32-S3:
   ```bash
   idf.py set-target esp32s3
   ```
3. Configure o certificado SSL:
   ```bash
   mkdir -p main/certs
   # Copie o certificado do servidor para main/certs/greense_cert.pem
   ```
4. Compile e grave na placa:  
   ```bash
   idf.py build
   idf.py flash -b 921600
   idf.py monitor
   ```

### Configurações Importantes no `sdkconfig`

- **Flash Size:** 16MB (`CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`)
- **PSRAM:** Habilitado (OCT SPI, 80MHz)
- **Target:** ESP32-S3 (`CONFIG_IDF_TARGET_ESP32S3=y`)
- **Partition Table:** Custom (`partitions.csv`)

---

## 🌡️ Câmera Térmica MLX90640

![Câmera Térmica MLX90640](imagens/camera_termica.png)

O sistema integra uma câmera térmica MLX90640 conectada via UART:

### Especificações
- **Resolução:** 24 linhas × 32 colunas = 768 pixels
- **Interface:** UART (115200 baud)
- **GPIO RX:** GPIO14
- **Faixa de Temperatura:** -40°C a +200°C
- **Precisão:** ±0.5°C (em condições ideais)

### Funcionamento
- Captura frames térmicos baseado em agendamento independente
- Calcula estatísticas (temperatura mínima, máxima e média)
- **Envia imediatamente** para servidor após captura e salvamento no SPIFFS
- **Acumula frames em SPIFFS** antes de migrar para SD Card
- **Migração automática** quando atinge limite configurado (SD card serve como backup/histórico)
- **Sem envio duplicado** - cada frame é enviado apenas uma vez (não há reenvio após migração)
- **Apenas arquivamento** - task de envio térmico agora apenas arquiva dados no SD card (sem reenvio HTTP)
- Armazena dados em formato binário (floats) com metadados separados

### Sistema de Acumulação e Migração

O sistema utiliza uma arquitetura de dois níveis para armazenamento térmico:

```
Captura → SPIFFS → Envio Imediato → Servidor
              ↓
         Migração (quando buffer cheio)
              ↓
         SD Card (backup/histórico)
```

**Fluxo detalhado:**

1. **Captura:** Cada frame térmico (768 floats = 3072 bytes) é capturado e anexado ao arquivo acumulativo em SPIFFS
2. **Envio Imediato:** Frame é enviado imediatamente para servidor via HTTPS após salvamento no SPIFFS (se Wi-Fi conectado)
3. **Metadados:** Timestamp de cada frame é salvo em arquivo JSON separado em SPIFFS
4. **Migração:** Quando SPIFFS atinge `THERMAL_SPIFFS_MAX_SIZE`, os dados são migrados para SD card em chunks (apenas arquivamento)
5. **Verificação:** Durante migração, cada chunk é verificado com read-after-write e checksum CRC32
6. **Arquivamento:** Task de arquivamento move dados do SD card para histórico (`THERMS.BIN`) - sem reenvio HTTP
7. **Histórico:** Dados migrados são anexados ao arquivo histórico (`THERMS.BIN`) no SD card (já foram enviados anteriormente)

**Arquivos no SD Card:**

| Arquivo | Descrição | Formato |
|---------|-----------|---------|
| `THERML.BIN` | Dados térmicos acumulativos (migrados do SPIFFS) | Binário: frames concatenados (768 floats cada) |
| `THERMLM.TXT` | Metadados com timestamps (migrados do SPIFFS) | JSON: `{"frames":[{"timestamp":...},...]}\n` |
| `THERMS.BIN` | Dados térmicos históricos (backup completo) | Binário: todos os frames migrados concatenados |
| `THERMSM.TXT` | Metadados históricos (backup completo) | JSON: todos os timestamps concatenados |

**Nota:** Os frames em `THERML.BIN` já foram enviados para o servidor antes da migração. O SD card serve apenas como backup/histórico. Não há reenvio HTTP após migração - a task de envio térmico foi convertida para apenas arquivamento.

**Configuração em `main/config.h`:**

```c
// Intervalo de acumulação (quantos frames antes de migrar)
#define THERMAL_SAVE_INTERVAL 3  // 3 frames = ~9 KB em SPIFFS

// Limite máximo em SPIFFS (calculado automaticamente)
#define THERMAL_SPIFFS_MAX_SIZE (THERMAL_SAVE_INTERVAL * 768 * sizeof(float))

// Tamanho do chunk para migração (reduz uso de memória)
#define THERMAL_MIGRATION_CHUNK_SIZE (10 * 768 * sizeof(float))  // ~30 KB por chunk

// URL do endpoint para envio de dados térmicos (definido em secrets.h)
// #define CAMERA_THERMAL_UPLOAD_URL "https://camera02.greense.com.br/termica"
```

### Verificação de Integridade

O sistema implementa múltiplas camadas de verificação para garantir a integridade dos dados:

1. **Checksum CRC32:** Cada frame em SPIFFS tem checksum calculado e validado durante escrita e leitura
2. **Read-after-write:** Dados escritos no SD card são lidos imediatamente após escrita para verificação
3. **Validação de chunks:** Durante migração, cada chunk é verificado antes de continuar para o próximo
4. **Retry com backoff exponencial:** Operações críticas têm retry automático (3 tentativas: 1s, 2s, 4s)
5. **Validação de tamanho:** Tamanho do arquivo é verificado após cada operação de escrita
6. **Verificação de metadados:** Metadados são validados antes de limpar dados temporários em SPIFFS
7. **Migração atômica:** SPIFFS só é limpo após confirmação completa de migração (dados + metadados)

### Envio Imediato para Servidor

O sistema envia frames térmicos **imediatamente após captura**:

- **Envio Imediato:** Frame é enviado logo após captura e salvamento no SPIFFS
- **Sem Duplicação:** Cada frame é enviado apenas uma vez (não há reenvio após migração)
- **Apenas Arquivamento:** Task de envio térmico foi convertida para apenas arquivamento no SD card (sem reenvio HTTP)
- **Retry automático:** HTTP client tem retry com backoff exponencial (3 tentativas: 1s, 2s, 4s)
- **Validação de status:** Apenas códigos HTTP 2xx são considerados sucesso
- **Callback HTTP:** Reseta watchdog durante toda a transferência HTTP (previne timeouts)
- **Timeout robusto:** Aguarda até 30 segundos por resposta do servidor sem resetar o sistema
- **SD Card como Backup:** Dados migrados para SD card servem apenas como backup/histórico (já foram enviados)

**Formato de envio para servidor:**

```json
{
  "temperaturas": [25.5, 26.0, 26.5, ...],  // 768 valores float
  "timestamp": 1763809109                    // Unix timestamp
}
```

**Endpoint:** Configurado em `CAMERA_THERMAL_UPLOAD_URL` em `secrets.h` (padrão: `https://camera02.greense.com.br/termica`)

### Dados Capturados

Cada frame contém 768 valores de temperatura (em °C) organizados em uma matriz 24×32. Os dados são salvos em arquivos binários acumulativos que podem ser visualizados usando o script Python incluído.

**Estrutura do arquivo binário acumulativo:**
```
[Frame 1: 3072 bytes] [Frame 2: 3072 bytes] ... [Frame N: 3072 bytes]
```

**Estrutura do arquivo de metadados:**
```json
{"frames":[{"timestamp":1234567890,"datetime":"2025-01-20 08:00:00"}]}
{"frames":[{"timestamp":1234567900,"datetime":"2025-01-20 08:00:10"}]}
...
```

## 💡 Sinalização por LED

O LED RGB WS2812 (GPIO48) indica o estado da conexão:

| Estado | Indicação | Descrição |
|---------|-----------|-----------|
| 🔴 **Sem Wi-Fi** | LED vermelho | Tentando conectar à rede Wi-Fi |
| 🔵 **Wi-Fi Conectado** | LED azul | Conectado à rede com IP válido |
| 🟢 **Sucesso** | LED verde (flash) | Envio de imagem bem-sucedido |
| 🟡 **Erro** | LED amarelo (flash) | Erro durante operação |

O Flash LED (GPIO21) é ativado durante a captura de imagem para iluminação.

---

## 💾 Armazenamento no SD Card

### Imagens Visuais

**Nota:** O salvamento de imagens JPG no SD card foi desativado. As imagens são enviadas diretamente para o servidor via HTTPS e não são salvas localmente.

### Dados Térmicos

**Sistema de Arquivos Acumulativos:**

Os dados térmicos são salvos em arquivos acumulativos que crescem ao longo do tempo:

- **Arquivo de dados:** `THERML.BIN` (pendentes) ou `THERMS.BIN` (enviados/histórico)
- **Arquivo de metadados:** `THERMLM.TXT` (pendentes) ou `THERMSM.TXT` (enviados/histórico)
- **Formato:** Frames concatenados (768 floats = 3072 bytes cada)
- **Metadados:** Timestamps em JSON separado para cada frame

**Fluxo de armazenamento:**

1. **Captura e Envio:** Frame é capturado, salvo no SPIFFS e enviado imediatamente para servidor
2. **SPIFFS (temporário):** Frames são acumulados até atingir `THERMAL_SPIFFS_MAX_SIZE`
3. **Migração:** Dados são migrados para SD card em chunks com verificação de integridade (apenas arquivamento)
4. **SD Card (backup):** Arquivo acumulativo cresce com cada migração (dados já foram enviados)
5. **Arquivamento:** Task de arquivamento move dados para histórico (`THERMS.BIN`) - sem reenvio HTTP
6. **Histórico:** Dados migrados são anexados ao histórico (`THERMS.BIN`) no SD card

**Vantagens do sistema acumulativo:**

- ✅ **Eficiência:** Reduz número de operações de escrita no SD card
- ✅ **Robustez:** Dados em SPIFFS são protegidos durante migração
- ✅ **Integridade:** Verificação de dados em múltiplas etapas
- ✅ **Escalabilidade:** Suporta milhares de frames sem fragmentação excessiva
- ✅ **Histórico completo:** Todos os frames enviados são preservados em `THERMS.BIN`
- ✅ **Sem duplicação:** Cada frame é enviado apenas uma vez (não há reenvio após migração)

O cartão SD é montado em `/sdcard` e deve ser formatado em FAT32.

**Configuração do SD Card:**
- Modo: 1-bit SDMMC
- Pull-ups internos habilitados
- Sistema de arquivos FAT32
- Slot integrado na placa

---

## 📋 Tabela de Partição

O projeto utiliza uma tabela de partição customizada para ESP32-S3 N16R8 (16MB flash):

| Nome | Tipo | Subtipo | Tamanho | Descrição |
|------|------|---------|---------|-----------|
| **NVS** | data | nvs | 24KB | Armazenamento não-volátil |
| **PHY Init** | data | phy | 4KB | Dados de inicialização PHY |
| **Factory** | app | factory | 3MB | Aplicação principal |
| **SPIFFS** | data | spiffs | 12MB | Sistema de arquivos para buffer térmico e dados temporários |

**Uso do SPIFFS:**
- Buffer temporário para frames térmicos antes de migração
- Proteção contra perda de dados durante reinicializações
- Limite configurável via `THERMAL_SPIFFS_MAX_SIZE`

A tabela está definida em `partitions.csv`.

---

## 🔒 Segurança e Robustez

O sistema implementa múltiplas camadas de segurança e robustez:

### Segurança de Comunicação
- **HTTPS** para envio seguro das imagens e dados térmicos
- Certificado SSL embutido no firmware
- Validação do certificado do servidor
- Timeout de 30 segundos para requisições HTTP
- **Tratamento correto de status HTTP:** Apenas códigos 2xx são considerados sucesso
- **Retry com backoff exponencial:** 3 tentativas (1s, 2s, 4s) para erros de rede
- **Callback HTTP robusto:** Reseta watchdog em todos os eventos HTTP durante transferências longas
  - Eventos monitorados: `HTTP_EVENT_ON_CONNECTED`, `HTTP_EVENT_HEADER_SENT`, `HTTP_EVENT_ON_DATA`, `HTTP_EVENT_ON_FINISH`, etc.
  - Previne timeouts do watchdog mesmo em transferências de arquivos grandes (>20KB)
- Reset do watchdog antes e depois de `esp_http_client_perform()`
- Uso de `esp_http_client_perform()` para gerenciamento automático e robusto de toda a transferência HTTP

### Thread Safety
- **Mutexes FreeRTOS** para proteger todas as operações de SD card
- Timeout de 5 segundos para aquisição de mutex (previne deadlocks)
- Macros `SDCARD_LOCK()` e `SDCARD_UNLOCK()` aplicadas consistentemente
- Proteção em todas as funções críticas de acesso ao sistema de arquivos

### Monitoramento e Recuperação
- **Watchdog timer** configurado para monitorar tasks críticas:
  - Task de captura visual
  - Task de captura térmica
  - Task de envio térmico
- Resets periódicos do watchdog em loops principais
- **Callback HTTP robusto:** Reseta watchdog em todos os eventos HTTP durante transferências
  - Monitora eventos de conexão, envio de headers, recebimento de dados e finalização
  - Previne timeouts mesmo em transferências longas (imagens grandes, múltiplos frames térmicos)
  - Logs de debug para monitoramento de eventos HTTP
- Delays longos divididos em múltiplos delays menores para evitar timeouts
- Reconexão automática em caso de falha de Wi-Fi

### Proteção de Memória
- **Alocação inteligente de memória:**
  - Tenta PSRAM primeiro (mais disponível) para buffers grandes
  - Fallback para RAM interna se PSRAM não disponível
  - Fallback para qualquer RAM disponível como último recurso
- **Alocação dinâmica** para buffers grandes (JSON de metadados)
- Validação de tamanho necessário antes de alocar
- Uso seguro de `snprintf` com limites de tamanho
- Verificação de falha de alocação antes de uso
- Limpeza adequada de recursos em caso de erro
- Logs informativos sobre qual tipo de memória foi utilizada

### Integridade de Dados
- **CRC32 checksums** em SPIFFS e durante migração
- **Read-after-write verification** em SD card
- Validação de tamanho após cada escrita
- Verificação de metadados antes de limpar SPIFFS
- Migração em chunks para reduzir uso de memória
- SPIFFS só é limpo após confirmação completa (dados + metadados)

### Qualidade de Código

O código foi desenvolvido seguindo práticas profissionais de engenharia de software embarcado:

- ✅ **Thread Safety:** Todas as operações críticas protegidas com mutexes
- ✅ **Watchdog Timer:** Tasks críticas monitoradas para prevenir travamentos
  - Callback HTTP robusto reseta watchdog em todos os eventos durante transferências longas
  - Resets periódicos em loops principais e antes/depois de operações longas
  - Previne timeouts mesmo quando servidor demora para responder (até 30s)
- ✅ **Proteção de Memória:** 
  - Alocação inteligente (PSRAM → RAM interna → RAM geral)
  - Validação de buffers e alocação dinâmica quando necessário
  - Logs informativos sobre tipo de memória utilizada
- ✅ **Tratamento de Erros:** 85+ pontos de validação e log no código
- ✅ **Recuperação Graciosa:** Sistema continua operando mesmo após falhas parciais
- ✅ **Logs Informativos:** Logs detalhados para diagnóstico e depuração
- ✅ **Arquitetura Modular:** Separação clara de responsabilidades (BSP/APP/GUI)

**Status:** ⭐⭐⭐⭐⭐ (5/5) - **Código Profissional e Pronto para Produção**

Veja `ANALISE_QUALIDADE_FINAL.md` para análise detalhada da qualidade do código.

---

## 🧩 Componentes ESP-IDF

Declarados em `main/CMakeLists.txt` e `main/idf_component.yml`:

### Componentes Principais

- `esp32-camera` – controle da câmera visual OV2640 (via IDF Component Manager)
- `led_strip` – controle de LED RGB WS2812 (via IDF Component Manager)
- `esp_wifi` – conexão Wi-Fi STA
- `esp_http_client` – envio HTTPS POST
- `esp_vfs_fat` / `sdmmc_cmd` – sistema de arquivos e SD Card
- `esp_spiffs` – sistema de arquivos SPIFFS (buffer térmico)
- `esp_sntp` – sincronização NTP para tempo real
- `driver/uart` – comunicação UART com câmera térmica MLX90640
- `FreeRTOS` – tarefas principais e controle do LED
- `nvs_flash` – armazenamento não-volátil

### Dependências Externas (IDF Component Manager)

```yaml
dependencies:
  espressif/esp32-camera: "^2.0.15"
  espressif/led_strip: "*"
```

---

## 🔋 Requisitos e Considerações

- ESP-IDF v5.0 ou superior  
- ESP32-S3 WROOM (N16R8) com câmera e slot SD integrados  
- Cartão SD formatado em FAT32  
- Wi-Fi 2.4 GHz ativo  
- Certificado SSL do servidor de destino  
- Alimentação adequada (recomendado 5V/2A para operação estável)  
- PSRAM habilitado e configurado (necessário para buffer de imagem)

**Nota:** A ESP32-S3 com câmera consome bastante energia durante a captura. Certifique-se de usar uma fonte de alimentação adequada.

---

## 📊 Estrutura de Dados

### Imagens Visuais

As imagens são enviadas como:
- **Content-Type:** `image/jpeg`
- **Método:** POST
- **Body:** Dados binários da imagem JPEG
- **Endpoint:** Configurável via `CAMERA_UPLOAD_URL` em `secrets.h` (padrão: `https://camera02.greense.com.br/upload`)

### Dados Térmicos

Os dados térmicos são enviados como:
- **Content-Type:** `application/json`
- **Método:** POST
- **Body:** JSON com array de temperaturas e timestamp
- **Endpoint:** Configurável via `CAMERA_THERMAL_UPLOAD_URL` em `secrets.h` (padrão: `https://camera02.greense.com.br/termica`)

**Formato JSON:**
```json
{
  "temperaturas": [25.5, 26.0, 26.5, ...],  // 768 valores float
  "timestamp": 1763809109                    // Unix timestamp
}
```

---

## 🔄 Lógica de Conexão Wi-Fi

O sistema utiliza uma lógica robusta de conexão Wi-Fi baseada no projeto N02:

- **Inicialização:** `bsp_wifi_init()` bloqueia até conectar (ou falhar após 5 tentativas)
- **Monitoramento:** Loop principal verifica conexão a cada 5 segundos
- **Reconexão:** Em caso de desconexão, tenta reconectar automaticamente
- **Sinalização:** LED RGB indica estado da conexão em tempo real

---

## 📸 Aplicação: Monitoramento de Estufa NFT

O sistema foi desenvolvido para monitoramento de estufas NFT (Nutrient Film Technique), capturando imagens visuais e dados térmicos para análise do ambiente de cultivo.

### Imagens da Estufa

#### Visão Visual
![Estufa NFT - Visão Visual](imagens/estufaNFTVisual.jpg)

A câmera visual OV2640 captura imagens da estufa com campo de visão de 120°, permitindo monitoramento amplo do ambiente de cultivo.

#### Visão Térmica
![Estufa NFT - Visão Térmica](imagens/estufaNFTTermica.jpg)

A câmera térmica MLX90640 captura dados de temperatura (24×32 pixels) que são processados e visualizados como mapas de calor, permitindo identificar variações térmicas no ambiente.

#### Visão Sobreposta (Térmica + Visual)
![Estufa NFT - Visão Sobreposta](imagens/EstufaNFTSobreposta.jpg)

A sobreposição de dados térmicos sobre a imagem visual permite análise combinada, identificando áreas específicas com problemas térmicos no contexto visual da estufa.

### Sequência Temporal de Dados Térmicos

![Sequência de Imagens Térmicas](imagens/imagensTermicas.png)

O sistema captura dados térmicos a cada 30 minutos (conforme agendamento configurado), permitindo análise temporal da evolução da temperatura na estufa ao longo do dia.

---

## 🌡️ Visualização de Dados Térmicos

O projeto inclui um script Python para visualizar os arquivos binários gerados pela câmera térmica:

### Instalação

```bash
pip install numpy matplotlib pillow
```

### Uso

```bash
# Visualizar um arquivo
python visualize_thermal.py THM46455.BIN

# Salvar imagens em um diretório
python visualize_thermal.py THM46455.BIN --output-dir ./output

# Criar animação GIF
python visualize_thermal.py THM46455.BIN --gif animation.gif

# Especificar número de frames
python visualize_thermal.py THM46455.BIN --frames 2
```

**Recursos:**
- Visualização térmica com mapas de cores (hot, jet, viridis, etc.)
- Estatísticas de temperatura (mínima, máxima, média)
- Geração de imagens PNG individuais
- Criação de animações GIF
- Processamento em lote de múltiplos arquivos

Veja `README_THERMAL.md` para mais detalhes.

## 📊 Logs e Monitoramento

O sistema gera logs informativos sobre o agendamento:

```
📸 Câmera Visual - Hora atual: 2025-11-21 14:25:20 | Próxima aquisição: 2025-11-21 14:30:00 (em 4 minutos e 40 segundos)
🌡️ Câmera Térmica - Hora atual: 2025-11-21 14:25:20 | Próxima aquisição: 2025-11-21 14:26:00 (em 40 segundos)
```

Os logs são exibidos periodicamente (a cada ~2.5 minutos) e mostram:
- Hora atual sincronizada via NTP
- Próximo horário de aquisição agendado
- Tempo de espera formatado de forma legível

## 🧪 Próximos Passos

- [x] Sincronização NTP para timestamps precisos  
- [x] Agendamento independente para câmera visual e térmica
- [x] Câmera térmica MLX90640 integrada
- [x] Logs informativos sobre próxima aquisição
- [x] Script Python para visualização de dados térmicos
- [x] **Sistema de acumulação térmica em SPIFFS**
- [x] **Migração automática para SD card com verificação**
- [x] **Envio imediato de dados térmicos após captura (sem duplicação ou reenvio)**
- [x] **Arquivamento automático no SD card (sem reenvio HTTP)**
- [x] **Verificação de integridade (checksums e read-after-write)**
- [x] **Preservação de histórico completo (todos os frames enviados)**
- [x] **Thread safety completo com mutexes**
- [x] **Watchdog timer para monitoramento de tasks**
- [x] **Callback HTTP para resetar watchdog durante transferências longas**
- [x] **Alocação inteligente de memória (PSRAM primeiro)**
- [x] **Proteção contra buffer overflow**
- [x] **Retry HTTP com backoff exponencial**
- [x] **Configurações centralizadas em secrets.h**
- [ ] Configuração via web interface  
- [ ] Compressão adicional de imagens  
- [ ] Detecção de movimento para captura sob demanda  
- [ ] Stream de vídeo em tempo real  
- [ ] Integração com sistema de monitoramento  
- [ ] Suporte a múltiplas resoluções de captura
- [ ] Dashboard web para visualização de dados térmicos

---

## 📄 Arquivos Adicionais

- **`visualize_thermal.py`** - Script Python para visualização de dados térmicos
- **`exemplo_uso_thermal.py`** - Exemplos de uso do script de visualização
- **`README_THERMAL.md`** - Documentação detalhada sobre visualização térmica
- **`ANALISE_QUALIDADE_FINAL.md`** - Análise completa da qualidade do código e robustez

## 📄 Licença

Licença **MIT**  
Desenvolvido por **Prof. Marcelino Monteiro de Andrade**  
**Universidade de Brasília (FCTE/UnB)**  
🌐 [https://github.com/marcelinoandrade/greense](https://github.com/marcelinoandrade/greense)

---

## 🔗 Projetos Relacionados

- **N04_Estufa_Camera_C** – Versão anterior para ESP32-CAM (AI Thinker)
- **N02_Estufa_Maturar_C** – Projeto base para lógica de Wi-Fi e LED

---

## 📝 Notas de Migração (N04 → N07)

Este projeto (N07) substitui o projeto N04 (ESP32-CAM) com as seguintes melhorias:

- ✅ **Hardware:** Migração de ESP32-CAM para ESP32-S3 WROOM
- ✅ **Arquitetura:** Modularização em camadas BSP/APP/GUI
- ✅ **LED:** Migração de LED GPIO simples para LED RGB WS2812
- ✅ **PSRAM:** Suporte nativo a PSRAM OCT SPI (8MB)
- ✅ **Flash:** Aumento de 4MB para 16MB
- ✅ **Câmera:** Câmera integrada (não módulo externo)
- ✅ **SD Card:** Slot integrado (não módulo externo)
- ✅ **Wi-Fi:** Lógica de reconexão melhorada
