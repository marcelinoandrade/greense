#!/usr/bin/env python3
"""
Script para visualizar dados térmicos salvos pela câmera MLX90640
Arquivo binário: THM#####L.BIN ou THM#####S.BIN
Formato: THERMAL_SAVE_INTERVAL frames de apenas floats (sem timestamp)
Cada frame: 768 floats = 3072 bytes
Total por arquivo: THERMAL_SAVE_INTERVAL × 3072 bytes

Arquivo de metadados: THM#####M.TXT (JSON)
Contém timestamps de cada frame no arquivo binário correspondente
"""

import struct
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from pathlib import Path
import argparse
import os
from datetime import datetime
import time
import json

# Dimensões da câmera térmica MLX90640
THERMAL_ROWS = 24
THERMAL_COLS = 32
THERMAL_TOTAL = THERMAL_ROWS * THERMAL_COLS  # 768 pixels

# Tamanhos em bytes
TIMESTAMP_SIZE = 4  # time_t no ESP32 é 32-bit (4 bytes)
FLOAT_SIZE = 4
FRAME_SIZE = TIMESTAMP_SIZE + (THERMAL_TOTAL * FLOAT_SIZE)  # 4 + 3072 = 3076 bytes


def detect_file_format(data):
    """
    Detecta automaticamente o formato do arquivo (antigo ou novo)
    
    Args:
        data: Bytes do arquivo
    
    Returns:
        'new' se tem timestamp, 'old' se não tem, None se não consegue determinar
    """
    if len(data) < 8:
        return None
    
    # Tenta ler como timestamp (primeiros 4 bytes)
    timestamp_unix = struct.unpack('<i', data[:4])[0]
    
    # Verifica se é um timestamp Unix válido (entre 2020 e 2100)
    MIN_TIMESTAMP = 1577836800  # 2020-01-01 00:00:00
    MAX_TIMESTAMP = 4102444800  # 2100-01-01 00:00:00
    
    if MIN_TIMESTAMP <= timestamp_unix <= MAX_TIMESTAMP:
        # Verifica se o primeiro float após timestamp é uma temperatura razoável
        if len(data) >= 8:
            first_temp = struct.unpack('<f', data[4:8])[0]
            if -50 < first_temp < 100:  # Temperatura razoável
                return 'new'
    
    # Verifica se o primeiro float (sem timestamp) é uma temperatura razoável
    first_temp = struct.unpack('<f', data[:4])[0]
    if -50 < first_temp < 100:
        return 'old'
    
    # Heurística: verifica qual formato se alinha melhor
    old_remainder = len(data) % (THERMAL_TOTAL * FLOAT_SIZE)
    new_remainder = len(data) % FRAME_SIZE
    
    if old_remainder < new_remainder:
        return 'old'
    elif new_remainder < old_remainder:
        return 'new'
    
    return None


def load_metadata_file(bin_filepath):
    """
    Carrega arquivo de metadados JSON correspondente ao arquivo binário
    
    Args:
        bin_filepath: Caminho para o arquivo .BIN
    
    Returns:
        Dict com metadados ou None se não encontrado
    """
    bin_path = Path(bin_filepath)
    # Tenta encontrar arquivo de metadados: THM####M.TXT
    meta_path = bin_path.parent / f"{bin_path.stem.replace('L', 'M').replace('S', 'M')}.TXT"
    
    if not meta_path.exists():
        # Tenta com .txt minúsculo
        meta_path = bin_path.parent / f"{bin_path.stem.replace('L', 'M').replace('S', 'M')}.txt"
    
    if not meta_path.exists():
        return None
    
    try:
        with open(meta_path, 'r', encoding='utf-8') as f:
            metadata = json.load(f)
        return metadata
    except (json.JSONDecodeError, IOError) as e:
        print(f"⚠️ Erro ao ler arquivo de metadados {meta_path}: {e}")
        return None


