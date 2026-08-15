#include "screenshot.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

namespace bey {
namespace {

constexpr int kW = 360;
constexpr int kH = 360;

uint16_t* s_fb = nullptr;  // PSRAM，259KB，只在擷取期間存在
bool s_active = false;

const char kB64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// base64 串流編碼器。整張圖 RLE 後還有幾十 KB，不可能先組成一個字串再送。
struct B64Stream {
    uint32_t acc = 0;
    int bits = 0;
    int col = 0;

    void put(uint8_t byte) {
        acc = (acc << 8) | byte;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            emit(kB64[(acc >> bits) & 0x3F]);
        }
    }

    void emit(char c) {
        Serial.write(c);
        // 分行，免得單行過長把接收端的行緩衝撐爆
        if (++col >= 76) {
            Serial.println();
            col = 0;
        }
    }

    void finish() {
        if (bits > 0) {
            emit(kB64[(acc << (6 - bits)) & 0x3F]);
            const int pad = (bits == 2) ? 2 : 1;
            for (int i = 0; i < pad; ++i) {
                emit('=');
            }
        }
        if (col > 0) {
            Serial.println();
        }
    }
};

// RLE：連續相同的 16 位元像素壓成 (count, value)。
// UI 是大片純色，壓縮率很高 —— 不壓的話 259KB 走 115200 baud 要半分鐘。
void emitRle(B64Stream& out) {
    int i = 0;
    while (i < kW * kH) {
        const uint16_t v = s_fb[i];
        int n = 1;
        while (i + n < kW * kH && s_fb[i + n] == v && n < 65535) {
            ++n;
        }
        out.put(static_cast<uint8_t>(n & 0xFF));
        out.put(static_cast<uint8_t>(n >> 8));
        out.put(static_cast<uint8_t>(v & 0xFF));
        out.put(static_cast<uint8_t>(v >> 8));
        i += n;
    }
}

}  // namespace

void screenshotOnFlush(const lv_area_t* area, const lv_color_t* px) {
    if (!s_active || s_fb == nullptr) {
        return;
    }
    // 逐列複製。area 是閉區間。
    for (int y = area->y1; y <= area->y2; ++y) {
        if (y < 0 || y >= kH) {
            continue;
        }
        const int rowW = area->x2 - area->x1 + 1;
        for (int x = 0; x < rowW; ++x) {
            const int dx = area->x1 + x;
            if (dx < 0 || dx >= kW) {
                continue;
            }
            s_fb[y * kW + dx] = px[(y - area->y1) * rowW + x].full;
        }
    }
}

void screenshotCapture() {
    s_fb = static_cast<uint16_t*>(
        heap_caps_malloc(kW * kH * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
    if (s_fb == nullptr) {
        Serial.println("[snap] PSRAM 配置失敗");
        return;
    }
    for (int i = 0; i < kW * kH; ++i) {
        s_fb[i] = 0;
    }

    s_active = true;

    // 讓整個畫面重畫一次。invalidate 涵蓋的區域上所有圖層都會重繪，
    // 所以 layer_top 的狀態晶片與 overlay 都會進到 flush。
    lv_obj_invalidate(lv_scr_act());
    lv_obj_invalidate(lv_layer_top());
    // 同步跑完一輪重繪。flush_cb 內部會等 on_color_trans_done，
    // 這行返回時所有區域都已經流過 screenshotOnFlush。
    lv_refr_now(nullptr);

    s_active = false;

    Serial.printf("\n[snap] begin %dx%d rgb565 rle\n", kW, kH);
    B64Stream out;
    emitRle(out);
    out.finish();
    Serial.println("[snap] end");

    heap_caps_free(s_fb);
    s_fb = nullptr;
}

}  // namespace bey
