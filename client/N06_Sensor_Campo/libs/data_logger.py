import os
import time

class LoggerInterno:
    def __init__(self, path="/log_temp.csv"):
        self.path = path
        if not self._existe():
            self._cria_arquivo()

    def _existe(self):
        try:
            os.stat(self.path)
            return True
        except OSError:
            return False

    def _cria_arquivo(self):
        with open(self.path, "w") as f:
            f.write("timestamp_s,temp_ar_C,umid_ar_pct,temp_solo_C,umid_solo_pct\n")

    def append(self, ts, temp_ar, umid_ar, temp_solo, umid_solo):
        linha = "{},{:.2f},{:.2f},{:.2f},{:.2f}\n".format(
            ts,
            temp_ar,
            umid_ar,
            temp_solo,
            umid_solo
        )
        with open(self.path, "a") as f:
            f.write(linha)

    def ler_linhas(self):
        try:
            with open(self.path, "r") as f:
                return f.readlines()
        except OSError:
            return []
