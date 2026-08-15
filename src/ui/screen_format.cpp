// 頁面二：賽制選擇（規格第 6 節）
//
// 預設 1 / 3 / 4 / 5 分制，加上 −/+ 微調即涵蓋規格的「自訂」需求，
// 而且畫面上只有一個目標分數，不會出現預設與自訂互相矛盾的狀態。
#include <cstdio>

#include "../app/app.h"
#include "ui.h"
#include "ui_theme.h"

namespace bey {
namespace ui {
namespace {

constexpr int kMinTarget = 1;
constexpr int kMaxTarget = 20;

int s_target = 3;
lv_obj_t* s_targetLabel = nullptr;

void refresh() {
    if (s_targetLabel == nullptr) {
        return;
    }
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d", s_target);
    lv_label_set_text(s_targetLabel, buf);
}

void setTarget(int v) {
    if (v < kMinTarget) {
        v = kMinTarget;
    }
    if (v > kMaxTarget) {
        v = kMaxTarget;
    }
    s_target = v;
    refresh();
}

void onDec(lv_event_t*) { setTarget(s_target - 1); }
void onInc(lv_event_t*) { setTarget(s_target + 1); }

void onPreset(lv_event_t* e) {
    const int v = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    setTarget(v);
}

void onBack(lv_event_t*) { showHome(); }

// 畫面被 LVGL 釋放時清掉指標，避免留下懸空指標。
void onScreenDel(lv_event_t*) { s_targetLabel = nullptr; }

void onNext(lv_event_t*) {
    // 賽制寫回設定並存檔，「快速對戰」下次才能沿用。
    AppSettings& s = g_store.mutableSettings();
    s.match.targetScore = s_target;
    g_store.save();
    applySettingsToMatch();
    showReady();
}

}  // namespace

void showFormat() {
    s_target = g_store.settings().match.targetScore;
    if (s_target < kMinTarget) {
        s_target = kMinTarget;
    }

    lv_obj_t* s = makeScreen();
    lv_obj_add_event_cb(s, onScreenDel, LV_EVENT_DELETE, nullptr);
    makeLabel(s, "賽制", &font_tc_22, colAccent(), 0, -135);

    s_targetLabel = makeLabel(s, "3", &lv_font_montserrat_48, colText(), 0, -75);
    refresh();
    makeLabel(s, "勝利分數", &font_tc_22, colSubtle(), 0, -30);

    makeButton(s, "-", -100, -75, 56, 56, colMuted(), onDec, nullptr);
    makeButton(s, "+", 100, -75, 56, 56, colMuted(), onInc, nullptr);

    // 常用賽制捷徑。外角 (134, 53) 距圓心 144 < kSafeR。
    const int presets[4] = {1, 3, 4, 5};
    const lv_coord_t xs[4] = {-102, -34, 34, 102};
    for (int i = 0; i < 4; ++i) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", presets[i]);
        makeButton(s, buf, xs[i], 25, 64, 56, colP1(), onPreset,
                   reinterpret_cast<void*>(static_cast<intptr_t>(presets[i])));
    }

    makeButton(s, "返回", -52, 100, 100, 48, colMuted(), onBack, nullptr);
    makeButton(s, "下一步", 52, 100, 100, 48, colP2(), onNext, nullptr);

    loadScreen(s, Nav::Forward);
}

}  // namespace ui
}  // namespace bey
