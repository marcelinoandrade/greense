import time
import json
import network
import socket
import ure  # regex leve para parsear querystring

class TempWebServer:
    """
    Servidor AP + HTTP:
    - /                 dashboard com 2 gráficos (temp_ar e umid_solo)
    - /history          dados em JSON (últimos 10 pontos)
    - /download         CSV bruto
    - /calibra          página de calibração do solo
    - /set_calibra      aplica calibração (via query seco=...&molhado=...)
    """

    def __init__(self, logger, soil_sensor, ap_ssid="ESP32_TEMP", ap_password="12345678"):
        self.logger = logger
        self.soil = soil_sensor
        self.csv_path = logger.path
        self.ip = None
        self.sock = None

        self._start_ap(ap_ssid, ap_password)
        self._start_http_server()

    # ---------- Wi-Fi AP ----------
    def _start_ap(self, ssid, password):
        ap = network.WLAN(network.AP_IF)
        ap.active(False)
        time.sleep_ms(200)
        ap.active(True)
        ap.config(essid=ssid, password=password, authmode=network.AUTH_WPA_WPA2_PSK)

        for _ in range(20):
            if ap.active():
                break
            time.sleep_ms(200)

        self.ip = ap.ifconfig()[0]
        print("AP ativo. SSID =", ssid, "IP =", self.ip)

    # ---------- Socket HTTP ----------
    def _start_http_server(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(('', 80))
        s.listen(2)
        s.setblocking(False)
        self.sock = s
        print("Servidor HTTP escutando porta 80")

    # ---------- HTTP util ----------
    def _http_response(self, client, status, content_type, body, extra_headers=None):
        hdr = "HTTP/1.1 %s\r\nContent-Type: %s\r\nCache-Control: no-cache\r\nConnection: close\r\n" % (
            status,
            content_type
        )
        if extra_headers:
            for k, v in extra_headers.items():
                hdr += "%s: %s\r\n" % (k, v)
        hdr += "\r\n"

        client.sendall(hdr.encode())
        if isinstance(body, str):
            client.sendall(body.encode())
        else:
            client.sendall(body)
        client.close()

    # ---------- /history ----------
    def _read_history_as_json(self):
        lines = self.logger.ler_linhas()

        temp_all_seq = []
        solo_all_seq = []

        for i, line in enumerate(lines):
            line = line.strip()
            if not line or i == 0:
                continue  # pula cabeçalho

            parts = line.split(",")
            if len(parts) < 5:
                continue

            try:
                n_val = int(parts[0])
                temp_ar_val = float(parts[1])
                umid_solo_val = float(parts[4])
            except:
                continue

            if -40 <= temp_ar_val <= 100:
                temp_all_seq.append((n_val, temp_ar_val))
            if 0 <= umid_solo_val <= 100:
                solo_all_seq.append((n_val, umid_solo_val))

        # Mantém apenas os últimos 10 pontos
        temp_recent = temp_all_seq[-10:]
        solo_recent = solo_all_seq[-10:]

        temp_points = [[n, t] for n, t in temp_recent]
        solo_points = [[n, u] for n, u in solo_recent]

        return json.dumps({
            "temp_points": temp_points,
            "solo_points": solo_points
        })

    # ---------- /download ----------
    def _send_csv_download(self, client):
        try:
            with open(self.csv_path, "r") as f:
                csv_data = f.read()
        except OSError:
            csv_data = "N,temp_ar_C,umid_ar_pct,temp_solo_C,umid_solo_pct\n"

        self._http_response(
            client,
            "200 OK",
            "text/csv",
            csv_data,
            extra_headers={"Content-Disposition": "attachment; filename=log_temp.csv"}
        )

    # ---------- /calibra ----------
    def _page_calibra(self):
        seco_v = self.soil.seco_raw
        molh_v = self.soil.molhado_raw
        leitura_atual = self.soil.leitura_bruta()

        return """<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Calibração Solo - greenSE</title>
<style>
body { font-family:sans-serif; background:#111; color:#eee; padding:1rem; }
input { background:#222; color:#eee; border:1px solid #444; padding:0.5rem; width:8rem; }
button { background:#222; color:#eee; border:1px solid #444; padding:0.5rem 1rem; margin-top:1rem; border-radius:4px; }
a { color:#4af; }
</style>
</head>
<body>
<h2>Calibração Umidade do Solo</h2>
<p>Leitura atual: <b>%d</b></p>
<p>SECO: %d | MOLHADO: %d</p>
<form action="/set_calibra" method="get">
<label>seco: <input type="number" name="seco" value="%d"></label><br>
<label>molhado: <input type="number" name="molhado" value="%d"></label><br>
<button type="submit">Salvar calibração</button>
</form>
<p><a href="/">Voltar ao painel</a></p>
</body></html>
""" % (leitura_atual, seco_v, molh_v, seco_v, molh_v)

    # ---------- /set_calibra ----------
    def _handle_set_calibra(self, path_full, client):
        seco_val = None
        molh_val = None

        m = ure.search(r"\?(.*)", path_full)
        if m:
            qs = m.group(1)
            for kv in qs.split("&"):
                if "=" in kv:
                    k, v = kv.split("=", 1)
                    if k == "seco":
                        seco_val = v
                    elif k == "molhado":
                        molh_val = v

        ok = False
        if seco_val is not None and molh_val is not None:
            try:
                seco_f = float(seco_val)
                molh_f = float(molh_val)
                ok = self.soil.salvar_calibracao(seco_f, molh_f)
            except:
                ok = False

        msg = "Calibração salva." if ok else "Falha ao salvar calibração."
        self._http_response(
            client,
            "200 OK",
            "text/html",
            """<html><body style='background:#111;color:#eee;font-family:sans-serif;padding:1rem;'>
            <h3>%s</h3>
            <p>seco=%s molhado=%s</p>
            <p><a style='color:#4af;' href='/calibra'>Voltar</a></p>
            </body></html>""" % (msg, seco_val, molh_val)
        )

    # ---------- Página principal ----------
    def _page_main(self):
        return """<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Unidade Eletrônica de Cultivo Remoto - greenSE</title>
<style>
body { font-family:sans-serif; background:#111; color:#eee; padding:1rem; }
h1 { color:#fff; }
canvas { background:#222; border:1px solid #444; width:100%%; max-width:400px; height:200px; }
a.btn { display:inline-block; background:#222; color:#eee; border:1px solid #444; padding:0.5rem 1rem; margin-right:0.5rem; border-radius:4px; text-decoration:none; }
#status { font-size:0.8rem; color:#888; margin-top:0.5rem; }
</style>
</head>
<body>
<h1>Unidade Eletrônica de Cultivo Remoto - greenSE</h1>
<a class="btn" href="/download">Baixar CSV</a>
<a class="btn" href="/calibra">Calibrar Solo</a>
<h3>Temperatura do Ar (°C)</h3>
<canvas id="chartTemp"></canvas>
<h3>Umidade do Solo (%%)</h3>
<canvas id="chartSolo"></canvas>
<div id="status"></div>
<script>
function drawChart(id, points, label){
  const c = document.getElementById(id);
  const ctx = c.getContext('2d');
  ctx.clearRect(0,0,c.width,c.height);
  if(points.length<2){ctx.fillStyle='#eee';ctx.fillText('Poucos dados',10,20);return;}
  const x=points.map(p=>p[0]),y=points.map(p=>p[1]);
  const xmin=Math.min(...x),xmax=Math.max(...x);
  let ymin=Math.min(...y),ymax=Math.max(...y);
  if(ymin==ymax){ymin-=0.5;ymax+=0.5;}
  function xs(v){return ((v-xmin)/(xmax-xmin))*(c.width-40)+20;}
  function ys(v){return (1-(v-ymin)/(ymax-ymin))*(c.height-40)+20;}
  ctx.strokeStyle='#0f0';ctx.beginPath();
  for(let i=0;i<points.length;i++){const xp=xs(points[i][0]),yp=ys(points[i][1]);if(i==0)ctx.moveTo(xp,yp);else ctx.lineTo(xp,yp);}ctx.stroke();
  ctx.fillStyle='#888';ctx.fillText(label+': '+y[y.length-1].toFixed(1),25,25);
}
function loadData(){
 fetch('/history').then(r=>r.json()).then(obj=>{
  drawChart('chartTemp',obj.temp_points||[],'Temp');
  drawChart('chartSolo',obj.solo_points||[],'Solo');
  document.getElementById('status').innerText='Total pontos: '+(obj.temp_points.length);
 });
}
loadData();
</script>
</body></html>
"""

    # ---------- loop principal ----------
    def poll_once(self):
        try:
            client, _ = self.sock.accept()
        except OSError:
            return

        try:
            req = client.recv(512)
        except OSError:
            client.close()
            return

        try:
            first_line = req.decode().split("\r\n")[0]
        except:
            first_line = ""
        parts = first_line.split(" ")
        path = parts[1] if len(parts) >= 2 else "/"

        try:
            if path.startswith("/set_calibra"):
                self._handle_set_calibra(path, client)
            elif path == "/":
                self._http_response(client, "200 OK", "text/html", self._page_main())
            elif path == "/history":
                self._http_response(client, "200 OK", "application/json", self._read_history_as_json())
            elif path == "/download":
                self._send_csv_download(client)
            elif path == "/calibra":
                self._http_response(client, "200 OK", "text/html", self._page_calibra())
            else:
                self._http_response(client, "404 Not Found", "text/plain", "not found")
        except OSError:
            client.close()
