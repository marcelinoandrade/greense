import numpy as np
import matplotlib.pyplot as plt
import csv
import os
import cv2


# ========= AJUSTES =========
#ARQUIVO_CSV = "termica_global.csv"
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ARQUIVO_CSV = os.path.join(SCRIPT_DIR, "termica_global.csv")


# Quantas ÚLTIMAS colunas de imagem usar (ex.: 12, 24, 48...)
N_ULTIMAS = 60+5*(23*2)+7

# Dimensões da imagem térmica (matriz do sensor reconstruída)
IMG_LINHAS, IMG_COLUNAS = 24, 32

# Escala fixa (opcional). Se quiser auto, coloque VMIN = VMAX = None
VMIN, VMAX = 22, 28
# ===========================

def carregar_dados(arquivo_csv):
    if not os.path.exists(arquivo_csv):
        raise FileNotFoundError(f"Arquivo não encontrado: {arquivo_csv}")

    with open(arquivo_csv, "r", encoding="utf-8") as f:
        reader = list(csv.reader(f))

    if len(reader) < 2:
        raise ValueError("CSV vazio ou só com cabeçalho.")

    header = reader[0]
    dados = reader[1:]
    colunas_temp = header[2:]  # depois de timestamp e sensor

    if not colunas_temp:
        raise ValueError("Nenhuma coluna de temperatura no cabeçalho (header[2:]).")

    return header, dados, colunas_temp

def coluna_para_matriz(header, dados, nome_coluna, img_linhas, img_colunas):
    col_index = header.index(nome_coluna)
    vetor = [float(linha[col_index].replace(",", ".")) for linha in dados]

    esperado = img_linhas * img_colunas
    if len(vetor) != esperado:
        raise ValueError(f"Coluna '{nome_coluna}': tamanho {len(vetor)} != esperado {esperado}")

    return np.array(vetor, dtype=float).reshape((img_linhas, img_colunas))

# --- 1) Carrega CSV ---
header, dados, colunas_temp = carregar_dados(ARQUIVO_CSV)

# --- 2) Seleciona as N últimas colunas ---
n = min(N_ULTIMAS, len(colunas_temp))
ultimas_colunas = colunas_temp[-n:-n+3]

if n == 0:
    raise ValueError("Não há colunas de imagem para processar.")

print(f"Usando as {n} últimas colunas: {ultimas_colunas[0]} ... {ultimas_colunas[-1]}")

# --- 3) Converte cada coluna em matriz e calcula a média ---
matrizes = []
for nome in ultimas_colunas:
    matrizes.append(coluna_para_matriz(header, dados, nome, IMG_LINHAS, IMG_COLUNAS))

media = np.mean(np.stack(matrizes, axis=0), axis=0)

# --- AJUSTE DE ORIENTAÇÃO ---
media = np.rot90(media, k=-1)
media = np.fliplr(media)

# --- REDIMENSIONA PARA 1024x768 ---
media_1024x768 = cv2.resize(
    media,
    (1024, 768),
    interpolation=cv2.INTER_LINEAR
)

# --- CAMINHO DE SAÍDA ---
output_dir = "/home/greense/projetoGreense/server/N01_RASP4_LAB/fotos_recebidas/cam_01"
output_file = os.path.join(output_dir, "ultima.jpg")
os.makedirs(output_dir, exist_ok=True)

# --- SALVA A IMAGEM ---
plt.imsave(
    output_file,
    media_1024x768,
    cmap="inferno",
    vmin=VMIN,
    vmax=VMAX
)

print(f"Imagem salva em: {output_file}")

# --- EXIBE ---
plt.figure(figsize=(10.24, 7.68), dpi=100)
plt.imshow(
    media_1024x768,
    cmap="inferno",
    aspect="auto",
    interpolation="nearest",
    vmin=VMIN,
    vmax=VMAX
)
plt.colorbar()
plt.tight_layout()
plt.show()
