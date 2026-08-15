// 頁面七：比賽完成（規格第 6 節）
//
// 規格列了 [新比賽] [儲存結果] [返回首頁] 三顆按鈕。這裡沒有 [儲存結果]：
// 比賽一結束就已依「保存歷史」設定自動寫入 NVS，再放一顆手動存檔按鈕
// 只會造成同一場被記錄兩次。改為顯示 SAVED 狀態文字。詳見 docs/DECISIONS.md。
#include <cstdio>

#include "../app/app.h"
#include "ui.h"
#include "ui_theme.h"

namespace bey {
namespace ui {
namespace {

void onNewMatch(lv_event_t*) {
    applySettingsToMatch();  // 重新套用設定並歸零
    showReady();
}

void onHome(lv_event_t*) { showHome(); }

}  // namespace

void showComplete() {
    lv_obj_t* s = makeScreen();

    makeLabel(s, "MATCH COMPLETE", &lv_font_montserrat_20, colSubtle(), 0, -140);

    const char* wname = g_match.winnerName();
    const Winner w = g_match.winner();
    const lv_color_t col =
        (w == Winner::P1) ? colP1() : ((w == Winner::P2) ? colP2() : colAccent());

    makeLabel(s, (wname != nullptr) ? "WINNER" : "DRAW", &lv_font_montserrat_28,
              col, 0, -95);

    if (wname != nullptr) {
        lv_obj_t* n = makeLabel(s, wname, &lv_font_montserrat_20, colText(), 0, -60);
        lv_obj_set_width(n, 260);
        lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
        lv_obj_align(n, LV_ALIGN_CENTER, 0, -60);
    }

    char buf[40];
    std::snprintf(buf, sizeof(buf), "%d : %d", g_match.scoreP1(),
                  g_match.scoreP2());
    makeLabel(s, buf, &lv_font_montserrat_48, colText(), 0, -10);

    const int played = g_match.round() - 1;
    std::snprintf(buf, sizeof(buf), "ROUNDS %d", played > 0 ? played : 0);
    makeLabel(s, buf, &lv_font_montserrat_20, colSubtle(), 0, 35);

    makeLabel(s, g_store.settings().match.saveHistory ? "SAVED" : "NOT SAVED",
              &lv_font_montserrat_14, colSubtle(), 0, 62);

    makeButton(s, "NEW", -55, 100, 104, 48, colP2(), onNewMatch, nullptr,
               &lv_font_montserrat_14);
    makeButton(s, "HOME", 55, 100, 104, 48, colMuted(), onHome, nullptr,
               &lv_font_montserrat_14);

    loadScreen(s);
}

}  // namespace ui
}  // namespace bey
