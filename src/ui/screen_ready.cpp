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
    lv_obj_t* lbl = makeLabel(parent, text, &lv_font_montserrat_20, col, dx, -70);
    lv_obj_set_width(lbl, 130);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_align(lbl, LV_ALIGN_CENTER, dx, -70);
    return lbl;
}

}  // namespace

void showReady() {
    const MatchConfig& cfg = g_store.settings().match;

    lv_obj_t* s = makeScreen();
    makeLabel(s, "READY", &lv_font_montserrat_28, colAccent(), 0, -130);

    nameLabel(s, cfg.p1Name, colP1(), -80);
    makeLabel(s, "VS", &lv_font_montserrat_20, colSubtle(), 0, -70);
    nameLabel(s, cfg.p2Name, colP2(), 80);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "FIRST TO %d", cfg.targetScore);
    makeLabel(s, buf, &lv_font_montserrat_20, colText(), 0, -25);

    makeButton(s, "START", 0, 30, 200, 58, colP2(), onStart, nullptr,
               &lv_font_montserrat_28);

    // 三顆窄按鈕：最右外角 (114, 119) 距圓心 165 < kSafeR(166)。
    makeButton(s, "BACK", -78, 95, 72, 48, colMuted(), onBack, nullptr,
               &lv_font_montserrat_14);
    makeButton(s, "NAMES", 0, 95, 72, 48, colMuted(), onNames, nullptr,
               &lv_font_montserrat_14);
    makeButton(s, "FORMAT", 78, 95, 72, 48, colMuted(), onFormat, nullptr,
               &lv_font_montserrat_14);

    loadScreen(s);
}

}  // namespace ui
}  // namespace bey
