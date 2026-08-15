// 觸控／手勢診斷韌體。
//
//   pio run -e touch_probe -t upload
//
// 刻意做成獨立韌體而不是塞進正式韌體的某個畫面：測手勢的時候不該有轉場
// 動畫、狀態晶片、音效在旁邊干擾，開機就直接進測試畫面最省事。
//
// 畫面上有：
//   黃色線   手指走過的軌跡（最多 160 點，畫一整圈約 149 點）
//   紅點     目前觸碰位置
//   灰圈     半徑 130，邊緣旋轉的判定門檻。手指在圈外畫才算
//   文字     座標、距圓心半徑、累積轉角（順時針正）、CST816 回報的手勢
//
// 累積轉角要看的是「轉了多少」而不是「現在在哪」，所以跨越 ±180 度時要折
// 回來再累加，否則每轉過正下方就會多算一整圈。
#include <Arduino.h>
#include <lvgl.h>

#include <cmath>
#include <cstdio>

#include "Display_ST77916.h"
#include "I2C_Driver.h"
#include "LVGL_Driver.h"
#include "Touch_CST816.h"
#include "app/gesture.h"

namespace {

constexpr int kCx = 180;
constexpr int kCy = 180;
constexpr int kEdgeR = 130;  // 邊緣旋轉的半徑門檻
constexpr int kTrailMax = 160;

lv_point_t s_trail[kTrailMax];
int s_trailN = 0;
bool s_wasDown = false;

lv_obj_t* s_line = nullptr;
lv_obj_t* s_info = nullptr;
lv_obj_t* s_dot = nullptr;

float s_lastAngle = 0.0f;
float s_totalTurn = 0.0f;
const char* s_lastGesture = "-";

void onTick(lv_timer_t*) {
    bey::Gesture g;
    while (bey::gesturePoll(g)) {
        switch (g) {
            case bey::Gesture::SwipeUp:    s_lastGesture = "UP";    break;
            case bey::Gesture::SwipeDown:  s_lastGesture = "DOWN";  break;
            case bey::Gesture::SwipeLeft:  s_lastGesture = "LEFT";  break;
            case bey::Gesture::SwipeRight: s_lastGesture = "RIGHT"; break;
            default: break;
        }
    }

    const bey::TouchPoint t = bey::touchLast();
    char buf[96];

    if (t.down) {
        const float dx = static_cast<float>(t.x - kCx);
        const float dy = static_cast<float>(t.y - kCy);
        const float r = sqrtf(dx * dx + dy * dy);
        const float a = atan2f(dy, dx) * 180.0f / static_cast<float>(M_PI);

        if (!s_wasDown) {
            s_trailN = 0;
            s_totalTurn = 0.0f;
        } else {
            float d = a - s_lastAngle;
            while (d > 180.0f) d -= 360.0f;
            while (d < -180.0f) d += 360.0f;
            s_totalTurn += d;
        }
        s_lastAngle = a;

        if (s_trailN < kTrailMax) {
            s_trail[s_trailN].x = t.x;
            s_trail[s_trailN].y = t.y;
            s_trailN++;
            if (s_trailN >= 2) {
                lv_line_set_points(s_line, s_trail, s_trailN);
                lv_obj_clear_flag(s_line, LV_OBJ_FLAG_HIDDEN);
            }
        }

        lv_obj_clear_flag(s_dot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(s_dot, t.x - 6, t.y - 6);

        std::snprintf(buf, sizeof(buf), "%d, %d\nr=%d %s\n%d deg\n%s", t.x, t.y,
                      static_cast<int>(r), (r >= kEdgeR) ? "EDGE" : "",
                      static_cast<int>(s_totalTurn), s_lastGesture);
    } else {
        lv_obj_add_flag(s_dot, LV_OBJ_FLAG_HIDDEN);
        std::snprintf(buf, sizeof(buf), "touch me\n%d pts\n%d deg\n%s",
                      s_trailN, static_cast<int>(s_totalTurn), s_lastGesture);
    }
    lv_label_set_text(s_info, buf);
    s_wasDown = t.down;
}

void buildUi() {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x08080F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* ring = lv_obj_create(scr);
    lv_obj_set_size(ring, kEdgeR * 2, kEdgeR * 2);
    lv_obj_center(ring);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_color(ring, lv_color_hex(0x2C2C3A), LV_PART_MAIN);
    lv_obj_set_style_border_width(ring, 2, LV_PART_MAIN);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);

    // 明確給滿版尺寸並固定在原點：點座標是螢幕座標，物件若被 LV_SIZE_CONTENT
    // 移動或縮小，線就會畫到看不見的地方。
    s_line = lv_line_create(scr);
    lv_obj_set_size(s_line, 360, 360);
    lv_obj_set_pos(s_line, 0, 0);
    lv_obj_set_style_line_color(s_line, lv_color_hex(0xFFD60A), LV_PART_MAIN);
    lv_obj_set_style_line_width(s_line, 3, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(s_line, true, LV_PART_MAIN);
    lv_obj_add_flag(s_line, LV_OBJ_FLAG_HIDDEN);

    s_info = lv_label_create(scr);
    lv_label_set_text(s_info, "touch me");
    lv_obj_set_style_text_color(s_info, lv_color_hex(0xF2F2F7), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_info, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(s_info);

    s_dot = lv_obj_create(scr);
    lv_obj_set_size(s_dot, 12, 12);
    lv_obj_set_style_bg_color(s_dot, lv_color_hex(0xFF3B30), LV_PART_MAIN);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_dot, 0, LV_PART_MAIN);
    lv_obj_add_flag(s_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_dot, LV_OBJ_FLAG_CLICKABLE);

    lv_timer_create(onTick, 30, nullptr);  // 與觸控輪詢同頻
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n===== touch probe =====");

    I2C_Init();
    Backlight_Init();
    LCD_Init();
    Lvgl_Init();
    buildUi();

    Serial.println("畫面已就緒：黃線=軌跡 紅點=目前位置 灰圈=半徑 130 門檻");
}

void loop() {
    Lvgl_Loop();
    delay(5);
}
