// 頁面一：首頁／待機（規格第 6 節）
#include <cstdio>
#include <ctime>

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

// 時鐘。Label 屬於畫面，畫面重建時會被 auto_del 釋放，timer 必須跟著收掉。
lv_obj_t* s_clock = nullptr;
lv_timer_t* s_clockTimer = nullptr;

void updateClock(lv_timer_t*) {
    if (s_clock == nullptr) {
        return;
    }
    const uint32_t e = nowEpoch();
    if (e == 0) {
        return;
    }
    // 存進 RTC 的已經是本地時間（見 pcf85063.h），gmtime_r 不會再套時區。
    const time_t t = static_cast<time_t>(e);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d:%02d", tm.tm_hour, tm.tm_min);
    lv_label_set_text(s_clock, buf);
}

void onHomeDel(lv_event_t*) {
    if (s_clockTimer != nullptr) {
        lv_timer_del(s_clockTimer);
        s_clockTimer = nullptr;
    }
    s_clock = nullptr;
}
void onHistory(lv_event_t*) { showHistory(); }

}  // namespace

void showHome() {
    setCurrentScreen(ScreenId::Home);
    lv_obj_t* s = makeScreen();

    // 整體下移 20：原本內容集中在 -133~74，視覺重心比螢幕中心高 30px，
    // 下半部空一大片。下移之後按鈕列外角 165.3，仍在 kSafeR(166) 內。
    makeLabel(s, "BEYBLADE X", &font_tc_30, colAccent(), 0, -95);
    makeLabel(s, "BATTLE SCORE", &font_tc_22, colSubtle(), 0, -65);

    // 2×2 配置。四個按鈕的外角都落在安全圓內：
    // 最遠角 (136, 74) 距圓心 155 < kSafeR(166)。
    constexpr lv_coord_t kW = 132;
    constexpr lv_coord_t kH = 54;
    constexpr lv_coord_t kDx = 70;

    makeButton(s, "開始比賽", -kDx, 5, kW, kH, colP1(), onStart, nullptr);
    makeButton(s, "快速對戰", kDx, 5, kW, kH, colP2(), onQuick, nullptr);
    makeButton(s, "設定", -kDx, 67, kW, kH, colMuted(), onSettings, nullptr);
    makeButton(s, "歷史紀錄", kDx, 67, kW, kH, colMuted(), onHistory, nullptr);

    // 底部時鐘。下半部原本整片空著，而 RTC 已經在跑了 —— 比賽時想知道幾點
    // 不用再拿手機。RTC 讀不到（振盪器停過）時 nowEpoch() 回 0，那就不顯示，
    // 不要印一個 1970 的時間。
    if (nowEpoch() != 0) {
        s_clock = makeLabel(s, "", &font_tc_22, colSubtle(), 0, 130);
        lv_obj_add_event_cb(s, onHomeDel, LV_EVENT_DELETE, nullptr);
        updateClock(nullptr);
        s_clockTimer = lv_timer_create(updateClock, 10000, nullptr);
    }

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
