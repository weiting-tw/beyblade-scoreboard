// 頁面七：比賽完成（規格第 6 節）
//
// 規格列了 [新比賽] [儲存結果] [返回首頁] 三顆按鈕。這裡沒有 [儲存結果]：
// 比賽一結束就已依「保存歷史」設定自動寫入 NVS，再放一顆手動存檔按鈕
// 只會造成同一場被記錄兩次。改為顯示 SAVED 狀態文字。詳見 docs/DECISIONS.md。
#include <cstdio>

#include "../app/app.h"
#include "status_chip.h"
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
    setCurrentScreen(ScreenId::Complete);
    lv_obj_t* s = makeScreen();

    const char* wname = g_match.winnerName();
    const Winner w = g_match.winner();
    const lv_color_t col =
        (w == Winner::P1) ? colP1() : ((w == Winner::P2) ? colP2() : colAccent());

    // 勝者顏色的光環由中心向外擴散。兩圈錯開半個週期，看起來是連續的脈動
    // 而不是一閃一閃。兩者都在背景層，不會擋到按鈕觸控。
    animRingBurst(s, col, 1800);
    animRingBurst(s, col, 1800, 900);

    // 資訊由上而下依序登場，讓視線跟著跑：標題 -> 勝者 -> 名字 -> 比分。
    lv_obj_t* title = makeLabel(s, "比賽結束", &font_tc_22, colSubtle(), 0, -140);
    animFadeIn(title, 300);

    lv_obj_t* verdict =
        makeLabel(s, (wname != nullptr) ? "勝者" : "平手", &font_tc_30, col, 0, -95);
    animPop(verdict, 60, 256, 480, 150);

    if (wname != nullptr) {
        lv_obj_t* n = makeLabel(s, wname, &font_tc_22, colText(), 0, -60);
        lv_obj_set_width(n, 260);
        lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
        lv_obj_align(n, LV_ALIGN_CENTER, 0, -60);
        animFadeIn(n, 320, 380);
    }

    // 比分拆成三個 label，兩邊的數字才能各自從 0 跑上去。
    // 用單一 "%d : %d" 字串就只能整串一次換掉，沒有揭曉感。
    lv_obj_t* sc1 = makeLabel(s, "0", &lv_font_montserrat_48, colP1(), -55, -10);
    makeLabel(s, ":", &lv_font_montserrat_36, colMuted(), 0, -10);
    lv_obj_t* sc2 = makeLabel(s, "0", &lv_font_montserrat_48, colP2(), 55, -10);
    animCountUp(sc1, 0, g_match.scoreP1(), 700, 550);
    animCountUp(sc2, 0, g_match.scoreP2(), 700, 550);

    char buf[40];
    const int played = g_match.round() - 1;
    std::snprintf(buf, sizeof(buf), "共 %d 局", played > 0 ? played : 0);
    lv_obj_t* rounds = makeLabel(s, buf, &font_tc_22, colSubtle(), 0, 35);
    animFadeIn(rounds, 300, 1100);

    lv_obj_t* saved =
        makeLabel(s, g_store.settings().match.saveHistory ? "已儲存" : "未儲存",
                  &font_tc_16, colSubtle(), 0, 62);
    animFadeIn(saved, 300, 1250);

    // 按鈕最後出現：動畫還在跑時就按下去會很容易誤觸。
    lv_obj_t* bNew = makeButton(s, "新比賽", -55, 100, 104, 48, colP2(),
                                onNewMatch, nullptr, &font_tc_16);
    lv_obj_t* bHome = makeButton(s, "首頁", 55, 100, 104, 48, colMuted(), onHome,
                                 nullptr, &font_tc_16);
    animFadeIn(bNew, 300, 1400);
    animFadeIn(bHome, 300, 1400);

    loadScreen(s, Nav::Rise);
    // 比賽流程中的四個畫面（倒數、計分、局結果、完成）一律收起狀態晶片。
    // 這頁的「比賽結束」在 dy -140，字框 y 26.5~53.5，跟晶片的 18.5~37.5
    // 重疊 11px —— 實機截圖上兩者確實疊在一起。
    // 收起來比把標題往下擠好：這是儀式性畫面，本來就該乾淨。
    statusChipSetHidden(true);
}

}  // namespace ui
}  // namespace bey
