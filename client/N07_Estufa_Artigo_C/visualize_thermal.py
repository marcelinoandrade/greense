#!/usr/bin/env python3
"""
Script para visualizar dados térmicos salvos pela câmera MLX90640
Arquivo binário: THM#####.BIN
Formato: THERMAL_SAVE_INTERVAL frames de 24x32 pixels (floats)
"""

import struct
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from pathlib import Path
import argparse
import os

# Dimensões da câmera térmica MLX90640
THERMAL_ROWS = 24
THERMAL_COLS = 32
THERMAL_TOTAL = THERMAL_ROWS * THERMAL_COLS  # 768 pixels

# Tamanho de um float em bytes
FLOAT_SIZE = 4


def read_thermal_file(filepath, thermal_save_interval=2):
    """
    Lê arquivo binário de dados térmicos
    
    Args:
        filepath: Caminho para o arquivo .BIN
        thermal_save_interval: Número de frames acumulados no arquivo
    
    Returns:
        Lista de arrays numpy com shape (24, 32) - um para cada frame
    """
    with open(filepath, 'rb') as f:
        data = f.read()
    
    # Verifica tamanho do arquivo
    expected_size = thermal_save_interval * THERMAL_TOTAL * FLOAT_SIZE
    if len(data) != expected_size:
        print(f"⚠️ Aviso: Tamanho do arquivo ({len(data)} bytes) não corresponde ao esperado ({expected_size} bytes)")
        print(f"   Tentando ajustar THERMAL_SAVE_INTERVAL...")
        # Tenta calcular o número de frames
        thermal_save_interval = len(data) // (THERMAL_TOTAL * FLOAT_SIZE)
        print(f"   Calculado: {thermal_save_interval} frames")
    
    # Converte bytes para floats
    num_floats = len(data) // FLOAT_SIZE
    floats = struct.unpack(f'{num_floats}f', data)
    
    # Reorganiza em frames
    frames = []
    for i in range(thermal_save_interval):
        start_idx = i * THERMAL_TOTAL
        end_idx = start_idx + THERMAL_TOTAL
        
        if end_idx > len(floats):
            break
            
        # Extrai os valores do frame
        frame_data = floats[start_idx:end_idx]
        
        # Converte para array numpy e reorganiza em matriz 24x32
        frame_array = np.array(frame_data).reshape(THERMAL_ROWS, THERMAL_COLS)
        frames.append(frame_array)
    
    return frames


def visualize_thermal_frame(frame, title="Imagem Térmica", save_path=None, colormap='hot'):
    """
    Visualiza um frame térmico
    
    Args:
        frame: Array numpy com shape (24, 32)
        title: Título da imagem
        save_path: Caminho para salvar a imagem (opcional)
        colormap: Mapa de cores (hot, jet, viridis, etc.)
    """
    fig, ax = plt.subplots(figsize=(10, 8))
    
    # Cria a imagem térmica
    im = ax.imshow(frame, cmap=colormap, interpolation='bilinear', aspect='auto')
    
    # Adiciona barra de cores
    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label('Temperatura (°C)', rotation=270, labelpad=20)
    
    # Configurações do gráfico
    ax.set_title(title, fontsize=14, fontweight='bold')
    ax.set_xlabel('Coluna (32 pixels)')
    ax.set_ylabel('Linha (24 pixels)')
    
    # Adiciona valores de temperatura mínima, máxima e média
    temp_min = np.min(frame)
    temp_max = np.max(frame)
    temp_avg = np.mean(frame)
    
    info_text = f'Min: {temp_min:.2f}°C | Max: {temp_max:.2f}°C | Média: {temp_avg:.2f}°C'
    ax.text(0.5, -0.1, info_text, transform=ax.transAxes, 
            ha='center', fontsize=10, bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    
    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches='tight')
        print(f"✅ Imagem salva: {save_path}")
    else:
        plt.show()
    
    plt.close()


def visualize_all_frames(frames, output_dir=None, base_filename="thermal_frame"):
    """
    Visualiza todos os frames de um arquivo
    
    Args:
        frames: Lista de arrays numpy
        output_dir: Diretório para salvar as imagens (opcional)
        base_filename: Nome base para os arquivos
    """
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
    
    for i, frame in enumerate(frames):
        title = f"Frame Térmico #{i+1}"
        save_path = None
        
        if output_dir:
            save_path = os.path.join(output_dir, f"{base_filename}_{i+1:02d}.png")
        
        visualize_thermal_frame(frame, title=title, save_path=save_path)
        print(f"📊 Frame {i+1}/{len(frames)} processado")


