# N04 · Câmera Visual · ESP32-CAM (ESP-IDF)

Sistema embarcado em **C (ESP-IDF)** para captura de imagens usando **ESP32-CAM (AI Thinker)** e envio automático via **HTTPS POST** para servidor remoto, com armazenamento local em cartão SD.

![ESP32-CAM](esp32_cam.png)

---

## Descrição Geral

Firmware executado em **ESP32-CAM (AI Thinker)**, capturando imagens JPEG periodicamente e enviando via HTTPS para endpoint configurável. Imagens também são salvas localmente em cartão SD para backup.

### Recursos Principais

- Captura de imagens JPEG (XGA - 1024×768)
- Armazenamento local em cartão SD
- Conexão Wi-Fi com reconexão automática
- Envio seguro via HTTPS com certificado SSL
- Sinalização por LED para indicar estado do Wi-Fi
- Flash LED para iluminação durante captura

---

## Hardware Utilizado

| Componente | Função | Interface |
|-------------|---------|-----------|
| **ESP32-CAM (AI Thinker)** | Microcontrolador + Câmera OV2640 | USB-C, Wi-Fi, GPIO |
| **Cartão SD** | Armazenamento local | SDMMC |
| **LED GPIO 33** | Indicador de status Wi-Fi | Digital |
| **Flash LED GPIO 4** | Iluminação para fotos | Digital |

### Pinos da ESP32-CAM

| Função | GPIO | Descrição |
|--------|------|-----------|
| PWDN | 32 | Power Down |
| XCLK | 0 | Clock da câmera |
| SIOD | 26 | I2C Data |
| SIOC | 27 | I2C Clock |
| Y9 | 35 | Dados da câmera |
| Y8 | 34 | Dados da câmera |
| Y7 | 39 | Dados da câmera |
| Y6 | 36 | Dados da câmera |
| Y5 | 21 | Dados da câmera |
| Y4 | 19 | Dados da câmera |
| Y3 | 18 | Dados da câmera |
| Y2 | 5 | Dados da câmera |
| VSYNC | 25 | Sincronização vertical |
| HREF | 23 | Horizontal Reference |
| PCLK | 22 | Pixel Clock |
| Flash LED | 4 | LED de iluminação |
| LED Wi-Fi | 33 | Indicador de status |

---

## Arquitetura de Software

```
main.c
├── Inicialização de NVS e Wi-Fi (STA)
├── Inicialização do cartão SD
├── Inicialização da câmera (OV2640)
├── Task periódica de captura
│   ├── Verificação de conexão Wi-Fi
│   ├── Ativação do flash LED
│   ├── Captura de imagem JPEG
│   ├── Envio HTTPS POST
│   ├── Salvamento no SD Card
│   └── Liberação do buffer
└── Reconexão automática em falhas
```

---

## Configuração

Defina as credenciais Wi-Fi e o endpoint no arquivo `secrets.h`:

```c
#define WIFI_SSID "sua_rede"
#define WIFI_PASS "sua_senha"
#define CAMERA_UPLOAD_URL "https://seu-servidor.com/upload"
```

**Importante**: O certificado SSL do servidor deve estar em `main/certs/greense_cert.pem` (ou ajuste o caminho no `CMakeLists.txt`).

### Parâmetros de Captura

Configurados em `main.c`:

- **Intervalo entre capturas**: 60 segundos (60000 ms)
- **Resolução**: XGA (1024×768)
- **Qualidade JPEG**: 12 (0-63, menor = melhor qualidade)
- **Formato**: JPEG

---

## Compilação e Execução

1. Instale o **ESP-IDF v5.0+**
2. Configure o certificado SSL:
   ```bash
   mkdir -p main/certs
   # Copie o certificado do servidor para main/certs/greense_cert.pem
   ```
3. Compile e grave na placa:
   ```bash
   idf.py build
   idf.py flash -b 921600
   idf.py monitor
   ```

---

## Sinalização por LED

O LED Wi-Fi (GPIO 33) indica o estado da conexão:

| Estado | Indicação | Descrição |
|---------|-----------|-----------|
| Sem Wi-Fi | LED aceso | Tentando conectar à rede Wi-Fi |
| Wi-Fi Conectado | LED apagado | Conectado à rede com IP válido |

O Flash LED (GPIO 4) é ativado durante a captura de imagem para iluminação.

---

## Armazenamento no SD Card

As imagens são salvas no cartão SD com o seguinte formato de nome:

- **Com NTP sincronizado**: `YYYYMMDD_HHMMSS.jpg`
- **Sem NTP**: `img_TIMESTAMP.jpg`

O cartão SD é montado em `/sdcard` e deve ser formatado em FAT32.

**Configuração do SD Card**:

- Modo: 1-bit (pode ser alterado para 4-bit se suportado)
- Pull-ups internos habilitados
- Sistema de arquivos FAT

---

## Segurança

O sistema utiliza **HTTPS** para envio seguro das imagens:

- Certificado SSL embutido no firmware
- Validação do certificado do servidor
- Timeout de 10 segundos para requisições

---

## Componentes ESP-IDF

Declarados em `main/CMakeLists.txt`:

```
idf_component_register(
  SRCS "main.c"
  INCLUDE_DIRS "."
  REQUIRES esp32-camera esp_http_server esp_wifi nvs_flash 
           esp_http_client driver fatfs sdmmc
  EMBED_TXTFILES "certs/greense_cert.pem"
)
```

### Principais Bibliotecas Usadas

- `esp_camera.h` – controle da câmera OV2640
- `esp_wifi.h` – conexão Wi-Fi STA
- `esp_http_client.h` – envio HTTPS POST
- `esp_vfs_fat.h` / `sdmmc_cmd.h` – sistema de arquivos e SD Card
- `FreeRTOS` – tarefas principais e controle do LED

---

## Requisitos e Considerações

- ESP-IDF v5.0 ou superior
- ESP32-CAM (AI Thinker) com câmera OV2640
- Cartão SD formatado em FAT32
- Wi-Fi 2.4 GHz ativo
- Certificado SSL do servidor de destino
- Alimentação adequada (recomendado 5V/2A para operação estável)

**Nota**: A ESP32-CAM consome bastante energia durante a captura. Certifique-se de usar uma fonte de alimentação adequada.

---

## Estrutura de Dados

As imagens são enviadas como:

- **Content-Type**: `image/jpeg`
- **Método**: POST
- **Body**: Dados binários da imagem JPEG

---

## Próximos Passos

- [ ] Sincronização NTP para timestamps precisos
- [ ] Configuração via web interface
- [ ] Compressão adicional de imagens
- [ ] Detecção de movimento para captura sob demanda
- [ ] Stream de vídeo em tempo real
- [ ] Integração com sistema de monitoramento

---

## Licença

Licença **MIT**  
Desenvolvido por **Prof. Marcelino Monteiro de Andrade**  
**Universidade de Brasília (FCTE/UnB)**  
[https://github.com/marcelinoandrade/greense](https://github.com/marcelinoandrade/greense)
