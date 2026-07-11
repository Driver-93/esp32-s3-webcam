/*
 * ESP32-S3 + OV5640 网络摄像头
 * ============================
 *
 * 功能：
 *   - OV5640 摄像头驱动 (5MP, JPEG 输出)
 *   - WiFi 连接 (STA + AP 双模式)
 *   - Web 界面: MJPEG 视频流、拍照、参数调节
 *   - 分辨率切换 (QVGA ~ UXGA, 最大 2592×1944)
 *   - 运行时 WiFi 重配置 (保存到 NVS)
 *
 * 接线参考 (ESP32-S3 + OV5640):
 *
 *   OV5640  | ESP32-S3       说明
 *   --------|----------------------
 *   PWDN    | GND (或 NC)
 *   RESET   | GND (或 NC)
 *   XCLK    | GPIO15         主时钟 (20MHz)
 *   SIOD    | GPIO4          I2C SDA
 *   SIOC    | GPIO5           I2C SCL
 *   Y9      | GPIO16         数据 D9
 *   Y8      | GPIO17         数据 D8
 *   Y7      | GPIO18         数据 D7
 *   Y6      | GPIO12         数据 D6
 *   Y5      | GPIO10         数据 D5
 *   Y4      | GPIO8          数据 D4
 *   Y3      | GPIO9          数据 D3
 *   Y2      | GPIO11         数据 D2
 *   VSYNC   | GPIO6          帧同步
 *   HREF    | GPIO7          行同步
 *   PCLK    | GPIO13         像素时钟
 *
 *   注意：
 *   - 确保摄像头供电充足 (3.3V / 150mA+)
 *   - 必须启用 PSRAM (推荐 8MB)
 *   - 使用前修改下方 WIFI_SSID / WIFI_PASSWORD
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_camera.h>
#include "camera_config.h"
#include "web_server.h"

// ============================================================
// WiFi 配置 (硬编码仅作首次连接用, 网页保存后持久化到 NVS)
// 设为空字符串则首次启动直接开热点
// ============================================================
const char* WIFI_SSID       = "";
const char* WIFI_PASSWORD   = "";

// 当 STA 连接失败时启动 AP 热点
const char* AP_SSID         = "ESP32-CAM-OV5640";
const char* AP_PASSWORD     = "12345678";   // 至少 8 位
const int   AP_CHANNEL      = 1;

// ============================================================
// 全局对象
// ============================================================
CameraWebServer camServer;

// ============================================================
// 函数声明
// ============================================================
bool initCamera();
void initWiFi();
void initWebServer();

// ============================================================
// 摄像头初始化
// ============================================================
bool initCamera()
{
    Serial.println("[Camera] 初始化 OV5640 ...");

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("[Camera] 初始化失败! 错误码: 0x%x\n", err);
        Serial.printf("[Camera] 常见原因: 接线错误 / 供电不足 / PSRAM 未启用\n");
        return false;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        Serial.println("[Camera] 获取传感器指针失败");
        return false;
    }

    Serial.printf("[Camera] 型号: OV%04x\n", s->id.PID);
    Serial.printf("[Camera] PSRAM: %s\n", ESP.getPsramSize() > 0 ? "可用" : "不可用");

    s->set_framesize(s, FRAMESIZE_VGA);
    s->set_quality(s, 12);
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_sharpness(s, 0);
    s->set_denoise(s, 0);

    // ISP 完整开启 (0x3F = 启用所有 ISP 模块, bit7=0 不旁路)
    s->set_reg(s, 0x5001, 0xFF, 0x3F);
    delay(10);

    s->set_reg(s, 0x5186, 0xFF, 0x20);   // AWB 蓝增益
    s->set_reg(s, 0x5187, 0xFF, 0x20);   // AWB 红增益
    s->set_reg(s, 0x5188, 0xFF, 0x20);   // AWB 绿增益
    Serial.println("[Camera] OV5640 颜色校准完成!");

    // OV5640 自动对焦初始化
    Serial.println("[Camera] 对焦初始化中...");
    s->set_reg(s, 0x3000, 0xFF, 0x20);
    delay(10);
    s->set_reg(s, 0x3022, 0xFF, 0x02);
    delay(30);
    s->set_reg(s, 0x3022, 0xFF, 0x04);
    delay(50);
    s->set_reg(s, 0x3022, 0xFF, 0x08);
    delay(10);
    Serial.println("[Camera] 自动对焦已启动");

    Serial.println("[Camera] OV5640 初始化成功!");
    return true;
}

// ============================================================
// WiFi 初始化 - 优先用 NVS 保存的凭证, 硬编码仅作回退
// ============================================================
void initWiFi()
{
    Serial.println("\n[WiFi] 正在连接网络...");

    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.setHostname("esp32-cam-ov5640");

    // 第一步: 从 Preferences 读取网页保存的凭证
    Preferences pref;
    pref.begin("wificfg", true);
    String savedSsid = pref.getString("ssid", "");
    String savedPass = pref.getString("pass", "");
    pref.end();

    String useSsid, usePass;
    if (savedSsid.length() > 0) {
        useSsid = savedSsid;
        usePass = savedPass;
        Serial.printf("[WiFi] 尝试保存的凭证: %s\n", useSsid.c_str());
    } else {
        useSsid = WIFI_SSID;
        usePass = WIFI_PASSWORD;
        Serial.printf("[WiFi] 尝试硬编码: %s\n", useSsid.c_str());
    }

    if (useSsid.length() > 0) {
        WiFi.persistent(false);  // 不让 WiFi 栈 NVS 干扰
        WiFi.begin(useSsid.c_str(), usePass.c_str());
        int timeout = 15;
        while (WiFi.status() != WL_CONNECTED && timeout > 0) {
            delay(1000); Serial.print("."); timeout--;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\n[WiFi] %s 连接成功! IP: %s\n",
                WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
            return;
        }
        Serial.printf("\n[WiFi] %s 连接失败\n", useSsid.c_str());

        // 如果保存的凭证连不上但有硬编码, 重置 WiFi 后回退
        if (savedSsid.length() > 0 && strlen(WIFI_SSID) > 0 &&
            savedSsid != WIFI_SSID) {
            Serial.printf("[WiFi] 回退到硬编码: %s\n", WIFI_SSID);
            WiFi.disconnect(true);
            delay(500);
            WiFi.mode(WIFI_AP_STA);
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            timeout = 15;
            while (WiFi.status() != WL_CONNECTED && timeout > 0) {
                delay(1000); Serial.print("."); timeout--;
            }
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("\n[WiFi] %s 回退成功!\n", WiFi.SSID().c_str());
                return;
            }
        }
    }

    // 都失败, 启动 AP
    Serial.println("[WiFi] 启动 AP 热点");
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL);
    delay(500);
    Serial.printf("[WiFi] AP: %s / IP: %s\n",
        AP_SSID, WiFi.softAPIP().toString().c_str());
}

// ============================================================
// Web 服务器初始化
// ============================================================
void initWebServer()
{
    camServer.onWiFiConfig([](const char *ssid, const char *pass) {
        Serial.printf("[WiFi] 保存凭证到 Preferences: %s\n", ssid);
        Preferences pref;
        pref.begin("wificfg", false);
        pref.putString("ssid", ssid);
        pref.putString("pass", pass);
        pref.end();
        Serial.println("[WiFi] 保存成功, 等待重启");
    });
    camServer.begin(80);

    Serial.println("\n==============================");
    Serial.println("  ESP32-S3 摄像头已就绪!");
    Serial.println("==============================");

    if (WiFi.getMode() & WIFI_STA) {
        Serial.printf("  局域网: http://%s\n", WiFi.localIP().toString().c_str());
    }
    if (WiFi.getMode() & WIFI_AP) {
        Serial.printf("  热点:   http://%s\n", WiFi.softAPIP().toString().c_str());
    }
    Serial.println("==============================\n");
}

// ============================================================
// Arduino 入口
// ============================================================
void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println("\n\n==============================");
    Serial.println("  ESP32-S3 + OV5640 摄像头");
    Serial.println("==============================");
    Serial.printf("  芯片: %s\n", ESP.getChipModel());
    Serial.printf("  内核: %d 核 @ %d MHz\n", ESP.getChipCores(), ESP.getCpuFreqMHz());
    Serial.printf("  PSRAM: %d bytes\n", ESP.getPsramSize());
    Serial.printf("  Flash: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.println("==============================\n");

    if (!initCamera()) {
        Serial.println("[系统] 摄像头初始化失败, 以诊断模式运行");
    }

    initWiFi();
    initWebServer();
}

void loop()
{
    static unsigned long lastReport = 0;
    if (millis() - lastReport > 60000) {
        lastReport = millis();
        Serial.printf("[状态] 堆: %d / PSRAM: %d / RSSI: %d dBm\n",
            ESP.getFreeHeap(), ESP.getPsramSize(), WiFi.RSSI());
    }

    // WiFi 保存后延迟重启, 确保 HTTP 响应发出
    extern volatile bool g_pending_restart;
    if (g_pending_restart) {
        static unsigned long restart_at = 0;
        if (restart_at == 0) {
            restart_at = millis();
            Serial.println("[WiFi] 准备重启...");
        } else if (millis() - restart_at > 3000) {
            Serial.println("[WiFi] 重启!");
            ESP.restart();
        }
    }

    if (ESP.getFreeHeap() < 8192) {
        Serial.println("[警告] 堆内存严重不足, 即将重启");
        delay(1000);
        ESP.restart();
    }

    delay(50);
}
