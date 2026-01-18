# Configurações Ideais da Câmera para Detecção de Buracos de Tiro em Alvos

## Resumo Executivo

Este documento resume as configurações ideais da câmera **ESP32-P4-EYE com sensor OV2710** para o problema de **detecção de buracos de tiro de 9mm em alvos de 1 metro**, especialmente em **ambiente outdoor (estante a céu aberto)**.

---

## 🎯 Configurações Recomendadas

### **1. Resolução da Câmera**

```
RESOLUÇÃO: 1080P (1920x1080)
```

**Nota:** A resolução da câmera é o parâmetro **mais crítico** para detecção de buracos de 9mm.

**Justificativa:**
- **Alvos de 1 metro**: Requer alta resolução para detectar buracos de 9mm (~9.65mm de diâmetro)
- **1080P oferece**: ~1.9 pixels/mm em um alvo de 1m ocupando quase toda a tela
- **Suficiente para**: Detectar buracos de 9mm com boa margem para processamento AI
- **Recomendado para treinamento**: Maior qualidade de imagem = melhor dataset

**Alternativas:**
- **720P (1280x720)**: Aceitável, mas reduz precisão para alvos menores
- **480P (640x480)**: **NÃO RECOMENDADO** - resolução insuficiente

---

### **2. Parâmetros ISP (Image Signal Processor)**

#### **Ambiente: Outdoor (Estante a Céu Aberto)**

```c
CONTRAST (Contraste):   78%  // Alto contraste para destacar buracos
SATURATION (Saturação): 52%  // Reduzida (evita oversaturação com sol)
BRIGHTNESS (Brilho):    62%  // Aumentado (compensa sombras solares)
HUE (Matiz):            0%   // Neutro (ideal para condições naturais)
```

**Justificativa:**

| Parâmetro | Valor | Motivo |
|-----------|-------|--------|
| **Contraste** | **78%** | Alto contraste ajuda a destacar buracos escuros contra fundo claro do alvo |
| **Saturação** | **52%** | Reduzida para evitar cores saturadas demais com iluminação solar intensa |
| **Brilho** | **62%** | Aumentado para compensar variações de sombra e luz solar no outdoor |
| **Matiz** | **0%** | Neutro - ideal para condições de luz natural variável |

#### **Ambiente: Indoor (Tiro ao Alvo)**

```c
CONTRAST (Contraste):   65%  // Contraste moderado (iluminação controlada)
SATURATION (Saturação): 58%  // Saturação média (sem sol)
BRIGHTNESS (Brilho):    55%  // Brilho normal (iluminação artificial)
HUE (Matiz):            0%   // Neutro
```

---

### **3. Tempo do Obturador (Shutter Speed / Exposure Time)**

```
OUTDOOR (Dia ensolarado):    1/1000s a 1/2000s (Automático recomendado)
OUTDOOR (Nublado):            1/500s a 1/1000s (Automático recomendado)
INDOOR (Iluminação artificial): 1/250s a 1/500s (Automático recomendado)
```

**Justificativa:**

| Ambiente | Tempo de Exposição | Motivo |
|----------|-------------------|--------|
| **Outdoor (Dia ensolarado)** | **1/1000s - 1/2000s** | Luz solar intensa requer exposição rápida para evitar superexposição |
| **Outdoor (Nublado)** | **1/500s - 1/1000s** | Luz reduzida permite exposição um pouco mais longa |
| **Indoor (Iluminação artificial)** | **1/250s - 1/500s** | Iluminação artificial geralmente requer exposição mais longa |

**Recomendações:**
- **Modo Automático (AUTO_EXPOSURE)**: Recomendado para a maioria dos casos
  - A câmera ajusta automaticamente conforme condições de iluminação
  - Evita imagens superexpostas ou subexpostas
  - Melhor para dataset diversificado (diferentes horários do dia)
  
- **Modo Manual**: Apenas se necessário para condições muito específicas
  - **Muito sol (meio-dia)**: 1/2000s ou mais rápido
  - **Sol moderado (manhã/tarde)**: 1/1000s a 1/1500s
  - **Nublado**: 1/500s a 1/1000s
  - **Indoor**: 1/250s a 1/500s

