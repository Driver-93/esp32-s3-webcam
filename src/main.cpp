#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>
#include "camera_config.h"
#include "web_server.h"

const char* WIFI_SSID       = "CMCC-577v";
const char* WIFI_PASSWORD   = "84602971";
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
    Serial.println("[Camera] OK");
    return true;
}

void initWiFi()
{
    Serial.println("\n=== WiFi ===");
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.setHostname("esp32-cam-ov5640");

    Serial.printf("[WiFi] Connecting %s...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int t = 15;
    while (WiFi.status() != WL_CONNECTED && t > 0) {
        delay(1000); Serial.print("."); t--;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] OK: %s (%s)\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        return;
    }
    Serial.println("\n[WiFi] Failed, AP mode");
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL);
    Serial.printf("[WiFi] AP: %s / %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
}

void initWebServer()
{
    camServer.onWiFiConfig([](const char *ssid, const char *pass) {
        Serial.printf("[WiFi] Switch to: %s\n", ssid);
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, pass);
    });
    camServer.begin(80);
    Serial.printf("[Web] http://%s\n", WiFi.localIP().toString().c_str());
    if (WiFi.getMode() & WIFI_AP)
        Serial.printf("[Web] AP: http://%s\n", WiFi.softAPIP().toString().c_str());
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.printf("\nESP32-S3 + OV5640 | PSRAM: %d\n", ESP.getPsramSize());
    if (!initCamera()) Serial.println("[Camera] no cam");
    initWiFi();
    initWebServer();
}

void loop()
{
    static unsigned long lastReport = 0;
    if (millis() - lastReport > 60000) {
        lastReport = millis();
        Serial.printf("[状态] %d %d %d\n", ESP.getFreeHeap(), ESP.getPsramSize(), WiFi.RSSI());
    }
    if (ESP.getFreeHeap() < 8192) { delay(1000); ESP.restart(); }
    delay(50);
}
