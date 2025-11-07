import numpy as np
import matplotlib.pyplot as plt
import csv

arquivo = "termica_global.csv"

# Ler CSV completo
with open(arquivo, "r") as f:
    reader = list(csv.reader(f))

header = reader[0]
dados = reader[1:]

# Colunas de temperatura (a partir da 3ª)
colunas_temp = header[2:]
ultimas_colunas = colunas_temp[-36:]
num_imagens = len(ultimas_colunas)
print(f"Total de {num_imagens} imagens para plotar.")

# Dimensões da câmera térmica
n_linhas, n_colunas = 24, 32

# Figura 6x6
fig, axs = plt.subplots(6, 6, figsize=(18, 18))
axs_flat = axs.flatten()

# Colormap colorido, saturando fora da faixa
cmap = plt.cm.inferno
cmap = cmap.reversed().reversed()
cmap.set_under('black')
cmap.set_over('white')

for idx, nome_coluna in enumerate(ultimas_colunas):
    ax = axs_flat[idx]

    col_index = header.index(nome_coluna)
    vetor_temp = [float(linha[col_index]) for linha in dados]

    # Converte para matriz
    matriz = np.array(vetor_temp).reshape((n_linhas, n_colunas))

    # Rotaciona 180°
    matriz_rotacionada = np.rot90(matriz, k=2)

    # Menos suavização (usar 'nearest')
    im = ax.imshow(
        matriz_rotacionada,
        cmap=cmap,
        aspect="auto",
        interpolation='nearest',
        vmin=20,
        vmax=40
    )

    ax.set_title(f"{nome_coluna}", fontsize=8)
    ax.set_xlabel("Coluna", fontsize=6)
    ax.set_ylabel("Linha", fontsize=6)
    ax.tick_params(labelsize=6)

    plt.colorbar(im, ax=ax, fraction=0.046, pad=0.04)

# Desliga eixos extras
if num_imagens < 36:
    for i in range(num_imagens, 36):
        axs_flat[i].axis('off')

plt.suptitle(f"Últimas {num_imagens} Imagens Térmicas (escala 20°C–40°C, colormap inferno)", fontsize=14)
plt.tight_layout(rect=[0, 0.03, 1, 0.98])
plt.show()
