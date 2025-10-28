/* Servidor AP + HTTP para visualização e calibração
 *
 * Rotas:
 *   GET /            painel com gráficos e status
 *   GET /history     últimos pontos em JSON
 *   GET /download    CSV completo
 *   GET /calibra     página de calibração
 *   GET /set_calibra?seco=XXXX&molhado=YYYY   salva calibração
 */

 #include "http_server.h"

 #include <string.h>
 #include <stdio.h>
 #include <stdlib.h>
 
 #include "esp_event.h"
 #include "esp_log.h"
 #include "esp_wifi.h"
 #include "esp_netif.h"
 #include "esp_http_server.h"
 
 #include "data_logger.h"
 #include "sensores.h"
 #include "atuadores.h"
 
 static const char *TAG = "HTTP_SERVER";
 
 static httpd_handle_t server_handle = NULL;
 
 /* -------------------------------------------------------------------------- */
 /* Wi-Fi SoftAP                                                               */
 /* -------------------------------------------------------------------------- */
 
 static void wifi_event_handler(void *arg,
                                esp_event_base_t event_base,
                                int32_t event_id,
                                void *event_data)
 {
     if (event_base == WIFI_EVENT &&
         event_id == WIFI_EVENT_AP_STACONNECTED)
     {
         atuadores_cliente_conectou();
         wifi_event_ap_staconnected_t *e =
             (wifi_event_ap_staconnected_t *)event_data;
         ESP_LOGI(TAG,
                  "Cliente %02x:%02x:%02x:%02x:%02x:%02x conectou, AID=%d",
                  e->mac[0], e->mac[1], e->mac[2],
                  e->mac[3], e->mac[4], e->mac[5],
                  e->aid);
     }
     else if (event_base == WIFI_EVENT &&
              event_id == WIFI_EVENT_AP_STADISCONNECTED)
     {
        atuadores_cliente_desconectou();
         wifi_event_ap_stadisconnected_t *e =
             (wifi_event_ap_stadisconnected_t *)event_data;
         ESP_LOGI(TAG,
                  "Cliente %02x:%02x:%02x:%02x:%02x:%02x saiu, AID=%d",
                  e->mac[0], e->mac[1], e->mac[2],
                  e->mac[3], e->mac[4], e->mac[5],
                  e->aid);
     }
 }
 
 static esp_err_t wifi_init_softap(void)
 {
     ESP_ERROR_CHECK(esp_netif_init());
     ESP_ERROR_CHECK(esp_event_loop_create_default());
 
     esp_netif_create_default_wifi_ap();
 
     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
     ESP_ERROR_CHECK(esp_wifi_init(&cfg));
 
     ESP_ERROR_CHECK(
         esp_event_handler_instance_register(
             WIFI_EVENT,
             ESP_EVENT_ANY_ID,
             &wifi_event_handler,
             NULL,
             NULL));
 
     wifi_config_t ap_config = {
         .ap = {
             .ssid = "ESP32_TEMP",
             .ssid_len = 0,
             .password = "12345678",
             .channel = 1,
             .authmode = WIFI_AUTH_WPA_WPA2_PSK,
             .max_connection = 4,
             .pmf_cfg = {
                 .required = false,
             },
         },
     };
 
     if (strlen((char *)ap_config.ap.password) == 0)
     {
         ap_config.ap.authmode = WIFI_AUTH_OPEN;
     }
 
     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
     ESP_ERROR_CHECK(esp_wifi_start());
 
     ESP_LOGI(TAG,
              "Modo AP iniciado. SSID='%s' senha='%s'",
              ap_config.ap.ssid,
              ap_config.ap.password);
 
     /* O esp_netif já cria DHCP server padrão para WIFI_AP_DEF
        e entrega IP tipo 192.168.4.1 */
 
     return ESP_OK;
 }
 
 /* -------------------------------------------------------------------------- */
 /* Handlers HTTP                                                              */
 /* -------------------------------------------------------------------------- */
 
 /* Página principal / */
 static esp_err_t handle_root(httpd_req_t *req)
{
    float temp_ar      = sensores_get_temp_ar();
    float umid_ar      = sensores_get_umid_ar();
    float temp_solo    = sensores_get_temp_solo();
    int   umid_raw     = sensores_get_umid_solo_raw();

    float seco, molhado;
    data_logger_get_calibracao(&seco, &molhado);

    float umid_solo_pct = data_logger_raw_to_pct(umid_raw);

    char page[6000];
    int len = snprintf(
        page, sizeof(page),
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset='utf-8'/>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
        "<title>Sensor de Campo</title>"
        "<style>"
        "body{font-family:sans-serif;margin:16px;background:#f5f5f5;color:#222}"
        ".card{background:#fff;border-radius:8px;padding:12px;margin-bottom:16px;"
        "box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
        "h1,h2{margin-top:0}"
        ".grid{display:grid;grid-template-columns:1fr 1fr;grid-gap:12px}"
        "canvas{max-width:100%%;height:180px;border:1px solid #ccc;border-radius:4px}"
        "a.button{display:inline-block;margin-top:12px;padding:8px 12px;"
        "background:#1976d2;color:#fff;border-radius:4px;text-decoration:none;"
        "font-size:14px}"
        "table{font-size:14px}"
        "td{padding:4px 8px;vertical-align:top}"
        "</style>"
        "</head>"
        "<body>"

        "<div class='card'>"
        "<h1>Sensor de Campo</h1>"
        "<table>"
        "<tr><td>Temp Ar:</td><td>%.2f &deg;C</td></tr>"
        "<tr><td>UR Ar:</td><td>%.2f %%</td></tr>"
        "<tr><td>Temp Solo:</td><td>%.2f &deg;C</td></tr>"
        "<tr><td>Umid Solo Atual:</td><td>%.2f %% (raw=%d)</td></tr>"
        "<tr><td>Calibra&ccedil;&atilde;o:</td><td>seco=%.0f molhado=%.0f</td></tr>"
        "</table>"
        "<div style='display:flex;gap:12px;flex-wrap:wrap;margin-top:8px'>"
        "  <a class='button' href='/calibra'>Calibrar umidade do solo</a>"
        "  <a class='button' href='/download' style='background:#388e3c'>Baixar log (CSV)</a>"
        "</div>"
        "</div>"

        "<div class='card'>"
        "<h2>Hist&oacute;rico recente</h2>"
        "<div class='grid'>"
        "<div><canvas id='chart_temp_ar'></canvas></div>"
        "<div><canvas id='chart_umid_ar'></canvas></div>"
        "<div><canvas id='chart_temp_solo'></canvas></div>"
        "<div><canvas id='chart_umid_solo'></canvas></div>"
        "</div>"
        "<p style='font-size:12px;color:#555'>Exibindo at&eacute; 10 &uacute;ltimos pontos.</p>"
        "</div>"

        "<script>"
        "async function atualiza(){"
        " try{"
        "  const r=await fetch('/history');"
        "  const j=await r.json();"

        "  function extrairXY(arr){"
        "    const xs=[];const ys=[];"
        "    for(let i=0;i<arr.length;i++){"
        "      xs.push(arr[i][0]);"
        "      ys.push(arr[i][1]);"
        "    }"
        "    return {xs,ys};"
        "  }"

        "  const tAr   = extrairXY(j.temp_ar_points||[]);"
        "  const uAr   = extrairXY(j.umid_ar_points||[]);"
        "  const tSolo = extrairXY(j.temp_solo_points||[]);"
        "  const uSolo = extrairXY(j.umid_solo_points||[]);"

        "  desenha('chart_temp_ar','Temp Ar (C)',tAr.xs,tAr.ys);"
        "  desenha('chart_umid_ar','UR Ar (%%)',uAr.xs,uAr.ys);"
        "  desenha('chart_temp_solo','Temp Solo (C)',tSolo.xs,tSolo.ys);"
        "  desenha('chart_umid_solo','Umid Solo (%%)',uSolo.xs,uSolo.ys);"
        " }catch(e){console.log('erro /history',e);}"
        "}"

        "function desenha(id,titulo,labels,data){"
        "  const ctx=document.getElementById(id).getContext('2d');"
        "  if(!ctx._chartRef){"
        "    ctx._chartRef=new Chart(ctx,{"
        "      type:'line',"
        "      data:{labels:labels,datasets:[{label:titulo,data:data,fill:false}]}," /* cor padrão */
        "      options:{responsive:true,scales:{y:{beginAtZero:false}}}"
        "    });"
        "  }else{"
        "    ctx._chartRef.data.labels=labels;"
        "    ctx._chartRef.data.datasets[0].data=data;"
        "    ctx._chartRef.data.datasets[0].label=titulo;"
        "    ctx._chartRef.update();"
        "  }"
        "}"

        "setInterval(atualiza,5000);"
        "atualiza();"
        "</script>"

        /* Chart mínimo inline. Só inclua este bloco se você NÃO
           já estiver servindo Chart.js de CDN. Se você já tem Chart.js
           carregado em outro lugar, remova este bloco inteiro. */
        "<script>"
        "!function(){"
        "function Chart(c,o){this.ctx=c;this.data=o.data;this.type=o.type;this.options=o.options;this._init()}"
        "Chart.prototype._init=function(){this._draw()};"
        "Chart.prototype.update=function(){this._draw()};"
        "Chart.prototype._draw=function(){"
        " const ctx=this.ctx;"
        " const w=ctx.canvas.width;const h=ctx.canvas.height;"
        " ctx.clearRect(0,0,w,h);"
        " ctx.strokeStyle='#000';ctx.lineWidth=1;"
        " ctx.strokeRect(0,0,w,h);"
        " const labels=this.data.labels||[];"
        " const vals=this.data.datasets[0].data||[];"
        " const lbl=this.data.datasets[0].label||'';"
        " const minVal=Math.min.apply(null,vals.length?vals:[0]);"
        " const maxVal=Math.max.apply(null,vals.length?vals:[1]);"
        " const range=(maxVal-minVal)||1;"
        " ctx.beginPath();ctx.strokeStyle='#1976d2';"
        " for(let i=0;i<vals.length;i++){"
        "   const x=(i/(vals.length-1||1))*w;"
        "   const y=h-((vals[i]-minVal)/range)*h;"
        "   if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);"
        " }"
        " ctx.stroke();"
        " ctx.fillStyle='#000';ctx.font='10px sans-serif';"
        " ctx.fillText(lbl,4,12);"
        "};"
        "window.Chart=Chart;"
        "}();"
        "</script>"

        "</body>"
        "</html>",
        (double)temp_ar,
        (double)umid_ar,
        (double)temp_solo,
        (double)umid_solo_pct, umid_raw,
        (double)seco,
        (double)molhado
    );

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, page, len);
}

 
 /* /history -> últimos pontos em JSON */
 static esp_err_t handle_history(httpd_req_t *req)
 {
     char *json = data_logger_build_history_json();
     if (!json) {
         httpd_resp_set_status(req, "500 Internal Server Error");
         return httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
     }
 
     httpd_resp_set_type(req, "application/json");
     esp_err_t ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
     free(json);
     return ret;
 }
 
 /* /download -> CSV inteiro */
 static esp_err_t handle_download(httpd_req_t *req)
 {
     FILE *f = fopen("/spiffs/log_temp.csv", "r");
     if (!f)
     {
         httpd_resp_send_err(req,
                             HTTPD_500_INTERNAL_SERVER_ERROR,
                             "Erro abrindo log");
         return ESP_FAIL;
     }
 
     httpd_resp_set_type(req, "text/csv");
     httpd_resp_set_hdr(req, "Content-Disposition",
                        "attachment; filename=\"log_temp.csv\"");
 
     char buf[256];
     while (fgets(buf, sizeof(buf), f))
     {
         httpd_resp_sendstr_chunk(req, buf);
     }
     fclose(f);
     httpd_resp_sendstr_chunk(req, NULL); /* fim chunked */
     return ESP_OK;
 }
 
 /* /calibra -> página HTML com calibração atual e leitura atual */
 static esp_err_t handle_calibra(httpd_req_t *req)
 {
     float seco, molhado;
     data_logger_get_calibracao(&seco, &molhado);
 
     int leitura_raw = sensores_get_umid_solo_raw();
 
     char page[1024];
     snprintf(page, sizeof(page),
              "<!DOCTYPE html><html><head>"
              "<meta charset='utf-8'/>"
              "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
              "<title>Calibracao Solo - greenSE</title>"
              "<style>"
              "body{background:#111;color:#eee;font-family:sans-serif;padding:1rem;}"
              "input{background:#222;color:#eee;border:1px solid #444;"
              "padding:0.5rem;width:8rem;margin-bottom:.5rem;}"
              "button{background:#222;color:#eee;border:1px solid #444;"
              "padding:.5rem 1rem;margin-top:1rem;border-radius:4px;}"
              "a{color:#4af;}"
              "</style></head><body>"
              "<h2>Calibracao Umidade do Solo</h2>"
              "<p>Leitura atual bruta: <b>%d</b></p>"
              "<p>SECO atual: %.0f | MOLHADO atual: %.0f</p>"
              "<form action='/set_calibra' method='get'>"
              "<label>Novo Seco: <input type='number' name='seco' value='%.0f'></label><br>"
              "<label>Novo Molhado: <input type='number' name='molhado' value='%.0f'></label><br>"
              "<button type='submit'>Salvar calibracao</button>"
              "</form>"
              "<p><a href='/'>Voltar</a></p>"
              "</body></html>",
              leitura_raw,
              seco,
              molhado,
              seco,
              molhado);
 
     httpd_resp_set_type(req, "text/html");
     httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
     return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
 }
 
 /* /set_calibra -> salva calibração e redireciona */
 static esp_err_t handle_set_calibra(httpd_req_t *req)
 {
     /* pega query string */
     char qs[128];
     int len = httpd_req_get_url_query_str(req, qs, sizeof(qs));
     if (len < 0)
     {
         httpd_resp_send_err(req,
                             HTTPD_400_BAD_REQUEST,
                             "sem query");
         return ESP_FAIL;
     }
 
     char buf_seco[32];
     char buf_molhado[32];
     if (httpd_query_key_value(qs, "seco",
                               buf_seco, sizeof(buf_seco)) != ESP_OK ||
         httpd_query_key_value(qs, "molhado",
                               buf_molhado, sizeof(buf_molhado)) != ESP_OK)
     {
         httpd_resp_send_err(req,
                             HTTPD_400_BAD_REQUEST,
                             "parametros faltando");
         return ESP_FAIL;
     }
 
     float novo_seco    = atof(buf_seco);
     float novo_molhado = atof(buf_molhado);
 
     if (data_logger_set_calibracao(novo_seco, novo_molhado) != ESP_OK)
     {
         httpd_resp_send_err(req,
                             HTTPD_500_INTERNAL_SERVER_ERROR,
                             "falha salvando calib");
         return ESP_FAIL;
     }
 
     /* resposta simples tipo redirect manual */
     const char *ok_page =
         "<!DOCTYPE html><html><body>"
         "<p>Calibracao salva.</p>"
         "<p><a href='/calibra'>Voltar</a></p>"
         "</body></html>";
 
     httpd_resp_set_type(req, "text/html");
     return httpd_resp_send(req, ok_page, HTTPD_RESP_USE_STRLEN);
 }
 
 /* responde favicon.ico para parar 404 */
