/* Servidor AP + HTTP para visualização e calibração
 *
 * Rotas:
 *   GET /            painel com gráficos e status
 *   GET /history     últimos pontos em JSON
 *   GET /download    CSV completo
 *   GET /calibra     página de calibração
 *   GET /set_calibra?seco=XXXX&molhado=YYYY   salva calibração
 *   GET /sampling    seleciona período de amostragem
 *   GET /set_sampling?periodo=XXXX            salva período
 */

#include "gui_http_server.h"
#include "../../bsp/network/bsp_wifi_ap.h"
#include "../../app/gui_services.h"
#include "../../bsp/board.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_http_server.h"

static const char *TAG = "GUI_HTTP_SERVER";

static httpd_handle_t server_handle = NULL;

typedef struct {
    uint32_t ms;
    const char *label;
} sampling_option_def_t;

static const sampling_option_def_t SAMPLING_OPTIONS[] = {
    { 1 * 1000,         "1 segundo" },
    { 60 * 1000,        "1 minuto" },
    { 10 * 60 * 1000,   "10 minutos" },
    { 60 * 60 * 1000,   "1 hora" },
    { 6 * 60 * 60 * 1000,  "6 horas" },
    { 12 * 60 * 60 * 1000, "12 horas" },
};

static const size_t SAMPLING_OPTION_COUNT =
    sizeof(SAMPLING_OPTIONS) / sizeof(SAMPLING_OPTIONS[0]);

static const sampling_option_def_t* find_sampling_option(uint32_t ms)
{
    for (size_t i = 0; i < SAMPLING_OPTION_COUNT; ++i) {
        if (SAMPLING_OPTIONS[i].ms == ms) {
            return &SAMPLING_OPTIONS[i];
        }
    }
    return NULL;
}

/* -------------------------------------------------------------------------- */
/* Handlers HTTP                                                              */
/* -------------------------------------------------------------------------- */

