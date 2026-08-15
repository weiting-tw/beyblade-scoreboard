// 頁面一：首頁／待機（規格第 6 節）
#include "../app/app.h"
#include "ui.h"
#include "ui_theme.h"

namespace bey {
namespace ui {
namespace {

void onStart(lv_event_t*) { showFormat(); }

// 快速對戰：沿用上次存下的賽制，跳過賽制選擇。
void onQuick(lv_event_t*) {
    applySettingsToMatch();
    showReady();
}

void onSettings(lv_event_t*) { showSettings(); }
void onHistory(lv_event_t*) { showHistory(); }

}  // namespace

void showHome() {
    lv_obj_t* s = makeScreen();

    makeLabel(s, "BEYBLADE X", &lv_font_montserrat_28, colAccent(), 0, -115);
    makeLabel(s, "BATTLE SCORE", &lv_font_montserrat_20, colSubtle(), 0, -85);

    // 2×2 配置。四個按鈕的外角都落在安全圓內：
    // 最遠角 (136, 74) 距圓心 155 < kSafeR(166)。
    constexpr lv_coord_t kW = 132;
    constexpr lv_coord_t kH = 54;
    constexpr lv_coord_t kDx = 70;

    makeButton(s, "START", -kDx, -15, kW, kH, colP1(), onStart, nullptr);
    makeButton(s, "QUICK", kDx, -15, kW, kH, colP2(), onQuick, nullptr);
    makeButton(s, "SETTINGS", -kDx, 47, kW, kH, colMuted(), onSettings, nullptr);
    makeButton(s, "HISTORY", kDx, 47, kW, kH, colMuted(), onHistory, nullptr);

    loadScreen(s);
}

void init() { showHome(); }

}  // namespace ui
}  // namespace bey
