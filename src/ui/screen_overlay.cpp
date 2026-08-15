// 疊層對話框：勝利類型選單與二次確認（規格第 6、9 節）
//
// 兩者都建在 lv_layer_top() 而不是新畫面，因為底下的計分畫面必須保留 ——
// 使用者按「取消」時要看到原本的比分，不能重畫。
// 代價是 lv_layer_top 不會隨畫面切換自動清空，每個出口都必須明確 close()。
#include <cstdio>

#include "../app/app.h"
#include "ui.h"
#include "ui_theme.h"

namespace bey {
namespace ui {
namespace {

lv_obj_t* s_overlay = nullptr;
void (*s_onConfirm)() = nullptr;

// close() 一律從疊層內按鈕的事件回呼中被呼叫，此時同步 lv_obj_del 會刪掉
// 正在派送事件的物件的祖先，導致 LVGL 回到已釋放的記憶體。
// 因此先隱藏（使用者立刻看不到），再交給 LVGL 在下一輪 timer handler 釋放。
void close() {
    if (s_overlay != nullptr) {
        lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_del_async(s_overlay);
        s_overlay = nullptr;
    }
}

// 半透明遮罩 + 全螢幕容器。
lv_obj_t* openOverlay() {
    close();
    lv_obj_t* o = lv_obj_create(lv_layer_top());
    lv_obj_set_size(o, kScreenW, kScreenH);
    lv_obj_center(o);
    lv_obj_set_style_bg_color(o, colBg(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    s_overlay = o;
    return o;
}

void onCancel(lv_event_t*) { close(); }

void onPickResult(lv_event_t* e) {
    const auto r = static_cast<ResultType>(
        reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    close();  // 必須先關掉，applyResultAndAdvance 會切換畫面
    applyResultAndAdvance(r);
}

void onConfirm(lv_event_t*) {
    void (*fn)() = s_onConfirm;
    close();
    if (fn != nullptr) {
        fn();
    }
}

}  // namespace

void showWinTypeOverlay(bool forPlayer1) {
    lv_obj_t* o = openOverlay();

    const lv_color_t col = forPlayer1 ? colP1() : colP2();
    const MatchConfig& cfg = g_match.config();

    char title[40];
    std::snprintf(title, sizeof(title), "%s WINS BY",
                  forPlayer1 ? cfg.p1Name : cfg.p2Name);
    lv_obj_t* t = makeLabel(o, title, &lv_font_montserrat_20, col, 0, -120);
    lv_obj_set_width(t, 240);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -120);

    const ResultType results[3] = {
        forPlayer1 ? ResultType::P1Normal : ResultType::P2Normal,
        forPlayer1 ? ResultType::P1Burst : ResultType::P2Burst,
        forPlayer1 ? ResultType::P1Xtreme : ResultType::P2Xtreme,
    };
    const char* names[3] = {"NORMAL", "BURST", "XTREME"};
    const lv_coord_t ys[3] = {-55, 5, 65};

    for (int i = 0; i < 3; ++i) {
        // 按鈕上直接顯示目前設定的點數，改過設定一眼就看得出來。
        const ScoreRule& rule = g_match.rule(results[i]);
        const int pts = forPlayer1 ? rule.p1Points : rule.p2Points;
        char label[32];
        std::snprintf(label, sizeof(label), "%s  +%d", names[i], pts);
        makeButton(o, label, 0, ys[i], 200, 52, col, onPickResult,
                   reinterpret_cast<void*>(static_cast<intptr_t>(results[i])));
    }

    makeButton(o, "CANCEL", 0, 125, 120, 48, colMuted(), onCancel, nullptr,
               &lv_font_montserrat_14);
}

void showConfirmOverlay(const char* message, void (*confirmFn)()) {
    s_onConfirm = confirmFn;
    lv_obj_t* o = openOverlay();

    makeLabel(o, message, &lv_font_montserrat_28, colText(), 0, -40);

    makeButton(o, "CANCEL", -60, 60, 110, 52, colMuted(), onCancel, nullptr,
               &lv_font_montserrat_14);
    makeButton(o, "YES", 60, 60, 110, 52, colDanger(), onConfirm, nullptr);
}

}  // namespace ui
}  // namespace bey