/* Página principal / */
static esp_err_t handle_dashboard(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Serviços não disponíveis");
        return ESP_FAIL;
    }
    
    float temp_ar      = svc->get_temp_air();
    float umid_ar      = svc->get_humid_air();
    float temp_solo    = svc->get_temp_soil();
    int   umid_raw     = svc->get_soil_raw();

    float seco, molhado;
    svc->get_calibration(&seco, &molhado);

    float umid_solo_pct = svc->get_soil_pct(umid_raw);

    /* Buffer alocado no heap para evitar stack overflow */
    char *page = (char *)malloc(6000);
    if (!page) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro de memória");
        return ESP_FAIL;
    }
    
    int len = snprintf(
        page, 6000,
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset='utf-8'/>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
        "<title>Sensor de Campo</title>"
        "<style>"
        "body{font-family:'Inter',sans-serif;margin:0;padding:24px;"
        "background:linear-gradient(180deg,#e8f5e9 0%%,#f6fff5 65%%,#ffffff 100%%);color:#1f2a24}"
        ".content{max-width:960px;margin:auto}"
        ".card{background:#fff;border-radius:16px;padding:20px 24px;margin-bottom:20px;"
        "box-shadow:0 16px 30px rgba(0,0,0,0.08)}"
        ".hero{background:linear-gradient(135deg,#dcedc8,#f1f8e9)}"
        "h1{margin:0;font-size:28px;color:#1b5e20}"
        "h2{margin-top:0;color:#2e4a34}"
        ".subtitle{color:#4f5b62;margin:8px 0 18px;font-size:15px;line-height:1.4}"
        ".info-text{font-size:14px;color:#546e7a;margin:14px 0}"
        ".grid{display:grid;grid-template-columns:1fr 1fr;grid-gap:16px}"
        "canvas{max-width:100%%;height:190px;border:1px solid #dfe6e0;border-radius:10px;background:#fafafa}"
        "a.button{display:inline-block;margin-top:12px;padding:11px 18px;"
        "background:#388e3c;color:#fff;border-radius:8px;text-decoration:none;"
        "font-size:14px;font-weight:600;box-shadow:0 6px 18px rgba(56,142,60,0.35)}"
        ".badge{display:inline-flex;align-items:center;gap:6px;background:#c8e6c9;"
        "color:#1b5e20;font-size:12px;font-weight:700;border-radius:999px;padding:4px 12px}"
        ".caption{font-size:12px;color:#78909c;margin-top:10px}"
        "table{font-size:14px;width:100%%;border-collapse:separate;border-spacing:0 6px}"
        "td{padding:6px 8px;vertical-align:top}"
        "</style>"
        "</head>"
        "<body>"
        "<div class='content'>"

        "<div class='card hero'>"
        "<div class='badge'>greenSe Campo</div>"
        "<h1>Painel em tempo real</h1>"
        "<p class='subtitle'>Monitore o microclima e a umidade do solo sem sair do campo. "
        "Atualizamos os dados automaticamente a cada período de amostragem configurado.</p>"
        "<p class='info-text'>A tabela abaixo resume as leituras capturadas pelo sensor instalado. "
        "Use-a para decidir rapidamente sobre irrigação, ventilação ou manutenção.</p>"
        "<table>"
        "<tr><td>Temp Ar:</td><td>%.2f &deg;C</td></tr>"
        "<tr><td>UR Ar:</td><td>%.2f %%</td></tr>"
        "<tr><td>Temp Solo:</td><td>%.2f &deg;C</td></tr>"
        "<tr><td>Umid Solo Atual:</td><td>%.2f %% (raw=%d)</td></tr>"
        "<tr><td>Calibra&ccedil;&atilde;o:</td><td>seco=%.0f molhado=%.0f</td></tr>"
        "</table>"
        "<div style='display:flex;gap:12px;flex-wrap:wrap;margin-top:8px'>"
        "  <a class='button' href='/' style='background:#388e3c'>Voltar ao painel principal</a>"
        "</div>"
        "</div>"

        "<div class='card'>"
        "<h2>Hist&oacute;rico recente</h2>"
        "<p class='info-text'>Os gráficos apresentam a evolução das últimas leituras. "
        "Observe tendências para planejar ações e validar o desempenho do greenSe Campo.</p>"
        "<div class='grid'>"
        "<div><canvas id='chart_temp_ar' width='320' height='180'></canvas></div>"
        "<div><canvas id='chart_umid_ar' width='320' height='180'></canvas></div>"
        "<div><canvas id='chart_temp_solo' width='320' height='180'></canvas></div>"
        "<div><canvas id='chart_umid_solo' width='320' height='180'></canvas></div>"
        "</div>"
        "<p class='caption'>Exibindo at&eacute; 10 &uacute;ltimos pontos.</p>"
        "</div>"

        /* Chart mínimo inline. Só inclua este bloco se você NÃO
           já estiver servindo Chart.js de CDN. Se você já tem Chart.js
           carregado em outro lugar, remova este bloco inteiro. */
        "<script>"
        "!function(){"
        "function Chart(c,o){this.ctx=c;this.data=o.data;this.type=o.type;this.options=o.options||{};this._init()}"
        "Chart.prototype._init=function(){this._draw()};"
        "Chart.prototype.update=function(){this._draw()};"
        "Chart.prototype._draw=function(){"
        " const ctx=this.ctx;"
        " const w=ctx.canvas.width;const h=ctx.canvas.height;"
        " ctx.clearRect(0,0,w,h);"
        " ctx.strokeStyle='#000';ctx.lineWidth=1;"
        " ctx.strokeRect(0,0,w,h);"
        " const labels=this.data.labels||[];"
        " const vals=(this.data.datasets&&this.data.datasets[0]&&this.data.datasets[0].data)||[];"
        " const lbl=(this.data.datasets&&this.data.datasets[0]&&this.data.datasets[0].label)||'';"
        " let minVal=vals.length?Math.min.apply(null,vals):0;"
        " let maxVal=vals.length?Math.max.apply(null,vals):1;"
        " if(isFinite(this.options.fixedMin)) minVal=this.options.fixedMin;"
        " if(isFinite(this.options.fixedMax)) maxVal=this.options.fixedMax;"
        " if(minVal===maxVal){maxVal=minVal+1;}"
        " const range=(maxVal-minVal)||1;"
        " const ticks=4;"
        " ctx.strokeStyle='#ccc';"
        " ctx.fillStyle='#555';"
        " ctx.font='9px sans-serif';"
        " for(let t=0;t<=ticks;t++){"
        "   const value=minVal+(range/ticks)*t;"
        "   const y=h-((value-minVal)/range)*h;"
        "   ctx.beginPath();"
        "   ctx.moveTo(0,y);ctx.lineTo(6,y);"
        "   ctx.stroke();"
        "   ctx.fillText(value.toFixed(0),8,y-2);"
        " }"
        " ctx.beginPath();ctx.strokeStyle='#1976d2';"
        " if(vals.length===0){ctx.moveTo(0,h);ctx.lineTo(w,h);}"
        " for(let i=0;i<vals.length;i++){"
        "   const clamped=Math.min(Math.max(vals[i],minVal),maxVal);"
        "   const x=(i/(vals.length-1||1))*w;"
        "   const y=h-((clamped-minVal)/range)*h;"
        "   if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);"
        " }"
        " ctx.stroke();"
        " ctx.fillStyle='#000';ctx.font='10px sans-serif';"
        " ctx.fillText(lbl,4,12);"
        "};"
        "window.Chart=Chart;"
        "}();"
        "</script>"

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

        "  desenha('chart_temp_ar','Temp Ar (C)',tAr.xs,tAr.ys,20,40);"
        "  desenha('chart_umid_ar','UR Ar (%%)',uAr.xs,uAr.ys,0,100);"
        "  desenha('chart_temp_solo','Temp Solo (C)',tSolo.xs,tSolo.ys,20,40);"
        "  desenha('chart_umid_solo','Umid Solo (%%)',uSolo.xs,uSolo.ys,0,100);"
        " }catch(e){console.log('erro /history',e);}"
        "}"

        "function desenha(id,titulo,labels,data,minY,maxY){"
        "  const ctx=document.getElementById(id).getContext('2d');"
        "  if(!ctx._chartRef){"
        "    ctx._chartRef=new Chart(ctx,{"
        "      type:'line',"
        "      data:{labels:labels,datasets:[{label:titulo,data:data,fill:false}]},"
        "      options:{fixedMin:minY,fixedMax:maxY}"
        "    });"
        "  }else{"
        "    ctx._chartRef.data.labels=labels;"
        "    ctx._chartRef.data.datasets[0].data=data;"
        "    ctx._chartRef.data.datasets[0].label=titulo;"
        "    ctx._chartRef.options.fixedMin=minY;"
        "    ctx._chartRef.options.fixedMax=maxY;"
        "    ctx._chartRef.update();"
        "  }"
        "}"

        "setInterval(atualiza,5000);"
        "atualiza();"
        "</script>"

        "</div>"
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
    esp_err_t ret = httpd_resp_send(req, page, len);
    /* Libera memória após enviar resposta */
    if (page) {
        free(page);
    }
    return ret;
}

 
/* /history -> últimos pontos em JSON */
static esp_err_t handle_history(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
    }
    
    char *json = svc->build_history_json();
    if (!json) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
    }
 
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ret;
}

