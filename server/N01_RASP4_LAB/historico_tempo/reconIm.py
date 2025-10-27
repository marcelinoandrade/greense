import csv
import numpy as np
import matplotlib.pyplot as plt

arquivo = "termica_global.csv"

# Ler o CSV completo
with open(arquivo, "r") as f:
    reader = list(csv.reader(f))

header = reader[0]
dados = reader[1:]

# Extrai os nomes das colunas de temperatura (a partir da 3ª coluna)
colunas_temp = header[2:]

# Seleciona as 4 últimas colunas
ultimas_colunas = colunas_temp[-4:] if len(colunas_temp) >= 4 else colunas_temp
print("Últimas colunas:", ultimas_colunas)

# Dimensões fixas da câmera térmica
n_linhas, n_colunas = 24, 32

fig, axs = plt.subplots(1, len(ultimas_colunas), figsize=(5 * len(ultimas_colunas), 6))

if len(ultimas_colunas) == 1:
    axs = [axs]

# Para cada uma das 4 últimas colunas, reconstruir matriz e exibir
for idx, nome_coluna in enumerate(ultimas_colunas):
    # índice da coluna correspondente
    col_index = header.index(nome_coluna)
    
    # vetor com as temperaturas dessa captura
    vetor_temp = [float(linha[col_index]) for linha in dados]
    
    # converter para matriz 24x32
    matriz = np.array(vetor_temp).reshape((n_linhas, n_colunas))
    
    im = axs[idx].imshow(matriz, cmap="hot", aspect="auto")
    axs[idx].set_title(f"{nome_coluna}")
    axs[idx].set_xlabel("Coluna")
    axs[idx].set_ylabel("Linha")
    plt.colorbar(im, ax=axs[idx], fraction=0.046, pad=0.04)

plt.suptitle("Quatro Últimas Imagens Térmicas")
plt.tight_layout()
plt.show()
