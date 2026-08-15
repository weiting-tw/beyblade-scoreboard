// 頁面一：首頁／待機（規格第 6 節）
#include "../app/app.h"
#include "status_chip.h"
#include "ui.h"
#include "ui_theme.h"

namespace bey {
namespace ui {
namespace {

// 賽制固定是 4 分制，每場都選一次分數只是多按一步。
// 賽制選擇頁沒有刪掉，只是移出主流程 —— 從準備頁的「賽制」按鈕進得去。
void onStart(lv_event_t*) {
    applySettingsToMatch();
    showReady();
}

// 快速對戰：連準備頁都跳過，直接進倒數。
void onQuick(lv_event_t*) {
    applySettingsToMatch();
    showCountdown();
}

void onSettings(lv_event_t*) { showSettings(); }
void onHistory(lv_event_t*) { showHistory(); }

}  // namespace

void showHome() {
    lv_obj_t* s = makeScreen();

    makeLabel(s, "BEYBLADE X", &font_tc_30, colAccent(), 0, -115);
    makeLabel(s, "BATTLE SCORE", &font_tc_22, colSubtle(), 0, -85);

    // 2×2 配置。四個按鈕的外角都落在安全圓內：
    // 最遠角 (136, 74) 距圓心 155 < kSafeR(166)。
    constexpr lv_coord_t kW = 132;
    constexpr lv_coord_t kH = 54;
    constexpr lv_coord_t kDx = 70;

    makeButton(s, "開始比賽", -kDx, -15, kW, kH, colP1(), onStart, nullptr);
    makeButton(s, "快速對戰", kDx, -15, kW, kH, colP2(), onQuick, nullptr);
    makeButton(s, "設定", -kDx, 47, kW, kH, colMuted(), onSettings, nullptr);
    makeButton(s, "歷史紀錄", kDx, 47, kW, kH, colMuted(), onHistory, nullptr);

    loadScreen(s, Nav::Back);
}

void init() {
    // 狀態晶片要在第一個畫面之前建立。它住在 lv_layer_top，先建立才會排在
    // Z 序最底，被 overlay 的半透明遮罩蓋住而不是浮在確認對話框上面。
    statusChipInit();
    showHome();
}

}  // namespace ui
}  // namespace bey
