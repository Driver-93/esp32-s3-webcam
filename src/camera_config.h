#ifndef CAMERA_CONFIG_H
#define CAMERA_CONFIG_H

#include "esp_camera.h"

/*
 * ESP32-S3 + OV5640 引脚分配
 *
 * 请根据你的实际硬件连接调整这些引脚
 * 常用引脚定义如下：
 */

// OV5640 引脚定义 (ESP32-S3)
#define PWDN_GPIO_NUM     -1    // 电源关闭 - 不使用
#define RESET_GPIO_NUM    -1    // 复位 - 不使用
#define XCLK_GPIO_NUM     15    // 主时钟输出
#define SIOD_GPIO_NUM      4    // SCCB 数据线 (I2C_SDA)
#define SIOC_GPIO_NUM      5    // SCCB 时钟线 (I2C_SCL)
#define Y9_GPIO_NUM        16   // 数据位 D9
#define Y8_GPIO_NUM        17   // 数据位 D8
#define Y7_GPIO_NUM        18   // 数据位 D7
#define Y6_GPIO_NUM        12   // 数据位 D6
#define Y5_GPIO_NUM        10   // 数据位 D5
#define Y4_GPIO_NUM         8   // 数据位 D4
#define Y3_GPIO_NUM         9   // 数据位 D3
#define Y2_GPIO_NUM        11   // 数据位 D2
#define VSYNC_GPIO_NUM      6   // 帧同步
#define HREF_GPIO_NUM       7   // 行同步
#define PCLK_GPIO_NUM      13   // 像素时钟

/*
 * 如果你的摄像头模块是这些常见型号，请参考以下引脚：
 *
 * 型号               | XCLK | SIOD | SIOC | Y9  | Y8  | Y7  | Y6  | Y5  | Y4  | Y3  | Y2  | VSYNC | HREF | PCLK | PWDN | RESET
 * -------------------|------|------|------|-----|-----|-----|-----|-----|-----|-----|-----|-------|------|------|------|------
 * ESP32-S3-OW5640    | 15   | 4    | 5    | 16  | 17  | 18  | 12  | 10  | 8   | 9   | 11  | 6     | 7    | 13   | -1   | -1
 * XIAO ESP32S3 (Seeed) | 10 | 4  | 5    | 16  | 17  | 18  | 12  | 11  | 48  | 47  | 21  | 6     | 7    | 13   | 1    | 2
 * FREENOVE S3         | 15   | 4    | 5    | 16  | 17  | 18  | 12  | 10  | 8   | 9   | 11  | 6     | 7    | 13   | -1   | 0
 *
 * 如果你的摄像头出现显示异常（偏色、条纹等），请尝试调整 XCLK 频率。
 */

// 摄像头初始化配置
static camera_config_t camera_config = {
    .pin_pwdn       = PWDN_GPIO_NUM,
    .pin_reset      = RESET_GPIO_NUM,
    .pin_xclk       = XCLK_GPIO_NUM,
    .pin_sccb_sda   = SIOD_GPIO_NUM,
    .pin_sccb_scl   = SIOC_GPIO_NUM,
    .pin_d7         = Y9_GPIO_NUM,
    .pin_d6         = Y8_GPIO_NUM,
    .pin_d5         = Y7_GPIO_NUM,
    .pin_d4         = Y6_GPIO_NUM,
    .pin_d3         = Y5_GPIO_NUM,
    .pin_d2         = Y4_GPIO_NUM,
    .pin_d1         = Y3_GPIO_NUM,
    .pin_d0         = Y2_GPIO_NUM,
    .pin_vsync      = VSYNC_GPIO_NUM,
    .pin_href       = HREF_GPIO_NUM,
    .pin_pclk       = PCLK_GPIO_NUM,

    // XCLK 频率: OV5640 支持 6-27MHz
    .xclk_freq_hz   = 20000000,

    // LEDC 通道用于生成 XCLK
    .ledc_timer     = LEDC_TIMER_0,
    .ledc_channel   = LEDC_CHANNEL_0,

    // 像素格式: OV5640 支持 JPEG 输出
    .pixel_format   = PIXFORMAT_JPEG,

    // 帧尺寸: OV5640 最大支持 2592x1944 (5MP)
    // 根据需求选择合适的分辨率
    .frame_size     = FRAMESIZE_VGA,    // 640x480 - 默认
    // 可选尺寸:
    // FRAMESIZE_QQVGA   - 160x120
    // FRAMESIZE_QVGA    - 320x240
    // FRAMESIZE_VGA     - 640x480
    // FRAMESIZE_SVGA    - 800x600
    // FRAMESIZE_XGA     - 1024x768
    // FRAMESIZE_HD      - 1280x720
    // FRAMESIZE_SXGA    - 1280x1024
    // FRAMESIZE_UXGA    - 1600x1200
    // FRAMESIZE_QXGA    - 2048x1536
    // FRAMESIZE_QSXGA   - 2592x1944

    .jpeg_quality   = 12,               // JPEG 质量 0-63 (越低越好)
    .fb_count       = 2,                // 帧缓冲区数量 (建议 2 用于并发流)
    .fb_location    = CAMERA_FB_IN_PSRAM, // 使用 PSRAM 存储帧缓冲
    .grab_mode      = CAMERA_GRAB_LATEST, // 抓取最新帧 (减少延迟)
};

#endif // CAMERA_CONFIG_H
