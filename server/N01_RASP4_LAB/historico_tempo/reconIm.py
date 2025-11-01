import numpy as np
import matplotlib.pyplot as plt
import csv

arquivo = "termica_global.csv"

# Ler o CSV completo
with open(arquivo, "r") as f:
    reader = list(csv.reader(f))

header = reader[0]
dados = reader[1:]

# Extrai os nomes das colunas de temperatura (a partir da 3ª coluna)
colunas_temp = header[2:]

# Seleciona as 36 últimas colunas
ultimas_colunas = colunas_temp[-36:]
num_imagens = len(ultimas_colunas)

print(f"Total de {num_imagens} imagens para plotar.")

# Dimensões fixas da câmera térmica
n_linhas, n_colunas = 24, 32

# Cria uma grade 6x6 de subplots
fig, axs = plt.subplots(6, 6, figsize=(18, 18))

# "Achata" o array de eixos 2D (6x6) para um array 1D (36)
axs_flat = axs.flatten()

# Para cada uma das 36 últimas colunas, reconstruir matriz e exibir
for idx, nome_coluna in enumerate(ultimas_colunas):
    # Pega o eixo correto do array achatado
    ax = axs_flat[idx]
    
    # índice da coluna correspondente
    col_index = header.index(nome_coluna)
    
    # vetor com as temperaturas dessa captura
    vetor_temp = [float(linha[col_index]) for linha in dados]
    
    # converter para matriz 24x32
    matriz = np.array(vetor_temp).reshape((n_linhas, n_colunas))
    
    # --- ALTERAÇÃO AQUI ---
    # Rotaciona a matriz em 180 graus (k=2 -> 2 * 90 graus)
    matriz_rotacionada = np.rot90(matriz, k=2)
    # --- FIM DA ALTERAÇÃO ---

    # Usamos a paleta 'PRGn' e a interpolação como antes
    im = ax.imshow(matriz_rotacionada, cmap='PRGn', aspect="auto", interpolation='bilinear')
    
    ax.set_title(f"{nome_coluna}", fontsize=10)
    ax.set_xlabel("Coluna")
    ax.set_ylabel("Linha")
    
    plt.colorbar(im, ax=ax, fraction=0.046, pad=0.04)

# Desliga os eixos extras se houver menos de 36 imagens
if num_imagens < 36:
    for i in range(num_imagens, 36):
        axs_flat[i].axis('off')

plt.suptitle(f"Últimas {num_imagens} Imagens Térmicas (Rotacionadas 180°)")
plt.tight_layout(rect=[0, 0.03, 1, 0.97]) 
plt.show()