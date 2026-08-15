// 實體按鍵診斷韌體。
//
//   pio run -e button_probe -t upload
//
// 板上有 PWR 與 BOOT 兩顆實體按鍵，但沒有任何文件寫它們接到哪支 GPIO ——
// 官方 repo 只有原理圖 PDF。BOOT 一般是 GPIO0；PWR 在不少板子上是接電源
// 管理晶片而不是可讀的 GPIO，那種情況軟體根本碰不到。這支把候選腳位全設成
// 上拉輸入，按下去哪個變 0 就知道是哪支。
//
// 候選腳位是「扣掉已知用途」之後剩下的：
//   1 觸控RST  2 MCLK   3 LCD RST  4 觸控INT  5 背光   9 SPK_EN
//   10/11 I2C  18 TE    21 LCD CS  38 LRCK    39 DIN   40 SCK
//   41/42/45/46 QSPI    47 DOUT    48 BCLK
//   19/20 USB D-/D+     26~37 SPI flash 與 Octal PSRAM   43/44 UART0
//
// GPIO0 設成上拉輸入是安全的：BOOT 的 strapping 只在 reset 那一瞬間判定，
// 開機之後它就是一支普通 GPIO。
#include <Arduino.h>
#include <lvgl.h>

#include <cstdio>

#include "Display_ST77916.h"
#include "I2C_Driver.h"
#include "LVGL_Driver.h"

namespace {

const int kPins[] = {0, 6, 7, 8, 12, 13, 14, 15, 16, 17};
constexpr int kPinCount = sizeof(kPins) / sizeof(kPins[0]);

int s_state[kPinCount];
int s_changes[kPinCount];  // 累計變化次數，按過的腳位一眼看得出來

lv_obj_t* s_label = nullptr;
lv_obj_t* s_hint = nullptr;

void onTick(lv_timer_t*) {
    char buf[256];
    int n = 0;
    bool anyChange = false;

    for (int i = 0; i < kPinCount; ++i) {
        const int v = digitalRead(kPins[i]);
        if (v != s_state[i]) {
            s_state[i] = v;
            s_changes[i]++;
            anyChange = true;
            Serial.printf("[btn] GPIO%-2d -> %d  (第 %d 次變化)\n", kPins[i], v,
                          s_changes[i]);
        }
    }

    // 只列出「曾經變化過」的腳位，沒被按過的不佔版面。
    for (int i = 0; i < kPinCount && n < static_cast<int>(sizeof(buf)) - 32; ++i) {
        if (s_changes[i] > 0) {
            n += std::snprintf(buf + n, sizeof(buf) - n, "GPIO%d = %d  (x%d)\n",
                               kPins[i], s_state[i], s_changes[i]);
        }
    }
    if (n == 0) {
        std::snprintf(buf, sizeof(buf), "還沒偵測到變化");
    }
    lv_label_set_text(s_label, buf);

    if (anyChange) {
        lv_obj_set_style_text_color(s_label, lv_color_hex(0xFFD60A), LV_PART_MAIN);
    }
}

// 每兩秒把累計結果印一次。變化本身是瞬間的，序列埠沒開著就漏掉了 ——
// 定期重印讓「先按、之後再讀」也拿得到結果，不必兩邊對時間。
void onSummary(lv_timer_t*) {
    bool any = false;
    for (int i = 0; i < kPinCount; ++i) {
        if (s_changes[i] > 0) {
            if (!any) {
                Serial.println("[sum] --- 按過的腳位 ---");
                any = true;
            }
            Serial.printf("[sum] GPIO%-2d 目前 %d，變化 %d 次\n", kPins[i],
                          s_state[i], s_changes[i]);
        }
    }
    if (!any) {
        Serial.println("[sum] 尚未偵測到任何腳位變化");
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n===== button probe =====");

    for (int i = 0; i < kPinCount; ++i) {
        pinMode(kPins[i], INPUT_PULLUP);
        s_state[i] = digitalRead(kPins[i]);
        s_changes[i] = 0;
        Serial.printf("GPIO%-2d 初始 %d\n", kPins[i], s_state[i]);
    }

    I2C_Init();
    Backlight_Init();
    LCD_Init();
    Lvgl_Init();

    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x08080F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    s_hint = lv_label_create(scr);
    lv_label_set_text(s_hint, "press PWR / BOOT");
    lv_obj_set_style_text_color(s_hint, lv_color_hex(0x8E8E9A), LV_PART_MAIN);
    lv_obj_align(s_hint, LV_ALIGN_CENTER, 0, -120);

    s_label = lv_label_create(scr);
    lv_label_set_text(s_label, "還沒偵測到變化");
    lv_obj_set_style_text_color(s_label, lv_color_hex(0xF2F2F7), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(s_label);

    lv_timer_create(onTick, 20, nullptr);
    lv_timer_create(onSummary, 2000, nullptr);
    Serial.println("按下 PWR 或 BOOT，畫面與序列埠都會顯示是哪支腳位");
}

void loop() {
    Lvgl_Loop();
    delay(5);
}
