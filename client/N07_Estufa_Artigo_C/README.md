# 📷 Câmera ESP32-S3 com Envio Automático (ESP-IDF)

Sistema embarcado em **C (ESP-IDF)** para captura de imagens usando **ESP32-S3 WROOM (N16R8)** com câmera e slot SD integrados, e envio automático via **HTTPS POST** para um servidor remoto, com armazenamento local em cartão SD.

![ESP32-S3](esp32s3.jpg)

---

## ⚙️ Descrição Geral

O firmware executa em uma placa **ESP32-S3 WROOM (N16R8)** com câmera e slot SD integrados, capturando imagens JPEG periodicamente e enviando-as via HTTPS para um endpoint configurável. As imagens também são salvas localmente em um cartão SD para backup.

O sistema realiza:
- 📸 Captura de imagens JPEG (XGA - 1024×768)  
- 💾 Armazenamento local em cartão SD  
- 🌐 Conexão Wi-Fi com reconexão automática  
- 🔒 Envio seguro via HTTPS com certificado SSL  
- 💡 Sinalização por LED RGB (WS2812) para indicar estado do Wi-Fi  
- ⚡ Flash LED para iluminação durante captura  
- 🧠 Arquitetura modular (BSP/APP/GUI)

---

## 🧩 Hardware Utilizado

| Componente | Função | Interface |
|-------------|---------|-----------|
| **ESP32-S3 WROOM (N16R8)** | Microcontrolador + Câmera integrada | USB-C, Wi-Fi, GPIO |
| **Cartão SD** | Armazenamento local | SDMMC (slot integrado) |
| **LED RGB WS2812 (GPIO48)** | Indicador de status Wi-Fi | Digital (SPI/RMT) |
| **Flash LED (GPIO21)** | Iluminação para fotos | Digital |

### Especificações da Placa

- **Chip:** ESP32-S3 (Dual-core Xtensa LX7, 240MHz)
- **Flash:** 16MB (N16R8)
- **PSRAM:** 8MB (OCT SPI PSRAM)
- **Câmera:** Integrada (OV2640 ou similar)
- **SD Card:** Slot integrado (SDMMC)

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

---

## 🧠 Arquitetura de Software

O projeto utiliza uma arquitetura modular em três camadas:

```
main/
├── app/                        # Camada de Aplicação
│   ├── app_main.c             # Lógica principal e task periódica
│   └── app_http.c             # Cliente HTTPS para envio de imagens
│
├── bsp/                        # Board Support Package (Hardware)
│   ├── bsp_pins.h             # Definição de pinagem
│   ├── bsp_gpio.c/.h          # Controle de GPIO
│   ├── bsp_wifi.c/.h          # Configuração Wi-Fi
│   ├── bsp_camera.c/.h        # Inicialização da câmera
│   └── bsp_sdcard.c/.h        # Sistema de arquivos SD Card
│
└── gui/                        # Interface Gráfica/Usuário
    └── gui_led.c/.h           # Controle de LED RGB (WS2812)
```

### Fluxo de Execução

```
app_main()
├── Inicialização de NVS
├── Inicialização do Wi-Fi (STA) - bloqueia até conectar
├── Inicialização do cartão SD
├── Inicialização da câmera (OV2640)
├── Inicialização do LED RGB (WS2812)
├── Task periódica de captura
│   ├── Verificação de conexão Wi-Fi
│   ├── Ativação do flash LED
│   ├── Captura de imagem JPEG
│   ├── Envio HTTPS POST
│   ├── Salvamento no SD Card
│   └── Liberação do buffer
└── Loop principal (monitoramento Wi-Fi e reconexão)
```

---

## ⚙️ Configuração

Defina as credenciais Wi-Fi e o endpoint no arquivo `main/secrets.h`:

```c
#define WIFI_SSID "sua_rede"
#define WIFI_PASS "sua_senha"
#define CAMERA_UPLOAD_URL "https://seu-servidor.com/upload"
```

**Importante:** O certificado SSL do servidor deve estar em `main/certs/greense_cert.pem` (ou ajuste o caminho no `CMakeLists.txt`).

### Parâmetros de Captura

Configurados em `main/bsp/bsp_camera.c`:
- **Intervalo entre capturas:** 60 segundos (60000 ms) - configurável em `app_main.c`
- **Resolução:** XGA (1024×768)
- **Qualidade JPEG:** 12 (0-63, menor = melhor qualidade)
- **Formato:** JPEG
- **Frame Size:** PSRAM (buffer alocado em PSRAM)

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

As imagens são salvas no cartão SD com o seguinte formato de nome:
- **Com NTP sincronizado:** `YYYYMMDD_HHMMSS.jpg`
- **Sem NTP:** `img_TIMESTAMP.jpg`

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
| **SPIFFS** | data | spiffs | 12MB | Sistema de arquivos para dados e logs |

A tabela está definida em `partitions.csv`.

---

## 🔒 Segurança

O sistema utiliza **HTTPS** para envio seguro das imagens:
- Certificado SSL embutido no firmware
- Validação do certificado do servidor
- Timeout de 10 segundos para requisições
- Reconexão automática em caso de falha

---

## 🧩 Componentes ESP-IDF

Declarados em `main/CMakeLists.txt` e `main/idf_component.yml`:

### Componentes Principais

- `esp32-camera` – controle da câmera OV2640 (via IDF Component Manager)
- `led_strip` – controle de LED RGB WS2812 (via IDF Component Manager)
- `esp_wifi` – conexão Wi-Fi STA
- `esp_http_client` – envio HTTPS POST
- `esp_vfs_fat` / `sdmmc_cmd` – sistema de arquivos e SD Card
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

As imagens são enviadas como:
- **Content-Type:** `image/jpeg`
- **Método:** POST
- **Body:** Dados binários da imagem JPEG
- **Endpoint:** Configurável via `CAMERA_UPLOAD_URL` em `secrets.h`

---

## 🔄 Lógica de Conexão Wi-Fi

O sistema utiliza uma lógica robusta de conexão Wi-Fi baseada no projeto N02:

- **Inicialização:** `bsp_wifi_init()` bloqueia até conectar (ou falhar após 5 tentativas)
- **Monitoramento:** Loop principal verifica conexão a cada 5 segundos
- **Reconexão:** Em caso de desconexão, tenta reconectar automaticamente
- **Sinalização:** LED RGB indica estado da conexão em tempo real

---

## 🧪 Próximos Passos

- [x] Sincronização NTP para timestamps precisos  
- [ ] Configuração via web interface  
- [ ] Compressão adicional de imagens  
- [ ] Detecção de movimento para captura sob demanda  
- [ ] Stream de vídeo em tempo real  
- [ ] Integração com sistema de monitoramento  
- [ ] Suporte a múltiplas resoluções de captura

---

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
