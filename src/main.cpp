/*
 * ESP32-S3 + OV5640 网络摄像头
 * ============================
 *
 * 功能：
 *   - OV5640 摄像头驱动 (5MP, JPEG 输出)
 *   - WiFi 连接 (STA + AP 双模式)
 *   - Web 界面: MJPEG 视频流、拍照、参数调节
 *   - 分辨率切换 (QVGA ~ UXGA, 最大 2592×1944)
 *   - 运行时 WiFi 重配置
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
 *
 * 使用:
 *   1. PlatformIO 编译并刷入
 *   2. 打开串口监视器查看 IP
 *   3. 浏览器访问 http://<ip>
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>
#include "camera_config.h"
#include "web_server.h"

// ============================================================
// WiFi 配置 (修改为你自己的网络)
// ============================================================
const char* WIFI_SSID       = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD   = "YOUR_WIFI_PASSWORD";

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

    // 配置摄像头引脚 (在 camera_config.h 中定义)
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

    // 打印传感器信息
    Serial.printf("[Camera] 型号: OV%04x\n", s->id.PID);
    Serial.printf("[Camera] PSRAM: %s\n", ESP.getPsramSize() > 0 ? "可用" : "不可用");

    // OV5640 JPEG 初始化
    s->set_framesize(s, FRAMESIZE_VGA);
    s->set_quality(s, 12);
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_sharpness(s, 0);
    s->set_denoise(s, 0);
    s->set_whitebal(s, 1);
    s->set_wb_mode(s, 0);
    s->set_ae_level(s, 0);
    s->set_hmirror(s, 1);  // 默认镜像翻转
    s->set_vflip(s, 0);

    // OV5640 颜色优化 - 关键: 0x5001 不能旁路 ISP!
    Serial.println("[Camera] OV5640 颜色校准中...");

    // 通过标准 API 设置
    s->set_exposure_ctrl(s, 1);
    s->set_gain_ctrl(s, 1);
    s->set_whitebal(s, 1);       // 自动白平衡
    s->set_awb_gain(s, 1);       // AWB 增益
    s->set_saturation(s, 0);     // 默认饱和度
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_sharpness(s, 0);
    s->set_denoise(s, 0);

    // ISP 完整开启 (0x3F = 启用所有 ISP 模块, bit7=0 不旁路)
    s->set_reg(s, 0x5001, 0xFF, 0x3F);
    delay(10);

    // 颜色矩阵保持默认, 仅设 AWB 增益
    s->set_reg(s, 0x5186, 0xFF, 0x20);   // AWB 蓝增益
    s->set_reg(s, 0x5187, 0xFF, 0x20);   // AWB 红增益
    s->set_reg(s, 0x5188, 0xFF, 0x20);   // AWB 绿增益

    Serial.println("[Camera] OV5640 颜色校准完成!");

    // OV5640 自动对焦初始化
    Serial.println("[Camera] 对焦初始化中...");
    s->set_reg(s, 0x3000, 0xFF, 0x20);   // 使能 SCCB 传感器控制
    delay(10);
    s->set_reg(s, 0x3022, 0xFF, 0x02);   // AF 初始化
    delay(30);
    s->set_reg(s, 0x3022, 0xFF, 0x04);   // 单次对焦
    delay(50);
    s->set_reg(s, 0x3022, 0xFF, 0x08);   // 连续自动对焦
    delay(10);
    Serial.println("[Camera] 自动对焦已启动");

    Serial.println("[Camera] OV5640 初始化成功!");
    return true;
}

// ============================================================
// WiFi 初始化
// ============================================================
void initWiFi()
{
    Serial.println("\n[WiFi] 正在连接网络...");

    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.setHostname("esp32-cam-ov5640");

    // 连接 WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // 等待连接 (最多 15 秒)
    int timeout = 15;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(1000);
        Serial.print(".");
        timeout--;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] STA 连接成功!");
        Serial.printf("[WiFi] IP: %s / RSSI: %d dBm\n",
            WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
        Serial.println("\n[WiFi] STA 连接失败, 启动 AP 热点");

        // 创建热点 (无需密码时可传 NULL)
        WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL);
        delay(500);

        Serial.printf("[WiFi] AP: %s / IP: %s\n",
            AP_SSID, WiFi.softAPIP().toString().c_str());
    }
}

// ============================================================
// Web 服务器初始化
// ============================================================
void initWebServer()
{
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
        Serial.println("[系统] 摄像头初始化失败, 以诊断模式运行 (WiFi + Web, 无视频)");
    }

    initWiFi();
    initWebServer();
}

void loop()
{
    // 系统运行状态打印 (每 60 秒)
    static unsigned long lastReport = 0;
    if (millis() - lastReport > 60000) {
        lastReport = millis();
        Serial.printf("[状态] 堆: %d / PSRAM: %d / RSSI: %d dBm\n",
            ESP.getFreeHeap(), ESP.getPsramSize(), WiFi.RSSI());
    }

    // 低内存保护 (自动重启)
    if (ESP.getFreeHeap() < 8192) {
        Serial.println("[警告] 堆内存严重不足, 即将重启");
        delay(1000);
        ESP.restart();
    }

    delay(100);
}