def create_animation(frames, output_path, fps=1):
    """
    Cria uma animação GIF dos frames térmicos
    
    Args:
        frames: Lista de arrays numpy
        output_path: Caminho para salvar o GIF
        fps: Frames por segundo
    """
    try:
        from matplotlib.animation import FuncAnimation
    except ImportError:
        print("❌ Erro: matplotlib.animation não disponível. Instale matplotlib completo.")
        return
    
    fig, ax = plt.subplots(figsize=(10, 8))
    
    # Inicializa a imagem
    im = ax.imshow(frames[0], cmap='hot', interpolation='bilinear', aspect='auto')
    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label('Temperatura (°C)', rotation=270, labelpad=20)
    
    ax.set_title('Animação Térmica', fontsize=14, fontweight='bold')
    ax.set_xlabel('Coluna (32 pixels)')
    ax.set_ylabel('Linha (24 pixels)')
    
    def update(frame):
        im.set_array(frame)
        temp_min = np.min(frame)
        temp_max = np.max(frame)
        temp_avg = np.mean(frame)
        ax.set_title(f'Frame | Min: {temp_min:.1f}°C | Max: {temp_max:.1f}°C | Média: {temp_avg:.1f}°C')
        return [im]
    
    anim = FuncAnimation(fig, update, frames=frames, interval=1000/fps, blit=True, repeat=True)
    anim.save(output_path, writer='pillow', fps=fps)
    print(f"✅ Animação salva: {output_path}")
    plt.close()


def main():
    parser = argparse.ArgumentParser(
        description='Visualiza dados térmicos da câmera MLX90640',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Exemplos:
  # Visualizar um arquivo
  python visualize_thermal.py THM12345.BIN
  
  # Visualizar e salvar imagens
  python visualize_thermal.py THM12345.BIN --output-dir ./output
  
  # Especificar número de frames
  python visualize_thermal.py THM12345.BIN --frames 2
  
  # Criar animação GIF
  python visualize_thermal.py THM12345.BIN --gif animation.gif
        """
    )
    
    parser.add_argument('file', type=str, help='Arquivo binário .BIN para processar')
    parser.add_argument('--frames', type=int, default=2, 
                       help='Número de frames no arquivo (THERMAL_SAVE_INTERVAL, padrão: 2)')
    parser.add_argument('--output-dir', type=str, default=None,
                       help='Diretório para salvar as imagens geradas')
    parser.add_argument('--gif', type=str, default=None,
                       help='Caminho para salvar animação GIF')
    parser.add_argument('--colormap', type=str, default='hot',
                       choices=['hot', 'jet', 'viridis', 'plasma', 'inferno', 'magma', 'coolwarm'],
                       help='Mapa de cores para visualização (padrão: hot)')
    parser.add_argument('--no-display', action='store_true',
                       help='Não exibir imagens, apenas salvar')
    
    args = parser.parse_args()
    
    # Verifica se o arquivo existe
    if not os.path.exists(args.file):
        print(f"❌ Erro: Arquivo não encontrado: {args.file}")
        return
    
    print(f"📂 Lendo arquivo: {args.file}")
    print(f"📊 Esperando {args.frames} frame(s)...")
    
    # Lê os frames
    try:
        frames = read_thermal_file(args.file, args.frames)
        print(f"✅ {len(frames)} frame(s) lido(s) com sucesso")
    except Exception as e:
        print(f"❌ Erro ao ler arquivo: {e}")
        return
    
    # Exibe informações sobre os frames
    for i, frame in enumerate(frames):
        temp_min = np.min(frame)
        temp_max = np.max(frame)
        temp_avg = np.mean(frame)
        print(f"\n📈 Frame {i+1}:")
        print(f"   Temperatura Mínima: {temp_min:.2f}°C")
        print(f"   Temperatura Máxima: {temp_max:.2f}°C")
        print(f"   Temperatura Média:  {temp_avg:.2f}°C")
    
    # Visualiza os frames
    if args.output_dir or args.no_display:
        base_name = Path(args.file).stem
        visualize_all_frames(frames, args.output_dir, base_name)
    else:
        for i, frame in enumerate(frames):
            visualize_thermal_frame(frame, f"Frame Térmico #{i+1}", colormap=args.colormap)
    
    # Cria animação GIF se solicitado
    if args.gif:
        print(f"\n🎬 Criando animação GIF...")
        create_animation(frames, args.gif)
    
    print("\n✅ Processamento concluído!")


if __name__ == '__main__':
    main()