/* Página de configurações */
static esp_err_t handle_config(httpd_req_t *req)
{
    const char *page =
        "<!DOCTYPE html>"
        "<html><head><meta charset='utf-8'/>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
        "<title>Painel greenSe Campo</title>"
        "<style>"
        "body{font-family:'Inter',sans-serif;background:#edf3ee;color:#1f2a24;margin:0;padding:32px}"
        ".wrapper{max-width:720px;margin:auto}"
        ".card{background:#fff;border-radius:20px;padding:28px;box-shadow:0 20px 40px rgba(0,0,0,0.1)}"
        ".tag{display:inline-block;padding:4px 12px;border-radius:999px;background:#c8e6c9;color:#1b5e20;font-weight:600;font-size:12px}"
        "h1{margin:12px 0 8px;font-size:28px;color:#1b5e20}"
        ".lead{color:#4f5b62;margin-bottom:24px;line-height:1.5}"
        ".actions{display:flex;flex-direction:column;gap:18px}"
        ".action{background:#f7faf6;border-radius:16px;padding:18px 20px;border:1px solid #e0e8e1}"
        ".action p{margin:10px 0 0;font-size:13px;color:#546e7a}"
        "a.button,button{display:inline-flex;align-items:center;justify-content:center;padding:12px 18px;border:none;border-radius:12px;font-weight:600;color:#fff;text-decoration:none;box-shadow:0 10px 24px rgba(56,142,60,0.25)}"
        "a.button{background:#388e3c}"
        "a.button.secondary{background:#1976d2}"
        "a.button.neutral{background:#455a64}"
        "button.delete{background:#c62828;width:100%}"
        ".footer-note{margin-top:28px;font-size:12px;color:#78909c}"
        "</style>"
        "<script>"
        "function confirmaApagar(){"
        "  if(!confirm('Deseja realmente apagar os dados gravados?')){return false;}"
        "  document.getElementById('clearForm').submit();"
        "  return false;"
        "}"
        "</script>"
        "</head><body>"
        "<div class='wrapper'>"
        "<div class='card'>"
        "<span class='tag'>greenSe Campo</span>"
        "<h1>Painel principal</h1>"
        "<p class='lead'>Selecione uma das ações para administrar seu sensor. "
        "Use este painel para centralizar monitoramento, calibrações e manutenção de dados.</p>"
        "<div class='actions'>"
        "<div class='action'>"
        "<a class='button' href='/dashboard'>Ver dashboard</a>"
        "<p>Acompanhe gráficos, históricos e um resumo das leituras mais recentes.</p>"
        "</div>"
        "<div class='action'>"
        "<a class='button secondary' href='/sampling'>Período de amostragem</a>"
        "<p>Defina de quanto em quanto tempo o greenSe Campo deve registrar novas amostras.</p>"
        "</div>"
        "<div class='action'>"
        "<a class='button secondary' href='/calibra'>Calibrar umidade do solo</a>"
        "<p>Atualize o ponto seco e molhado para manter a leitura de solo mais precisa.</p>"
        "</div>"
        "<div class='action'>"
        "<a class='button neutral' href='/download'>Baixar log em CSV</a>"
        "<p>Exporte todo o histórico e analise com mais detalhes em ferramentas externas.</p>"
        "</div>"
        "<div class='action'>"
        "<form id='clearForm' method='post' action='/clear_data' onsubmit='return confirmaApagar();'>"
        "<button class='delete' type='submit'>Apagar dados gravados</button>"
        "<p>Limpa o log e reinicia o armazenamento. Útil para novos testes de campo.</p>"
        "</form>"
        "</div>"
        "</div>"
        "<p class='footer-note'>greenSe Campo | Tecnologia desenhada para agricultura conectada.</p>"
        "</div>"
        "</div>"
        "</body></html>";

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_sampling_page(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL || svc->get_sampling_period_ms == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Serviços não disponíveis");
        return ESP_FAIL;
    }

    uint32_t current_ms = svc->get_sampling_period_ms();
    const sampling_option_def_t *opt = find_sampling_option(current_ms);

    char current_label[64];
    if (opt) {
        snprintf(current_label, sizeof(current_label), "%s", opt->label);
    } else {
        snprintf(current_label, sizeof(current_label), "%lu ms", (unsigned long)current_ms);
    }

    char radios[1024] = {0};
    size_t used = 0;
    for (size_t i = 0; i < SAMPLING_OPTION_COUNT && used < sizeof(radios) - 1; ++i) {
        const sampling_option_def_t *item = &SAMPLING_OPTIONS[i];
        int written = snprintf(
            radios + used,
            sizeof(radios) - used,
            "<label class='option'>"
            "<input type='radio' name='periodo' value='%lu' %s>"
            "<span>%s</span>"
            "</label>",
            (unsigned long)item->ms,
            (current_ms == item->ms) ? "checked" : "",
            item->label
        );
        if (written < 0) {
            break;
        }
        used += (size_t)written;
    }

    const size_t page_capacity = 4096;
    char *page = malloc(page_capacity);
    if (!page) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro de memória");
        return ESP_FAIL;
    }

    int len = snprintf(
        page,
        page_capacity,
        "<!DOCTYPE html>"
        "<html><head><meta charset='utf-8'/>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
        "<title>Período de amostragem</title>"
        "<style>"
        "body{font-family:'Inter',sans-serif;background:#f0f4f1;color:#1f2a24;margin:0;padding:28px}"
        ".card{background:#fff;border-radius:20px;padding:26px;max-width:520px;margin:auto;"
        "box-shadow:0 22px 40px rgba(0,0,0,0.12)}"
        ".tag{display:inline-block;padding:4px 12px;border-radius:999px;background:#c8e6c9;"
        "color:#1b5e20;font-size:12px;font-weight:600}"
        "h1{margin:10px 0 6px;font-size:26px;color:#1b5e20}"
        ".lead{color:#4f5b62;line-height:1.5;margin-bottom:18px}"
        ".options{display:flex;flex-direction:column;gap:12px;margin:16px 0}"
        ".option{display:flex;align-items:center;gap:10px;font-size:15px;background:#f7faf6;"
        "border-radius:12px;padding:10px 14px;border:1px solid #e0e8e1}"
        "button{padding:12px 18px;border:none;border-radius:12px;background:#388e3c;"
        "color:#fff;font-size:15px;font-weight:600;cursor:pointer;width:100%%;"
        "box-shadow:0 10px 24px rgba(56,142,60,0.25);box-sizing:border-box}"
        "p.hint{font-size:13px;color:#546e7a;margin-top:12px}"
        "a{color:#1976d2;text-decoration:none}"
        ".button-back{display:block;padding:12px 18px;border-radius:12px;"
        "background:#388e3c;color:#fff;text-decoration:none;text-align:center;"
        "border:1px solid #2e7d32;width:100%%;margin-top:16px;font-weight:600;"
        "box-shadow:0 10px 24px rgba(56,142,60,0.25);box-sizing:border-box}"
        "</style>"
        "</head><body>"
        "<div class='card'>"
        "<span class='tag'>greenSe Campo</span>"
        "<h1>Período de amostragem</h1>"
        "<p class='lead'>Selecione com que frequência uma nova amostra será registrada. "
        "Períodos menores deixam o dashboard mais responsivo; períodos maiores economizam energia.</p>"
        "<form action='/set_sampling' method='get'>"
        "<div class='options'>%s</div>"
        "<button type='submit'>Salvar período</button>"
        "</form>"
        "<p class='hint'>Período atual: <strong>%s</strong>. Sua escolha é salva no dispositivo e aplicada imediatamente.</p>"
        "<a class='button-back' href='/'>Voltar ao painel principal</a>"
        "</div>"
        "</body></html>",
        radios,
        current_label
    );

    if (len < 0 || len >= (int)page_capacity) {
        free(page);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro gerando página");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    esp_err_t resp = httpd_resp_send(req, page, len);
    free(page);
    return resp;
}