**Impacto no Problema de Alvo:**
- **Exposição muito rápida (< 1/2000s)**: Pode escurecer demais a imagem, dificultando ver buracos
- **Exposição muito lenta (> 1/250s)**: Pode causar blur se houver movimento e superexposição em luz forte
- **Exposição ideal**: Balanceada para capturar detalhes dos buracos sem perder contraste

**Nota Técnica:**
- O sensor OV2710 suporta **exposição automática (AE)** via configuração IPA (Image Processing Algorithm)
- A configuração é feita via arquivo JSON (`ov2710_custom.json`) ou via API V4L2
- Para detecção de alvos estáticos, **exposição automática é geralmente a melhor opção**

---

### **4. Iluminação e Flash**

```
FLASH: DESLIGADO (false)
```

**Justificativa:**
- **Outdoor**: Iluminação solar natural é suficiente
- **Indoor**: Iluminação artificial controlada é preferível
- **Flash pode**: Criar reflexos, sombras não desejadas, variações de brilho

**Recomendações:**
- Usar iluminação uniforme e consistente
- Evitar sombras fortes ou contrastes extremos
- Manter mesma iluminação durante todo o dataset

---

## 📐 Análise de Resolução vs. Tamanho do Alvo

### **Para Alvo de 1 Metro (1000mm)**

| Resolução | Pixels por Metro | Pixels por mm | Detectável (9mm) |
|-----------|------------------|---------------|------------------|
| **1080P** | **1920 px/m** | **1.92 px/mm** | **~17 pixels** ✅ |
| 720P | 1280 px/m | 1.28 px/mm | ~11 pixels ⚠️ |
| 480P | 640 px/m | 0.64 px/mm | ~6 pixels ❌ |

**Conclusão:** 1080P é **ideal e necessário** para detectar buracos de 9mm com confiança.

---

## 🤖 Configurações para Treinamento AI

### **Modelo de Detecção Recomendado**

```
MODELO: YOLO11n (YOLOv11 Nano)
RESOLUÇÃO DE ENTRADA: 640x640 ou 960x960
RESOLUÇÃO DE CAPTURA: 1080P (downscaled para treinamento)
```

**Justificativa:**
- **YOLO11n**: Modelo leve o suficiente para ESP32-P4 em tempo real
- **640x640**: Latência ~50-80ms no ESP32-P4 (12-20 FPS)
- **960x960**: Latência ~100-150ms no ESP32-P4 (6-10 FPS) - **recomendado para maior precisão**
- **1080P captura**: Permite downscale mantendo qualidade para treinamento

### **Processamento no ESP32-P4**

| Capacidade | Valor | Suficiente? |
|------------|-------|-------------|
| **CPU** | 2x RISC-V 400MHz + 1x 20MHz | ✅ Sim |
| **PSRAM** | 8MB externo | ✅ Sim |
| **L2 Cache** | 128KB | ✅ Sim |
| **FPS Estimado (640x640)** | 12-20 FPS | ✅ Aceitável |
| **FPS Estimado (960x960)** | 6-10 FPS | ⚠️ Limite |

**Recomendação:** Usar **640x640** para detecção em tempo real, **960x960** se precisar maior precisão e aceitar latência maior.

---

## 📸 Configurações para Captura de Dataset

### **Fase 1: Aquisição de Imagens (Treinamento)**

```
✓ Resolução: 1080P (1920x1080)
✓ ISP: Configurações otimizadas para outdoor (acima)
✓ Flash: Desligado
✓ Formato: JPEG
✓ Storage: SD Card
✓ Nomenclatura: Sequencial (IMG_0001.jpg, IMG_0002.jpg, ...)
```

### **Fase 2: Anotação e Treinamento**

- **Ferramentas**: LabelImg, Roboflow, CVAT
- **Formato de anotação**: COCO JSON, YOLO TXT
- **Classes**: `bullet_hole` (ou `shot`, `hole`)
- **Tamanho do dataset**: Mínimo 500 imagens, ideal 1000+ com variação

### **Fase 3: Deploy e Inferência**

```
✓ Modelo: YOLO11n quantizado (INT8)
✓ Resolução de inferência: 640x640 ou 960x960
✓ FPS esperado: 6-20 FPS (dependendo da resolução)
✓ Pós-processamento: NMS (Non-Maximum Suppression)
```

---

## 🎛️ Parâmetros Ajustáveis Durante Captura

### **Via Encoder Rotativo (se implementado)**

