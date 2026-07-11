#include "web_server.h"
#include "esp_camera.h"
#include <WiFi.h>

CameraWebServer* CameraWebServer::_inst = nullptr;

// 全局帧计数器 (由流线程递增, 状态查询读取)
static volatile uint32_t g_frame_count = 0;

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>ESP32-CAM · 监控终端</title>
<style>
:root{--bg:#0b0d12;--card:#12151c;--accent:#3b82f6;--text:#c8ccd4;--muted:#4d5363;--bd:#1a1e26}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'SF Mono','Cascadia Code','Noto Sans SC','Segoe UI',monospace,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;font-size:13px;line-height:1.5}
::selection{background:var(--accent);color:#fff}
.bar{display:flex;align-items:center;justify-content:space-between;height:38px;padding:0 14px;background:var(--card);border-bottom:1px solid var(--bd);position:sticky;top:0;z-index:10;gap:10px}
.bar h1{font-size:.75rem;font-weight:500;color:var(--muted);letter-spacing:.12em;text-transform:uppercase}
.bar h1 b{color:var(--accent);font-weight:600}
.st{display:flex;align-items:center;gap:8px;font-size:.68rem;color:var(--muted)}
.dot{width:3px;height:3px;background:#22c55e;animation:p 1.6s infinite}
@keyframes p{0%,100%{opacity:1}50%{opacity:.12}}
.w{max-width:960px;margin:0 auto;padding:10px}
.v{border:1px solid var(--bd);background:#000;position:relative}
.v img{width:100%;display:block;background:#000;aspect-ratio:4/3;object-fit:contain}
.ov{position:absolute;bottom:0;left:0;right:0;height:34px;background:rgba(12,14,18,.88);border-top:1px solid rgba(255,255,255,.06);display:flex;align-items:center;padding:0 8px;gap:4px}
.btn{height:24px;padding:0 10px;border:none;font-family:inherit;font-size:.65rem;font-weight:500;cursor:pointer;display:inline-flex;align-items:center;gap:3px;text-transform:uppercase;letter-spacing:.05em;transition:all .1s}
.bp{background:var(--accent);color:#fff}.bp:hover{background:#2563eb}
.sec{margin-top:12px}
.st2{font-size:.6rem;color:var(--muted);text-transform:uppercase;letter-spacing:.1em;margin-bottom:6px;padding-left:1px;font-weight:500}
.g{display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:1px;border:1px solid var(--bd);background:var(--bd)}
.cd{background:var(--card);padding:10px 12px}
.cd h3{font-size:.58rem;color:var(--muted);text-transform:uppercase;letter-spacing:.08em;margin-bottom:8px;font-weight:600}
.rw{display:flex;align-items:center;gap:4px;margin-bottom:4px}
.rw label{font-size:.7rem;color:var(--text);min-width:40px;flex-shrink:0;opacity:.7}
.rw input[type=range]{flex:1;height:1.5px;-webkit-appearance:none;background:var(--bd);outline:none;min-width:0}
.rw input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:4px;height:12px;background:var(--accent);cursor:pointer;border:none}
.rw .vl{font-size:.62rem;color:var(--muted);min-width:20px;text-align:right;font-variant-numeric:tabular-nums}
.rw select{background:var(--bg);color:var(--text);border:1px solid var(--bd);padding:1px 4px;font-family:inherit;font-size:.65rem;cursor:pointer;outline:none;flex:1;max-width:90px;text-transform:uppercase}
.rw select:focus{border-color:var(--accent)}
.rg{display:flex;gap:1px;flex-wrap:wrap;border:1px solid var(--bd)}
.rg .btn{height:24px;padding:0 10px;background:var(--card);color:var(--muted);border:none;font-family:inherit;font-size:.62rem;cursor:pointer;text-transform:uppercase;letter-spacing:.06em}
.rg .btn.on{background:var(--accent);color:#fff}
.rg .btn:not(.on):hover{background:#1a1e28}
.ft{display:flex;gap:14px;flex-wrap:wrap;font-size:.6rem;color:var(--muted);padding:6px 2px 0;margin-top:6px;border-top:1px solid var(--bd)}
.ts{position:fixed;bottom:12px;right:12px;background:var(--card);color:var(--text);padding:6px 12px;border:1px solid var(--accent);transform:translateY(50px);opacity:0;transition:.2s;z-index:99;font-size:.68rem;pointer-events:none;font-family:inherit}
.ts.s{transform:translateY(0);opacity:1}
@media(max-width:500px){.g{grid-template-columns:1fr}.ov .btn{font-size:.6rem;padding:0 6px}}
</style>
</head>
<body>

<div class="bar">
<h1><b>ESP32</b>-CAM</h1>
<div class="st"><span class="dot"></span><span id="fps">--</span><span id="res">--</span></div>
</div>

<div class="w">

<div class="v">
<div style="position:relative">
<img id="stream" alt="camera" style="width:100%;display:block;background:#000;aspect-ratio:4/3">
</div>
<div class="ov">
<button class="btn bp" onclick="snap()">📸 拍照</button>
</div>
</div>

<div class="sec"><div class="st2">分辨率</div>
<div class="rg">
<button class="btn" id="rb2" onclick="sr(2)">QVGA</button>
<button class="btn" id="rb4" onclick="sr(4)">VGA</button>
<button class="btn" id="rb5" onclick="sr(5)">SVGA</button>
<button class="btn on" id="rb8" onclick="sr(8)">UXGA</button>
</div></div>

<div class="sec"><div class="st2">图像调节</div>
<div class="g">

<div class="cd"><h3>基本</h3>
<div class="rw"><label>质量</label><input type="range" id="quality" min="6" max="63" value="6" oninput="upd('quality',this.value)"><span class="vl" id="quality-v">6</span></div>
<div class="rw"><label>亮度</label><input type="range" id="brightness" min="-2" max="2" value="0" step="1" oninput="upd('brightness',this.value)"><span class="vl" id="brightness-v">0</span></div>
<div class="rw"><label>对比度</label><input type="range" id="contrast" min="-2" max="2" value="0" step="1" oninput="upd('contrast',this.value)"><span class="vl" id="contrast-v">0</span></div>
<div class="rw"><label>饱和度</label><input type="range" id="saturation" min="-2" max="2" value="0" step="1" oninput="upd('saturation',this.value)"><span class="vl" id="saturation-v">0</span></div>
<div class="rw"><label>锐度</label><input type="range" id="sharpness" min="-3" max="3" value="0" step="1" oninput="upd('sharpness',this.value)"><span class="vl" id="sharpness-v">0</span></div>
</div>

<div class="cd"><h3>曝光</h3>
<div class="rw"><label>补偿</label><input type="range" id="ae_level" min="-2" max="2" value="0" step="1" oninput="upd('ae_level',this.value)"><span class="vl" id="ae_level-v">0</span></div>
<div class="rw"><label>曝光</label><select id="aec_value" onchange="upd('aec_value',this.value)"><option value="0">自动</option><option value="300">300</option><option value="600">600</option><option value="900">900</option></select></div>
<div class="rw"><label>AWB</label><select id="awb_gain" onchange="upd('awb_gain',this.value)"><option value="1">开</option><option value="0">关</option></select></div>
<div class="rw"><label>白平衡</label><select id="wb_mode" onchange="upd('wb_mode',this.value)"><option value="0">自动</option><option value="1">日光</option><option value="2">阴天</option><option value="3">荧光</option><option value="4">白炽</option></select></div>
<div class="rw"><label>增益</label><input type="range" id="agc_gain" min="0" max="30" value="5" oninput="upd('agc_gain',this.value)"><span class="vl" id="agc_gain-v">5</span></div>
</div>

<div class="cd"><h3>翻转</h3>
<div class="rw"><label>镜像</label><select id="hmirror" onchange="upd('hmirror',this.value)"><option value="1">开</option><option value="0">关</option></select></div>
<div class="rw"><label>翻转</label><select id="vflip" onchange="upd('vflip',this.value)"><option value="0">关</option><option value="1">开</option></select></div>
<div class="rw"><label>降噪</label><select id="denoise" onchange="upd('denoise',this.value)"><option value="0">关</option><option value="1">开</option></select></div>
</div>

</div></div>

<div class="ft">
<span>IP: <span id="sip">--</span></span>
<span>信号: <span id="srssi">--</span></span>
<span>CPU: <span id="scpu">--</span></span>
<span>温度: <span id="stemp">--</span></span>
<span>PSRAM: <span id="spsram">--</span></span>
<span>内存: <span id="sram">--</span></span>
<span>运行: <span id="sup">--</span></span>
</div>
</div>

<div class="ts" id="ts"></div>

<script>
var S=location.hostname,IM=document.getElementById('stream'),TS=document.getElementById('ts'),SP='http://'+S+':81/stream'
document.addEventListener('DOMContentLoaded',function(){IM.src=SP+'?'+Date.now();P();setInterval(P,2000)})
function snap(){var a=document.createElement('a');a.download='cam_'+Date.now()+'.jpg';a.href='/capture?'+Date.now();a.click();T('已保存')}
function upd(k,v){var e=document.getElementById(k+'-v');if(e)e.textContent=v;fetch('/set?'+k+'='+v).then(function(r){return r.json()}).then(function(d){if(d.error)T('失败')})}
function sr(r){var b=document.querySelectorAll('.rg .btn');for(var i=0;i<b.length;i++)b[i].classList.remove('on');document.getElementById('rb'+r).classList.add('on');fetch('/set?framesize='+r).then(function(){IM.src=SP+'?'+Date.now();T('已切换')})}
function T(m){TS.textContent=m;TS.className='ts s';setTimeout(function(){TS.className='ts'},2500)}
function P(){fetch('/status').then(function(r){return r.json()}).then(function(d){if(d.fps)document.getElementById('fps').textContent=d.fps+'FPS';if(d.resolution)document.getElementById('res').textContent=d.resolution;if(d.ip)document.getElementById('sip').textContent=d.ip;if(d.rssi)document.getElementById('srssi').textContent=d.rssi+'dBm';if(d.cpu)document.getElementById('scpu').textContent=d.cpu;if(d.temp)document.getElementById('stemp').textContent=d.temp;if(d.psram)document.getElementById('spsram').textContent=d.psram;if(d.free_heap)document.getElementById('sram').textContent=(d.free_heap/1024).toFixed(0)+'KB';if(d.uptime)document.getElementById('sup').textContent=d.uptime;if(d.settings){Object.keys(d.settings).forEach(function(k){var el=document.getElementById(k),vl=document.getElementById(k+'-v');if(el&&d.settings[k]!==undefined){el.value=d.settings[k];if(vl)vl.textContent=d.settings[k]}})}})}
</script>
</body>
</html>
)rawliteral";

// =====================  Server Implementation  =====================

CameraWebServer::CameraWebServer()
    : _ctrl_srv(nullptr), _stream_srv(nullptr), _wifiCb(nullptr)
{
    _inst = this;
    _settings = { FRAMESIZE_UXGA,6, 0,0,0,0,0, 0,1,0, 0,5, 1,0 };
}

CameraWebServer::~CameraWebServer() {
    if (_ctrl_srv)  httpd_stop(_ctrl_srv);
    if (_stream_srv && _stream_srv != _ctrl_srv) httpd_stop(_stream_srv);
    if (_inst == this) _inst = nullptr;
}

void CameraWebServer::onWiFiConfig(std::function<void(const char*, const char*)> cb) {
    _wifiCb = cb;
}

void CameraWebServer::begin(int port) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = port;
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 12;
    cfg.stack_size = 8192;

    auto reg = [&](httpd_handle_t srv, const char* path, httpd_method_t method, esp_err_t (*handler)(httpd_req_t*)) {
        httpd_uri_t u = { .uri = path, .method = method, .handler = handler, .user_ctx = this };
        httpd_register_uri_handler(srv, &u);
    };

    if (httpd_start(&_ctrl_srv, &cfg) == ESP_OK) {
        reg(_ctrl_srv, "/",         HTTP_GET, _root_h);
        reg(_ctrl_srv, "/capture",  HTTP_GET, _capture_h);
        reg(_ctrl_srv, "/set",      HTTP_GET, _set_h);
        reg(_ctrl_srv, "/status",   HTTP_GET, _status_h);
        Serial.printf("[HTTP] 控制 :%d\n", port);
    } else {
        Serial.println("[HTTP] 控制服务器失败");
    }

    httpd_config_t scfg = HTTPD_DEFAULT_CONFIG();
    scfg.server_port = port + 1;
    scfg.ctrl_port = 32769;
    scfg.stack_size = 16384;
    scfg.max_uri_handlers = 4;

    if (httpd_start(&_stream_srv, &scfg) == ESP_OK) {
        reg(_stream_srv, "/stream", HTTP_GET, _stream_h);
        Serial.printf("[HTTP] 流 :%d\n", port + 1);
    } else {
        Serial.printf("[HTTP] 流 :%d 失败, 合并\n", port + 1);
        _stream_srv = _ctrl_srv;
        reg(_stream_srv, "/stream", HTTP_GET, _stream_h);
    }

    applySettings();
    Serial.printf("[Web] http://%s:%d  | 流 http://%s:%d\n",
        WiFi.localIP().toString().c_str(), port,
        WiFi.localIP().toString().c_str(), port + 1);
}

// Static fwd
esp_err_t CameraWebServer::_root_h(httpd_req_t *r)    { return _inst ? _inst->handleRoot(r) : ESP_FAIL; }
esp_err_t CameraWebServer::_stream_h(httpd_req_t *r)  { return _inst ? _inst->handleStream(r) : ESP_FAIL; }
esp_err_t CameraWebServer::_capture_h(httpd_req_t *r) { return _inst ? _inst->handleCapture(r) : ESP_FAIL; }
esp_err_t CameraWebServer::_set_h(httpd_req_t *r)     { return _inst ? _inst->handleSet(r) : ESP_FAIL; }
esp_err_t CameraWebServer::_status_h(httpd_req_t *r)  { return _inst ? _inst->handleStatus(r) : ESP_FAIL; }
esp_err_t CameraWebServer::handleRoot(httpd_req_t *r) {
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    httpd_resp_sendstr(r, INDEX_HTML);
    return ESP_OK;
}

esp_err_t CameraWebServer::handleCapture(httpd_req_t *r) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) { httpd_resp_sendstr(r, "{}"); return ESP_FAIL; }
    httpd_resp_set_type(r, "image/jpeg");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    esp_err_t ret = httpd_resp_send(r, (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return ret;
}

esp_err_t CameraWebServer::handleStream(httpd_req_t *r) {
    httpd_resp_set_type(r, "multipart/x-mixed-replace; boundary=frame");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    httpd_resp_set_hdr(r, "Access-Control-Allow-Origin", "*");
    esp_err_t res;
    while (true) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) { delay(5); continue; }
        g_frame_count++;
        char hdr[160];
        int hlen = snprintf(hdr, sizeof(hdr),
            "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
        res = httpd_resp_send_chunk(r, hdr, hlen);
        if (res != ESP_OK) break;
        res = httpd_resp_send_chunk(r, (const char*)fb->buf, fb->len);
        if (res != ESP_OK) break;
        res = httpd_resp_send_chunk(r, "\r\n", 2);
        esp_camera_fb_return(fb);
        if (res != ESP_OK) break;
        delay(5);
    }
    return ESP_OK;
}

esp_err_t CameraWebServer::handleSet(httpd_req_t *r) {
    bool chg = false;
    size_t bl = httpd_req_get_url_query_len(r) + 1;
    if (bl > 1) {
        char *buf = (char*)malloc(bl);
        if (httpd_req_get_url_query_str(r, buf, bl) == ESP_OK) {
            char v[32];
            #define Q(k,f) if(httpd_query_key_value(buf,#k,v,sizeof(v))==ESP_OK){_settings.f=atoi(v);chg=true;}
            Q(framesize,framesize) Q(quality,quality)
            Q(brightness,brightness) Q(contrast,contrast)
            Q(saturation,saturation) Q(sharpness,sharpness)
            Q(denoise,denoise) Q(ae_level,ae_level)
            Q(awb_gain,awb_gain) Q(wb_mode,wb_mode)
            Q(aec_value,aec_value) Q(agc_gain,agc_gain)
            Q(hmirror,hmirror) Q(vflip,vflip)
            #undef Q
        }
        free(buf);
    }
    if (chg) applySettings();
    httpd_resp_set_type(r, "application/json");
    httpd_resp_sendstr(r, chg ? "{\"ok\":1}" : "{\"ok\":1}");
    return ESP_OK;
}

esp_err_t CameraWebServer::handleStatus(httpd_req_t *r) {
    String j = genStatusJSON();
    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    httpd_resp_sendstr(r, j.c_str());
    return ESP_OK;
}

bool CameraWebServer::applySettings() {
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return false;
    #define S(f,v) s->f(s, v)
    S(set_framesize,  (framesize_t)_settings.framesize);
    S(set_quality,    _settings.quality);
    S(set_brightness, _settings.brightness);
    S(set_contrast,   _settings.contrast);
    S(set_saturation, _settings.saturation);
    S(set_sharpness,  _settings.sharpness);
    S(set_denoise,    _settings.denoise);
    S(set_ae_level,   _settings.ae_level);
    S(set_whitebal,   _settings.awb_gain);
    S(set_wb_mode,    _settings.wb_mode);
    S(set_hmirror,    _settings.hmirror);
    S(set_vflip,      _settings.vflip);
    if (_settings.aec_value > 0) { S(set_exposure_ctrl,0); S(set_aec_value,_settings.aec_value); }
    else { S(set_exposure_ctrl,1); }
    if (_settings.agc_gain > 0) { S(set_gain_ctrl,0); S(set_agc_gain,_settings.agc_gain); }
    else { S(set_gain_ctrl,1); }
    #undef S
    return true;
}

String CameraWebServer::genStatusJSON() {
    StaticJsonDocument<1024> doc;
    static unsigned long last = 0;
    static uint32_t last_cnt = 0;
    static int fps = 0;
    uint32_t now_cnt = g_frame_count;
    unsigned long now = millis();
    if (now - last > 1000) {
        fps = (now_cnt - last_cnt) * 1000 / (now - last);
        last_cnt = now_cnt;
        last = now;
    }
    doc["fps"] = fps;
    const char *rn[] = {"96x96","QQVGA","QCIF","HQVGA","QVGA","CIF","VGA","SVGA","XGA","HD","SXGA","UXGA","FHD","QXGA","QSXGA"};
    if (_settings.framesize>=0 && _settings.framesize<=14) doc["resolution"] = rn[_settings.framesize];
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["cpu"] = String(ESP.getCpuFreqMHz())+"MHz · CH"+String(WiFi.channel());
    // 芯片温度 (ESP32-S3 内置温度传感器)
    doc["temp"] = String(temperatureRead(), 1)+"°C";
    doc["psram"] = String(ESP.getFreePsram()/1024)+"KB";
    doc["free_heap"] = ESP.getFreeHeap();
    doc["uptime"] = String(millis()/1000)+"s";
    JsonObject sk = doc.createNestedObject("settings");
    sk["framesize"]=_settings.framesize; sk["quality"]=_settings.quality;
    sk["brightness"]=_settings.brightness; sk["contrast"]=_settings.contrast;
    sk["saturation"]=_settings.saturation; sk["sharpness"]=_settings.sharpness;
    sk["denoise"]=_settings.denoise; sk["ae_level"]=_settings.ae_level;
    sk["awb_gain"]=_settings.awb_gain; sk["wb_mode"]=_settings.wb_mode;
    sk["aec_value"]=_settings.aec_value; sk["agc_gain"]=_settings.agc_gain;
    sk["hmirror"]=_settings.hmirror; sk["vflip"]=_settings.vflip;
    String out; serializeJson(doc, out); return out;
}
