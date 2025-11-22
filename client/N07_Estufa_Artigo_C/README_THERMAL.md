# Visualização de Dados Térmicos

Script Python para visualizar os arquivos binários gerados pela câmera térmica MLX90640.

## Instalação

```bash
pip install numpy matplotlib pillow
```

## Uso Básico

### Listar Arquivos no Cartão SD

```bash
# Listar todos os arquivos térmicos no diretório atual
python visualize_thermal.py --list

# Listar arquivos em diretório específico (ex: cartão SD montado)
python visualize_thermal.py --list --directory /media/sdcard

# Listar e processar todos os arquivos encontrados
python visualize_thermal.py --list --process-all --output-dir ./output
```

### Visualizar Arquivo Específico

```bash
# Visualizar um arquivo (exibe as imagens)
python visualize_thermal.py THM12345.BIN

# Ou arquivos do ESP32 (THERML.BIN, THERMS.BIN)
python visualize_thermal.py THERML.BIN

# Salvar imagens em um diretório
python visualize_thermal.py THM12345.BIN --output-dir ./output

# Especificar número de frames (se diferente do padrão)
python visualize_thermal.py THM12345.BIN --frames 2

# Criar animação GIF
python visualize_thermal.py THM12345.BIN --gif animation.gif

# Usar diferentes mapas de cores
python visualize_thermal.py THM12345.BIN --colormap viridis
```

## Formato dos Dados

- **Câmera**: MLX90640
- **Resolução**: 24 linhas × 32 colunas = 768 pixels
- **Formato**: Floats (4 bytes cada)
- **Estrutura**: `THERMAL_SAVE_INTERVAL` frames consecutivos
- **Tamanho do arquivo**: `THERMAL_SAVE_INTERVAL × 768 × 4 bytes`

### Arquivos no Cartão SD

O ESP32 salva os arquivos térmicos no cartão SD com os seguintes nomes:

- **THERML.BIN** - Arquivo acumulativo local (dados térmicos pendentes de envio)
- **THERMLM.TXT** - Arquivo de metadados local (timestamps em JSON)
- **THERMS.BIN** - Arquivo renomeado após envio completo (histórico)
- **THERMSM.TXT** - Metadados renomeado após envio completo

O script detecta automaticamente esses arquivos e seus metadados correspondentes.

## Exemplos

### Listar arquivos no cartão SD
```bash
# Montar cartão SD (exemplo no Linux)
sudo mount /dev/sdb1 /media/sdcard

# Listar arquivos térmicos
python visualize_thermal.py --list --directory /media/sdcard
```

### Processar todos os arquivos do cartão SD
```bash
# Processa todos os arquivos encontrados e salva em ./output
python visualize_thermal.py --list --directory /media/sdcard --process-all --output-dir ./output
```

### Visualizar e salvar todas as imagens
```bash
python visualize_thermal.py THM12345.BIN --output-dir ./thermal_images
```

### Criar animação de todos os frames
```bash
python visualize_thermal.py THM12345.BIN --gif thermal_animation.gif --frames 2
```

### Processar múltiplos arquivos (método antigo)
```bash
for file in THM*.BIN; do
    python visualize_thermal.py "$file" --output-dir ./output --no-display
done
```

## Mapas de Cores Disponíveis

- `hot` (padrão) - Preto → Vermelho → Amarelo → Branco
- `jet` - Azul → Verde → Amarelo → Vermelho
- `viridis` - Roxo → Azul → Verde → Amarelo
- `plasma` - Roxo → Rosa → Amarelo
- `inferno` - Preto → Roxo → Amarelo
- `magma` - Preto → Roxo → Branco
- `coolwarm` - Azul (frio) → Branco → Vermelho (quente)

## Saída

O script gera:
- Imagens PNG individuais para cada frame
- Estatísticas de temperatura (mínima, máxima, média)
- Opcionalmente, uma animação GIF

Cada imagem inclui:
- Visualização térmica com mapa de cores
- Barra de cores com escala em °C
- Informações de temperatura no rodapé

