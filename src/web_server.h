#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ArduinoJson.h>
#include <esp_http_server.h>

typedef struct {
    int framesize;
    int quality;
    int brightness;
    int contrast;
    int saturation;
    int sharpness;
    int denoise;
    int ae_level;
    int awb_gain;
    int wb_mode;
    int aec_value;
    int agc_gain;
    int hmirror;
    int vflip;
    int af_mode;    // 0=手动  1=连续自动对焦
    int focus_pos;  // 手动对焦位置 0-1023
} camera_settings_t;

class CameraWebServer {
public:
    CameraWebServer();
    ~CameraWebServer();
    void begin(int port = 80);
    void onWiFiConfig(std::function<void(const char*, const char*)> cb);

private:
    httpd_handle_t _ctrl_srv;
    httpd_handle_t _stream_srv;
    camera_settings_t _settings;
    std::function<void(const char*, const char*)> _wifiCb;

    static esp_err_t _root_h(httpd_req_t *r);
    static esp_err_t _stream_h(httpd_req_t *r);
    static esp_err_t _capture_h(httpd_req_t *r);
    static esp_err_t _set_h(httpd_req_t *r);
    static esp_err_t _status_h(httpd_req_t *r);

    esp_err_t handleRoot(httpd_req_t *r);
    esp_err_t handleStream(httpd_req_t *r);
    esp_err_t handleCapture(httpd_req_t *r);
    esp_err_t handleSet(httpd_req_t *r);
    esp_err_t handleStatus(httpd_req_t *r);

    bool applySettings();
    String genStatusJSON();
    static CameraWebServer* _inst;
};

#endif