def read_thermal_file(filepath, thermal_save_interval=3, auto_detect=True, prefer_old_format=True):
    """
    Lê arquivo binário de dados térmicos (suporta formato antigo e novo)
    
    Args:
        filepath: Caminho para o arquivo .BIN
        thermal_save_interval: Número de frames acumulados no arquivo (padrão: 3)
        auto_detect: Se True, detecta automaticamente o formato (padrão: True)
    
    Returns:
        Lista de tuplas (timestamp, frame_array) onde:
        - timestamp: datetime object (ou datetime.now() para formato antigo)
        - frame_array: Array numpy com shape (24, 32)
    """
    with open(filepath, 'rb') as f:
        data = f.read()
    
    # Tenta carregar arquivo de metadados (timestamps)
    metadata = load_metadata_file(filepath)
    timestamps_dict = {}
    if metadata and 'frames' in metadata:
        print(f"📅 Arquivo de metadados encontrado: {len(metadata['frames'])} timestamps")
        for frame_info in metadata['frames']:
            idx = frame_info.get('index', 0)
            timestamp_unix = frame_info.get('timestamp', 0)
            timestamps_dict[idx] = timestamp_unix
    else:
        print(f"📅 Nenhum arquivo de metadados encontrado (usando timestamp atual)")
    
    # Detecta formato automaticamente
    file_format = None
    if auto_detect:
        file_format = detect_file_format(data)
        if file_format == 'new':
            print(f"📅 Formato detectado: NOVO (com timestamp no binário)")
        elif file_format == 'old':
            print(f"📅 Formato detectado: ANTIGO (sem timestamp no binário)")
        else:
            # Se não conseguiu detectar, usa formato preferido
            if prefer_old_format:
                print(f"⚠️ Não foi possível detectar formato automaticamente, usando formato ANTIGO (sem timestamp)...")
                file_format = 'old'
            else:
                print(f"⚠️ Não foi possível detectar formato automaticamente, tentando novo formato...")
                file_format = 'new'
    
    frames = []
    offset = 0
    
    if file_format == 'old':
        # Formato antigo: apenas floats, sem timestamp
        frame_size_old = THERMAL_TOTAL * FLOAT_SIZE  # 3072 bytes
        num_frames = len(data) // frame_size_old
        
        if len(data) % frame_size_old != 0:
            print(f"⚠️ Arquivo antigo não está alinhado (resto: {len(data) % frame_size_old} bytes)")
        
        for i in range(num_frames):
            if offset + frame_size_old > len(data):
                break
            
            # Lê apenas os floats (sem timestamp)
            temps_bytes = data[offset:offset + frame_size_old]
            temps = struct.unpack(f'<{THERMAL_TOTAL}f', temps_bytes)
            
            # Converte para array numpy
            frame_array = np.array(temps).reshape(THERMAL_ROWS, THERMAL_COLS)
            
            # Usa timestamp do arquivo de metadados se disponível, senão usa timestamp atual
            if i in timestamps_dict:
                try:
                    timestamp_dt = datetime.fromtimestamp(timestamps_dict[i])
                except (ValueError, OSError):
                    timestamp_dt = datetime.now()
            else:
                timestamp_dt = datetime.now()
            
            frames.append((timestamp_dt, frame_array))
            offset += frame_size_old
        
        if timestamps_dict:
            print(f"✅ {len(frames)} frame(s) lido(s) (formato antigo, timestamps do arquivo de metadados)")
        else:
            print(f"✅ {len(frames)} frame(s) lido(s) (formato antigo, sem timestamps)")
        
    else:
        # Formato novo: timestamp + floats
        expected_size = thermal_save_interval * FRAME_SIZE
        if len(data) != expected_size:
            print(f"⚠️ Aviso: Tamanho do arquivo ({len(data)} bytes) não corresponde ao esperado ({expected_size} bytes)")
            print(f"   Tentando calcular número de frames...")
            calculated_frames = len(data) // FRAME_SIZE
            if len(data) % FRAME_SIZE != 0:
                print(f"   ⚠️ Arquivo não está alinhado (resto: {len(data) % FRAME_SIZE} bytes)")
            thermal_save_interval = calculated_frames
            print(f"   Calculado: {thermal_save_interval} frames")
        
        for i in range(thermal_save_interval):
            if offset + FRAME_SIZE > len(data):
                print(f"⚠️ Frame {i+1} incompleto (offset={offset}, necessário={FRAME_SIZE}, disponível={len(data)-offset})")
                break
            
            # Lê timestamp (4 bytes, signed int32)
            timestamp_bytes = data[offset:offset + TIMESTAMP_SIZE]
            timestamp_unix = struct.unpack('<i', timestamp_bytes)[0]  # Little-endian, signed int
            
            # Valida timestamp (deve estar entre 2020 e 2100)
            MIN_TIMESTAMP = 1577836800  # 2020-01-01 00:00:00
            MAX_TIMESTAMP = 4102444800  # 2100-01-01 00:00:00
            timestamp_valid = MIN_TIMESTAMP <= timestamp_unix <= MAX_TIMESTAMP
            
            # Converte timestamp Unix para datetime
            if timestamp_valid:
                try:
                    timestamp_dt = datetime.fromtimestamp(timestamp_unix)
                except (ValueError, OSError) as e:
                    print(f"⚠️ Erro ao converter timestamp do frame {i+1}: {e}")
                    print(f"   Timestamp Unix: {timestamp_unix}")
                    timestamp_valid = False
            else:
                print(f"⚠️ Frame {i+1} tem timestamp inválido: {timestamp_unix} (fora do range 2020-2100)")
                timestamp_dt = None
            
            offset += TIMESTAMP_SIZE
            
            # Lê array de temperaturas (768 floats)
            if offset + (THERMAL_TOTAL * FLOAT_SIZE) > len(data):
                print(f"⚠️ Frame {i+1} não tem dados de temperatura suficientes")
                break
            
            temps_bytes = data[offset:offset + (THERMAL_TOTAL * FLOAT_SIZE)]
            temps = struct.unpack(f'<{THERMAL_TOTAL}f', temps_bytes)  # Little-endian
            
            # Converte para array numpy e reorganiza em matriz 24x32
            frame_array = np.array(temps).reshape(THERMAL_ROWS, THERMAL_COLS)
            
            # Valida temperaturas (devem estar em range razoável)
            temp_min = np.min(frame_array)
            temp_max = np.max(frame_array)
            temp_valid = (-50 < temp_min < 100) and (-50 < temp_max < 100) and not np.any(np.isnan(frame_array)) and not np.any(np.isinf(frame_array))
            
            if not temp_valid:
                print(f"⚠️ Frame {i+1} tem temperaturas inválidas: min={temp_min:.2f}°C, max={temp_max:.2f}°C")
                print(f"   Pulando frame {i+1} (dados corrompidos)")
                offset += THERMAL_TOTAL * FLOAT_SIZE
                continue
            
            # Se timestamp inválido mas temperaturas válidas, usa timestamp atual
            if not timestamp_valid:
                timestamp_dt = datetime.now()
                print(f"   Usando timestamp atual como fallback: {timestamp_dt}")
            
            frames.append((timestamp_dt, frame_array))
            offset += THERMAL_TOTAL * FLOAT_SIZE
        
        print(f"✅ {len(frames)} frame(s) válido(s) lido(s) (formato novo, com timestamps)")
        
        # Avisa sobre bytes extras no final do arquivo
        if offset < len(data):
            extra_bytes = len(data) - offset
            print(f"⚠️ Arquivo tem {extra_bytes} bytes extras no final (serão ignorados)")
    
    return frames


