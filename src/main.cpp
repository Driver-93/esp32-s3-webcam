/*
 * ESP32-S3 + OV5640 网络摄像头
 *
 * WiFi 保存: Preferences (NVS) → 重启 → initWiFi 读 Preferences 连接
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_camera.h>
#include "camera_config.h"
#include "web_server.h"
#include "debug_log.h"

// 双串口: Serial(USB) + Serial0(UART0=TTL)
HardwareSerial Serial0(0);
#define DUAL(fmt, ...) do { Serial.printf(fmt, ##__VA_ARGS__); Serial0.printf(fmt, ##__VA_ARGS__); } while(0)

const char* WIFI_SSID       = "";
const char* WIFI_PASSWORD   = "";
const char* AP_SSID         = "ESP32-CAM-OV5640";
const char* AP_PASSWORD     = "12345678";
const int   AP_CHANNEL      = 1;

CameraWebServer camServer;

bool initCamera();
void initWiFi();
void initWebServer();

bool initCamera()
{
    Serial.println("[Camera] init...");
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) { Serial.printf("[Camera] fail 0x%x\n", err); return false; }
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return false;
    s->set_framesize(s, FRAMESIZE_VGA);
    s->set_quality(s, 12);
    s->set_brightness(s, 0); s->set_contrast(s, 0);
    s->set_saturation(s, 0); s->set_sharpness(s, 0);
    s->set_denoise(s, 0);
    s->set_reg(s, 0x5001, 0xFF, 0x3F); delay(10);
    s->set_reg(s, 0x5186, 0xFF, 0x20);
    s->set_reg(s, 0x5187, 0xFF, 0x20);
    s->set_reg(s, 0x5188, 0xFF, 0x20);
    s->set_reg(s, 0x3000, 0xFF, 0x20); delay(10);
    s->set_reg(s, 0x3022, 0xFF, 0x02); delay(30);
    s->set_reg(s, 0x3022, 0xFF, 0x04); delay(50);
    s->set_reg(s, 0x3022, 0xFF, 0x08); delay(10);
    DUAL("[Camera] OK");
    return true;
}

// ============================================================
// 读 Preferences → 连接 → 回退硬编码 → AP
// ============================================================
void initWiFi()
{
    DUAL("\n=== WiFi ===\n");
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.setHostname("esp32-cam-ov5640");

    Preferences pref;
    pref.begin("camwifi", true);
    String savedSsid = pref.getString("ssid", "");
    String savedPass = pref.getString("pass", "");
    pref.end();

    String useSsid, usePass;
    if (savedSsid.length() > 0) {
        useSsid = savedSsid; usePass = savedPass;
        Serial.printf("[WiFi] Preferences: %s\n", useSsid.c_str());
    } else {
        useSsid = WIFI_SSID; usePass = WIFI_PASSWORD;
        Serial.printf("[WiFi] 硬编码: %s\n", useSsid.c_str());
    }

    if (useSsid.length() > 0) {
        WiFi.begin(useSsid.c_str(), usePass.c_str());
        int t = 15;
        while (WiFi.status() != WL_CONNECTED && t > 0) {
            delay(1000); Serial.print("."); t--;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\n[WiFi] OK: %s (%s)\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
            return;
        }
        Serial.printf("\n[WiFi] %s 连接失败\n", useSsid.c_str());
        // 回退硬编码
        if (savedSsid.length() > 0 && strlen(WIFI_SSID) > 0 && savedSsid != WIFI_SSID) {
            Serial.printf("[WiFi] 回退: %s\n", WIFI_SSID);
            WiFi.disconnect(); delay(200);
            WiFi.mode(WIFI_OFF); delay(500);
            WiFi.mode(WIFI_AP_STA); delay(300);
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            t = 15;
            while (WiFi.status() != WL_CONNECTED && t > 0) {
                delay(1000); Serial.print("."); t--;
            }
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("\n[WiFi] 回退 OK: %s\n", WiFi.SSID().c_str());
                return;
            }
        }
    }
    Serial.println("[WiFi] AP 热点");
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL);
    Serial.printf("[WiFi] AP: %s / %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
}

void initWebServer()
{
    camServer.onWiFiConfig([](const char *ssid, const char *pass) {
        DUAL("[WiFi] 保存: %s / %s\n", ssid, pass);
        log_write((String("Saved: ")+ssid+"/"+pass).c_str());
        Preferences pref;
        pref.begin("camwifi", false);
        pref.putString("ssid", ssid);
        pref.putString("pass", pass);
        pref.end();
        Serial.println("[WiFi] OK");
    });
    camServer.begin(80);
    DUAL("[Web] http://%s\n", WiFi.localIP().toString().c_str());
    if (WiFi.getMode() & WIFI_AP)
        DUAL("[Web] AP: http://%s\n", WiFi.softAPIP().toString().c_str());
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    DUAL("\nESP32-S3 + OV5640 | PSRAM: %d\n", ESP.getPsramSize());
    if (!initCamera()) DUAL("[Camera] no cam");
    initWiFi();
    initWebServer();
}

void loop()
{
    extern volatile bool g_pending_restart;
    static unsigned long restart_at = 0;
    if (g_pending_restart) {
        if (restart_at == 0) {
            restart_at = millis();
            DUAL("[WiFi] 8s 后重启...\n");
        }
        if (millis() - restart_at > 8000) {
            DUAL("[WiFi] 重启");
        log_write("Device restarting");
            ESP.restart();
        }
    } else restart_at = 0;

    static unsigned long lastReport = 0;
    if (millis() - lastReport > 60000) {
        lastReport = millis();
        DUAL("[状态] %d %d %d\n", ESP.getFreeHeap(), ESP.getPsramSize(), WiFi.RSSI());
    }
    if (ESP.getFreeHeap() < 8192) { delay(1000); ESP.restart(); }
    delay(50);
}