static esp_err_t handle_set_sampling(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL || svc->set_sampling_period_ms == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Serviços não disponíveis");
        return ESP_FAIL;
    }

    char qs[64];
    if (httpd_req_get_url_query_str(req, qs, sizeof(qs)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Parâmetro ausente");
        return ESP_FAIL;
    }

    char buf_period[32];
    if (httpd_query_key_value(qs, "periodo", buf_period, sizeof(buf_period)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "periodo inválido");
        return ESP_FAIL;
    }

    uint32_t new_period = (uint32_t)strtoul(buf_period, NULL, 10);
    if (svc->set_sampling_period_ms(new_period) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Valor não suportado");
        return ESP_FAIL;
    }

    const sampling_option_def_t *opt = find_sampling_option(new_period);
    char label[64];
    if (opt) {
        snprintf(label, sizeof(label), "%s", opt->label);
    } else {
        snprintf(label, sizeof(label), "%lu ms", (unsigned long)new_period);
    }

    const size_t page_capacity = 512;
    char page[page_capacity];
    int len = snprintf(
        page,
        page_capacity,
        "<!DOCTYPE html><html><body>"
        "<p>Período atualizado para <strong>%s</strong>.</p>"
        "<p><a href='/sampling'>Voltar</a></p>"
        "</body></html>",
        label
    );

    if (len < 0 || len >= (int)page_capacity) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro gerando resposta");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, page, len);
}