def visualize_thermal_frame(timestamp, frame, title=None, save_path=None, colormap='hot'):
    """
    Visualiza um frame térmico com timestamp
    
    Args:
        timestamp: datetime object com o horário da captura
        frame: Array numpy com shape (24, 32)
        title: Título da imagem (opcional, será gerado se None)
        save_path: Caminho para salvar a imagem (opcional)
        colormap: Mapa de cores (hot, jet, viridis, etc.)
    """
    fig, ax = plt.subplots(figsize=(10, 8))
    
    # Cria a imagem térmica
    im = ax.imshow(frame, cmap=colormap, interpolation='bilinear', aspect='auto')
    
    # Adiciona barra de cores
    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label('Temperatura (°C)', rotation=270, labelpad=20)
    
    # Gera título com horário se não fornecido
    if title is None:
        timestamp_str = timestamp.strftime('%Y-%m-%d %H:%M:%S')
        title = f"Imagem Térmica - {timestamp_str}"
    
    # Configurações do gráfico
    ax.set_title(title, fontsize=14, fontweight='bold')
    ax.set_xlabel('Coluna (32 pixels)')
    ax.set_ylabel('Linha (24 pixels)')
    
    # Adiciona valores de temperatura mínima, máxima e média
    temp_min = np.min(frame)
    temp_max = np.max(frame)
    temp_avg = np.mean(frame)
    
    # Formata horário para exibição
    time_str = timestamp.strftime('%H:%M:%S')
    date_str = timestamp.strftime('%d/%m/%Y')
    
    info_text = f'Horário: {time_str} ({date_str}) | Min: {temp_min:.2f}°C | Max: {temp_max:.2f}°C | Média: {temp_avg:.2f}°C'
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
        frames: Lista de tuplas (timestamp, frame_array)
        output_dir: Diretório para salvar as imagens (opcional)
        base_filename: Nome base para os arquivos
    """
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
    
    for i, (timestamp, frame) in enumerate(frames):
        # Gera título com horário
        time_str = timestamp.strftime('%H:%M:%S')
        title = f"Frame Térmico #{i+1} - {time_str}"
        save_path = None
        
        if output_dir:
            # Nome do arquivo inclui horário
            time_filename = timestamp.strftime('%H%M%S')
            save_path = os.path.join(output_dir, f"{base_filename}_{i+1:02d}_{time_filename}.png")
        
        visualize_thermal_frame(timestamp, frame, title=title, save_path=save_path)
        print(f"📊 Frame {i+1}/{len(frames)} processado - {timestamp.strftime('%Y-%m-%d %H:%M:%S')}")


def create_animation(frames, output_path, fps=1):
    """
    Cria uma animação GIF dos frames térmicos com timestamps
    
    Args:
        frames: Lista de tuplas (timestamp, frame_array)
        output_path: Caminho para salvar o GIF
        fps: Frames por segundo
    """
    try:
        from matplotlib.animation import FuncAnimation
    except ImportError:
        print("❌ Erro: matplotlib.animation não disponível. Instale matplotlib completo.")
        return
    
    fig, ax = plt.subplots(figsize=(10, 8))
    
    # Inicializa a imagem com o primeiro frame
    first_timestamp, first_frame = frames[0]
    im = ax.imshow(first_frame, cmap='hot', interpolation='bilinear', aspect='auto')
    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label('Temperatura (°C)', rotation=270, labelpad=20)
    
    ax.set_xlabel('Coluna (32 pixels)')
    ax.set_ylabel('Linha (24 pixels)')
    
    # Extrai apenas os arrays de frames para a animação
    frame_arrays = [frame for _, frame in frames]
    timestamps = [ts for ts, _ in frames]
    
    def update(frame_idx):
        timestamp, frame = frames[frame_idx]
        im.set_array(frame)
        temp_min = np.min(frame)
        temp_max = np.max(frame)
        temp_avg = np.mean(frame)
        time_str = timestamp.strftime('%Y-%m-%d %H:%M:%S')
        ax.set_title(f'{time_str} | Min: {temp_min:.1f}°C | Max: {temp_max:.1f}°C | Média: {temp_avg:.1f}°C',
                     fontsize=12, fontweight='bold')
        return [im]
    
    anim = FuncAnimation(fig, update, frames=range(len(frames)), interval=1000/fps, blit=True, repeat=True)
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
    parser.add_argument('--frames', type=int, default=3, 
                       help='Número de frames no arquivo (THERMAL_SAVE_INTERVAL, padrão: 3)')
    parser.add_argument('--format', type=str, choices=['auto', 'old', 'new'], default='auto',
                       help='Formato do arquivo: auto (detecta), old (sem timestamp), new (com timestamp). Padrão: auto (prefere old)')
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
    print(f"📏 Tamanho esperado por frame: {FRAME_SIZE} bytes (timestamp: {TIMESTAMP_SIZE} + dados: {THERMAL_TOTAL * FLOAT_SIZE})")
    
    # Determina formato baseado no argumento
    prefer_old = True
    if args.format == 'new':
        prefer_old = False
    elif args.format == 'old':
        prefer_old = True
    # Se 'auto', usa prefer_old=True por padrão
    
    # Lê os frames
    try:
        frames = read_thermal_file(args.file, args.frames, auto_detect=(args.format == 'auto'), prefer_old_format=prefer_old)
        print(f"✅ {len(frames)} frame(s) lido(s) com sucesso")
    except Exception as e:
        print(f"❌ Erro ao ler arquivo: {e}")
        import traceback
        traceback.print_exc()
        return
    
    # Exibe informações sobre os frames
    print("\n" + "="*60)
    for i, (timestamp, frame) in enumerate(frames):
        temp_min = np.min(frame)
        temp_max = np.max(frame)
        temp_avg = np.mean(frame)
        time_str = timestamp.strftime('%Y-%m-%d %H:%M:%S')
        print(f"\n📈 Frame {i+1}:")
        print(f"   Horário: {time_str}")
        print(f"   Temperatura Mínima: {temp_min:.2f}°C")
        print(f"   Temperatura Máxima: {temp_max:.2f}°C")
        print(f"   Temperatura Média:  {temp_avg:.2f}°C")
    print("="*60)
    
    # Visualiza os frames
    if args.output_dir or args.no_display:
        base_name = Path(args.file).stem
        visualize_all_frames(frames, args.output_dir, base_name)
    else:
        for i, (timestamp, frame) in enumerate(frames):
            time_str = timestamp.strftime('%H:%M:%S')
            visualize_thermal_frame(timestamp, frame, 
                                   f"Frame Térmico #{i+1} - {time_str}", 
                                   colormap=args.colormap)
    
    # Cria animação GIF se solicitado
    if args.gif:
        print(f"\n🎬 Criando animação GIF...")
        create_animation(frames, args.gif)
    
    print("\n✅ Processamento concluído!")


if __name__ == '__main__':
    main()