1. **Contraste** (0-100%): Ajustar para destacar buracos
2. **Saturação** (0-100%): Reduzir em ambiente muito colorido
3. **Brilho** (0-100%): Aumentar em sombras, reduzir em luz excessiva
4. **Matiz** (0-100%): Geralmente manter em 0% (neutro)

**Passo recomendado:** 5% por incremento

---

## ✅ Checklist de Configuração

- [ ] **Resolução**: Configurada para 1080P
- [ ] **Tempo do Obturador**: Automático (recomendado) ou manual conforme ambiente
  - [ ] Outdoor (sol): 1/1000s - 1/2000s
  - [ ] Outdoor (nublado): 1/500s - 1/1000s
  - [ ] Indoor: 1/250s - 1/500s
- [ ] **Contraste**: 78% (outdoor) ou 65% (indoor)
- [ ] **Saturação**: 52% (outdoor) ou 58% (indoor)
- [ ] **Brilho**: 62% (outdoor) ou 55% (indoor)
- [ ] **Matiz**: 0% (neutro)
- [ ] **Flash**: Desligado
- [ ] **Foco**: Ajustado manualmente para distância do alvo
- [ ] **Iluminação**: Uniforme e consistente
- [ ] **Alvo ocupa**: Quase toda a tela (zoom/posicionamento)
- [ ] **Storage**: SD Card formatado e montado

---

## 🔧 Código de Referência (Configuração Base)

```c
// Configurações ISP para outdoor
#define CAPTURE_RESOLUTION PHOTO_RESOLUTION_1080P
#define CAPTURE_CONTRAST    78  // Alto contraste
#define CAPTURE_SATURATION  52  // Reduzida (sol)
#define CAPTURE_BRIGHTNESS  62  // Aumentado (sombras)
#define CAPTURE_HUE         0   // Neutro
#define CAPTURE_FLASH       false

// Nota: Tempo do obturador é configurado via:
// 1. IPA JSON (ov2710_custom.json) - exposição automática recomendada
// 2. Ou via V4L2_CID_EXPOSURE_ABSOLUTE (se disponível)

// Aplicar configurações
app_video_stream_set_photo_resolution(CAPTURE_RESOLUTION);
app_isp_set_contrast(CAPTURE_CONTRAST);
app_isp_set_saturation(CAPTURE_SATURATION);
app_isp_set_brightness(CAPTURE_BRIGHTNESS);
app_isp_set_hue(CAPTURE_HUE);
bsp_flashlight_set(CAPTURE_FLASH);
```

---

## 📊 Resumo Final

| Configuração | Valor Ideal | Impacto |
|--------------|-------------|---------|
| **Resolução** | **1080P** | **CRÍTICO** - Determina capacidade de detecção |
| **Tempo do Obturador** | **Auto (1/500s - 1/2000s)** | **CRÍTICO** - Afeta qualidade geral da imagem |
| **Contraste** | **78%** | **ALTO** - Melhora visibilidade dos buracos |
| **Saturação** | **52%** | **MÉDIO** - Evita oversaturação |
| **Brilho** | **62%** | **ALTO** - Compensa sombras outdoor |
| **Matiz** | **0%** | **BAIXO** - Geralmente neutro |
| **Flash** | **OFF** | **MÉDIO** - Evita reflexos |

**Prioridade de Ajuste:**
1. **Resolução** (mais crítico)
2. **Tempo do Obturador** (crítico - usar auto geralmente é melhor)
3. **Contraste** (destaca buracos)
4. **Brilho** (compensa iluminação)
5. **Saturação** (ajuste fino)

---

## 📝 Notas Finais

- Essas configurações são **iniciais** e devem ser **ajustadas** conforme:
  - Condições de iluminação específicas
  - Tipo de alvo (cor, material)
  - Distância da câmera ao alvo
  - Condições climáticas (sol, nuvens, horário do dia)

- **Recomendação**: Capturar algumas imagens de teste, revisar qualidade visual, e ajustar parâmetros ISP conforme necessário antes de iniciar captura massiva do dataset.

- **Dataset diversificado**: Varie condições de iluminação, ângulos (se possível), e tipos de alvos para treinamento mais robusto.

---

**Documento gerado para:** Projeto de detecção de tiros em alvos  
**Câmera:** ESP32-P4-EYE (OV2710)  
**Alvos:** 1 metro, buracos de 9mm  
**Ambiente:** Outdoor (estante a céu aberto)