/* /download -> CSV inteiro */
static esp_err_t handle_download(httpd_req_t *req)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/log_temp.csv", BSP_SPIFFS_MOUNT);
    
    FILE *f = fopen(filepath, "r");
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
    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        if (httpd_resp_sendstr_chunk(req, buf) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }
    fclose(f);
    httpd_resp_sendstr_chunk(req, NULL); /* fim chunked */
    return ESP_OK;
}

/* /calibra -> página HTML com calibração atual e leitura atual */
static esp_err_t handle_calibra(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Serviços não disponíveis");
        return ESP_FAIL;
    }
    
    float seco, molhado;
    svc->get_calibration(&seco, &molhado);
 
    int leitura_raw = svc->get_soil_raw();
 
    const size_t page_capacity = 4096;
    char *page = malloc(page_capacity);
    if (!page) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro de memória");
        return ESP_FAIL;
    }

    int len = snprintf(page, page_capacity,
             "<!DOCTYPE html><html><head>"
             "<meta charset='utf-8'/>"
             "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
             "<title>Calibração greenSe Campo</title>"
             "<style>"
             "body{font-family:'Inter',sans-serif;background:#f0f4f1;color:#1f2a24;margin:0;padding:24px;}"
             ".card{background:#fff;border-radius:20px;padding:26px;max-width:520px;margin:auto;"
             "box-shadow:0 24px 46px rgba(0,0,0,0.12)}"
             ".tag{display:inline-block;padding:4px 12px;border-radius:999px;background:#c8e6c9;"
             "color:#1b5e20;font-size:12px;font-weight:600}"
             "h2{margin:12px 0 6px;font-size:24px;color:#1b5e20}"
             ".lead{color:#4f5b62;line-height:1.5;margin-bottom:18px}"
             ".info{display:flex;gap:12px;margin:18px 0}"
             ".info-box{flex:1;background:#f7faf6;border:1px solid #e0e8e1;border-radius:14px;"
             "padding:12px 14px;text-align:center;font-size:14px}"
             "form label{display:block;margin:12px 0 4px;font-weight:600;font-size:14px;color:#37474f}"
             "input{width:100%%;padding:10px;border:1px solid #cddbd2;border-radius:10px;"
             "font-size:15px}"
             "button{width:100%%;padding:12px 18px;border:none;border-radius:12px;background:#388e3c;"
             "color:#fff;font-size:15px;font-weight:600;margin-top:18px;box-shadow:0 10px 24px rgba(56,142,60,0.25)}"
             "a.button{display:inline-block;width:100%%;text-align:center;padding:12px 18px;"
             "border-radius:12px;background:#388e3c;color:#fff;text-decoration:none;font-weight:600;"
             "margin-top:12px;border:1px solid #2e7d32;box-shadow:0 8px 20px rgba(56,142,60,0.25);"
             "box-sizing:border-box}"
             ".tip{font-size:13px;color:#546e7a;margin-top:14px}"
             "</style></head><body>"
             "<div class='card'>"
             "<span class='tag'>greenSe Campo</span>"
             "<h2>Calibração da umidade do solo</h2>"
             "<p class='lead'>Informe novos valores para representar o solo seco e totalmente molhado. "
             "Isso garante que o percentual exibido pelo greenSe Campo corresponda à realidade do seu canteiro.</p>"
             "<div class='info'>"
             "<div class='info-box'><strong>Leitura bruta</strong><br>%d</div>"
             "<div class='info-box'><strong>Faixa atual</strong><br>Seco %.0f | Molhado %.0f</div>"
             "</div>"
             "<form action='/set_calibra' method='get'>"
             "<label>Novo valor para solo seco</label>"
             "<input type='number' name='seco' value='%.0f'>"
             "<label>Novo valor para solo molhado</label>"
             "<input type='number' name='molhado' value='%.0f'>"
             "<button type='submit'>Salvar calibração</button>"
             "</form>"
             "<p class='tip'>Dica: realize a calibração logo após irrigar e depois de um período de seca para captar os extremos.</p>"
             "<a class='button' href='/'>Voltar ao painel principal</a>"
             "</div>"
             "</body></html>",
             leitura_raw,
             seco,
             molhado,
             seco,
             molhado);
 
    if (len < 0 || len >= (int)page_capacity) {
        free(page);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro gerando página");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    esp_err_t resp = httpd_resp_send(req, page, len);
    free(page);
    return resp;
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
 
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Serviços não disponíveis");
        return ESP_FAIL;
    }
    
    if (svc->set_calibration(novo_seco, novo_molhado) != ESP_OK)
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