static esp_err_t handle_favicon(httpd_req_t *req)
{
    static const unsigned char favicon_png[] = {
        0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,
        0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x06,0x00,0x00,0x00,0x1F,0x15,0xC4,
        0x89,0x00,0x00,0x00,0x0A,0x49,0x44,0x41,
        0x54,0x78,0x9C,0x63,0x60,0x00,0x02,0x00,
        0x00,0x05,0x00,0x01,0x0D,0x0A,0x2D,0xB4,
        0x00,0x00,0x00,0x00,0x49,0x45,0x4E,0x44,
        0xAE,0x42,0x60,0x82
    };

    httpd_resp_set_type(req, "image/png");
    return httpd_resp_send(req, (const char *)favicon_png, sizeof(favicon_png));
}

/* atende /generate_204 (Android captive portal check) */
static esp_err_t handle_generate204(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, NULL, 0);
}

 
 /* -------------------------------------------------------------------------- */
 /* Inicialização/parada do servidor HTTP                                      */
 /* -------------------------------------------------------------------------- */
 
 esp_err_t http_server_start(void)
{
    if (server_handle != NULL) {
        return ESP_OK;
    }

    ESP_ERROR_CHECK(wifi_init_softap());

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Robustez */
    config.stack_size        = 10240;  /* mais stack evita overflow */
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    config.max_open_sockets  = 4;
    config.lru_purge_enable  = true;

    /* IMPORTANTE:
       Removido config.max_uri_len e config.max_header_len
       porque sua struct httpd_config_t não tem esses campos.
    */

    if (httpd_start(&server_handle, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar httpd");
        return ESP_FAIL;
    }

    /* registrar rotas principais */
    httpd_uri_t uri_root = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = handle_root,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server_handle, &uri_root);

    httpd_uri_t uri_hist = {
        .uri      = "/history",
        .method   = HTTP_GET,
        .handler  = handle_history,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server_handle, &uri_hist);

    httpd_uri_t uri_down = {
        .uri      = "/download",
        .method   = HTTP_GET,
        .handler  = handle_download,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server_handle, &uri_down);

    httpd_uri_t uri_cal = {
        .uri      = "/calibra",
        .method   = HTTP_GET,
        .handler  = handle_calibra,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server_handle, &uri_cal);

    httpd_uri_t uri_set = {
        .uri      = "/set_calibra",
        .method   = HTTP_GET,
        .handler  = handle_set_calibra,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server_handle, &uri_set);

    /* favicon padrao */
    httpd_uri_t uri_icon = {
        .uri      = "/favicon.ico",
        .method   = HTTP_GET,
        .handler  = handle_favicon,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server_handle, &uri_icon);

    /* rota de teste captive portal Android */
    httpd_uri_t uri_gen204 = {
        .uri      = "/generate_204",
        .method   = HTTP_GET,
        .handler  = handle_generate204,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server_handle, &uri_gen204);

    ESP_LOGI(TAG,
             "Servidor HTTP iniciado e rotas registradas. Acesse http://192.168.4.1/");
    return ESP_OK;
}

 
 esp_err_t http_server_stop(void)
 {
     if (server_handle)
     {
         httpd_stop(server_handle);
         server_handle = NULL;
         return ESP_OK;
     }
     return ESP_FAIL;
 }
 