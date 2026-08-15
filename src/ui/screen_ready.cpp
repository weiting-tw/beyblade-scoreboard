// 頁面三：比賽準備（規格第 6 節）
#include <cstdio>

#include "../app/app.h"
#include "ui.h"
#include "ui_theme.h"

namespace bey {
namespace ui {
namespace {

void onStart(lv_event_t*) { showCountdown(); }
void onNames(lv_event_t*) { showNames(); }
void onFormat(lv_event_t*) { showFormat(); }
void onBack(lv_event_t*) { showHome(); }

// 玩家名稱最長 19 字元，裝不下時讓 LVGL 自動截字加省略號。
lv_obj_t* nameLabel(lv_obj_t* parent, const char* text, lv_color_t col,
                    lv_coord_t dx) {
    lv_obj_t* lbl = makeLabel(parent, text, &font_tc_22, col, dx, -70);
    lv_obj_set_width(lbl, 130);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_align(lbl, LV_ALIGN_CENTER, dx, -70);
    return lbl;
}

}  // namespace

void showReady() {
    const MatchConfig& cfg = g_store.settings().match;

    lv_obj_t* s = makeScreen();
    makeLabel(s, "準備", &font_tc_30, colAccent(), 0, -130);

    nameLabel(s, cfg.p1Name, colP1(), -80);
    makeLabel(s, "VS", &font_tc_22, colSubtle(), 0, -70);
    nameLabel(s, cfg.p2Name, colP2(), 80);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "先得 %d 分", cfg.targetScore);
    makeLabel(s, buf, &font_tc_22, colText(), 0, -25);

    makeButton(s, "開始", 0, 30, 200, 58, colP2(), onStart, nullptr,
               &font_tc_30);

    // 三顆窄按鈕：最右外角 (114, 119) 距圓心 165 < kSafeR(166)。
    makeButton(s, "返回", -78, 95, 72, 48, colMuted(), onBack, nullptr,
               &font_tc_16);
    makeButton(s, "玩家", 0, 95, 72, 48, colMuted(), onNames, nullptr,
               &font_tc_16);
    makeButton(s, "賽制", 78, 95, 72, 48, colMuted(), onFormat, nullptr,
               &font_tc_16);

    loadScreen(s, Nav::Forward);
}

}  // namespace ui
}  // namespace bey