/* Endpoint para limpar dados via GUI */
static esp_err_t handle_clear_data(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL || svc->clear_logged_data == NULL) {
        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Serviço indisponível");
        return ESP_FAIL;
    }

    /* descarta payload (se houver) */
    int remaining = req->content_len;
    char buf[64];
    while (remaining > 0) {
        int to_read = remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf);
        int received = httpd_req_recv(req, buf, to_read);
        if (received <= 0) {
            httpd_resp_send_err(req,
                                HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Falha ao receber requisição");
            return ESP_FAIL;
        }
        remaining -= received;
    }

    if (svc->clear_logged_data() != ESP_OK) {
        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Não foi possível apagar os dados");
        return ESP_FAIL;
    }

    const char *resp =
        "<!DOCTYPE html><html><head><meta charset='utf-8'/>"
        "<meta http-equiv='refresh' content='2; url=/config'/>"
        "<title>Dados apagados</title></head>"
        "<body><p>Dados apagados com sucesso. Você será redirecionado "
        "para a página de configurações.</p></body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}


 
/* -------------------------------------------------------------------------- */
/* Inicialização/parada do servidor HTTP                                      */
/* -------------------------------------------------------------------------- */
 
esp_err_t http_server_start(void)
{
    if (server_handle != NULL) {
        return ESP_OK;
    }
 
    /* Inicializa Wi-Fi AP sem callbacks (serão registrados por app_main) */
    ESP_ERROR_CHECK(wifi_ap_init());
 
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Robustez - stack maior para evitar overflow */
    config.stack_size        = 16384;  /* aumentado para evitar stack overflow */
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    config.max_open_sockets  = 4;
    config.lru_purge_enable  = true;
    config.max_uri_handlers  = 12;    /* garante espaço para todos os handlers */
 
    if (httpd_start(&server_handle, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar httpd");
        return ESP_FAIL;
    }
 
    /* registrar rotas principais */
    httpd_uri_t uri_root = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = handle_config,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_root) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /");
        return ESP_FAIL;
    }
 
    httpd_uri_t uri_hist = {
        .uri      = "/history",
        .method   = HTTP_GET,
        .handler  = handle_history,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_hist) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /history");
        return ESP_FAIL;
    }

    httpd_uri_t uri_dashboard = {
        .uri      = "/dashboard",
        .method   = HTTP_GET,
        .handler  = handle_dashboard,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_dashboard) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /dashboard");
        return ESP_FAIL;
    }
 
    httpd_uri_t uri_config = {
        .uri      = "/config",
        .method   = HTTP_GET,
        .handler  = handle_config,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_config) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /config");
        return ESP_FAIL;
    }

    httpd_uri_t uri_sampling = {
        .uri      = "/sampling",
        .method   = HTTP_GET,
        .handler  = handle_sampling_page,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_sampling) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /sampling");
        return ESP_FAIL;
    }

    httpd_uri_t uri_sampling_set = {
        .uri      = "/set_sampling",
        .method   = HTTP_GET,
        .handler  = handle_set_sampling,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_sampling_set) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /set_sampling");
        return ESP_FAIL;
    }

    httpd_uri_t uri_down = {
        .uri      = "/download",
        .method   = HTTP_GET,
        .handler  = handle_download,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_down) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /download");
        return ESP_FAIL;
    }

    httpd_uri_t uri_cal = {
        .uri      = "/calibra",
        .method   = HTTP_GET,
        .handler  = handle_calibra,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_cal) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /calibra");
        return ESP_FAIL;
    }

    httpd_uri_t uri_set = {
        .uri      = "/set_calibra",
        .method   = HTTP_GET,
        .handler  = handle_set_calibra,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_set) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /set_calibra");
        return ESP_FAIL;
    }

    httpd_uri_t uri_clear = {
        .uri      = "/clear_data",
        .method   = HTTP_POST,
        .handler  = handle_clear_data,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_clear) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /clear_data");
        return ESP_FAIL;
    }

    /* favicon padrao */
    httpd_uri_t uri_icon = {
        .uri      = "/favicon.ico",
        .method   = HTTP_GET,
        .handler  = handle_favicon,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_icon) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /favicon.ico");
        return ESP_FAIL;
    }

    /* rota de teste captive portal Android */
    httpd_uri_t uri_gen204 = {
        .uri      = "/generate_204",
        .method   = HTTP_GET,
        .handler  = handle_generate204,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_gen204) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /generate_204");
        return ESP_FAIL;
    }
 
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

