#!/usr/bin/env python3
# arquivo: copia_foto_cam03.py

import os
import sys
import shutil
from datetime import datetime

SRC_DIR = "/home/greense/projetoGreense/server/N01_RASP4_LAB/fotos_recebidas/cam_02"
DST_DIR = "/home/greense/projetoGreense/server/N01_RASP4_LAB/fotos_recebidas/historico_cam_02"
SRC_FILE = os.path.join(SRC_DIR, "ultima.jpg")

def main():
    if not os.path.isfile(SRC_FILE):
        print(f"ERRO: não encontrei {SRC_FILE}", file=sys.stderr)
        sys.exit(1)

    os.makedirs(DST_DIR, exist_ok=True)
    now = datetime.now()
    # Formato: ultimaYYYYMMDD_HHMMSS.jpg (garante único por minuto/segundo)
    dst_name = f"ultima{now:%Y%m%d_%H%M%S}.jpg"
    dst_path = os.path.join(DST_DIR, dst_name)

    shutil.copy2(SRC_FILE, dst_path)
    print(f"Copiado: {SRC_FILE} -> {dst_path}")

if __name__ == "__main__":
    main()
