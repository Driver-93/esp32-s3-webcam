#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <Arduino.h>
#include <Preferences.h>

// 轻量级持久日志 - 存 NVS, 掉电不丢, 循环覆盖
// 最多保存 LOG_MAX 条, 每条 64 字节
#define LOG_MAX 32
#define LOG_LEN 64

static inline void log_write(const char *msg) {
    Preferences pref;
    pref.begin("diaglog", false);
    int idx = pref.getInt("idx", 0);
    char key[8];
    snprintf(key, sizeof(key), "l%02d", idx % LOG_MAX);
    pref.putString(key, msg);
    pref.putInt("idx", idx + 1);
    pref.end();
}

// 返回所有日志 (HTML <br> 分隔)
static inline String log_read() {
    Preferences pref;
    pref.begin("diaglog", true);
    int idx = pref.getInt("idx", 0);
    String out;
    int start = idx > LOG_MAX ? idx % LOG_MAX : 0;
    int count = min(idx, LOG_MAX);
    for (int i = 0; i < count; i++) {
        char key[8];
        snprintf(key, sizeof(key), "l%02d", (start + i) % LOG_MAX);
        String line = pref.getString(key, "");
        if (line.length() > 0) {
            if (out.length() > 0) out += "<br>";
            out += line;
        }
    }
    if (out.length() == 0) out = "(empty)";
    pref.end();
    return out;
}

// 清空日志
static inline void log_clear() {
    Preferences pref;
    pref.begin("diaglog", false);
    pref.clear();
    pref.end();
}

#endif
