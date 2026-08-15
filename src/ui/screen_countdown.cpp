// 頁面四：倒數 3-2-1-GO（規格第 6 節）
#include "../app/app.h"
#include "../app/feedback.h"
#include "status_chip.h"
#include "ui.h"
#include "ui_theme.h"

namespace bey {
namespace ui {
namespace {

lv_timer_t* s_timer = nullptr;
lv_obj_t* s_num = nullptr;
lv_obj_t* s_arc = nullptr;
int s_value = 3;

void killTimer() {
    if (s_timer != nullptr) {
        lv_timer_del(s_timer);
        s_timer = nullptr;
    }
}

// 畫面若因任何原因被釋放（例如切到別頁），計時器必須一起收掉，
// 否則下一次 tick 會操作已釋放的 label。
void onScreenDel(lv_event_t*) {
    killTimer();
    s_num = nullptr;
    s_arc = nullptr;
}

void tick(lv_timer_t*) {
    --s_value;

    if (s_value > 0) {
        char buf[4];
        buf[0] = static_cast<char>('0' + s_value);
        buf[1] = '\0';
        lv_label_set_text(s_num, buf);
        // 每個數字從 1.6 倍收縮到原尺寸，像是「砸」在畫面上。
        animPop(s_num, 410, 256, 320);
        feedbackPlay(s_value == 2 ? Sfx::Count2 : Sfx::Count1);
        feedbackHaptic(30);
        return;
    }

    if (s_value == 0) {
        lv_label_set_text(s_num, "GO!");
        lv_obj_set_style_text_color(s_num, colAccent(), LV_PART_MAIN);
        // GO! 反過來由小放大，配合過衝路徑做出爆發感。
        animPop(s_num, 90, 256, 420);
        if (s_arc != nullptr) {
            lv_obj_set_style_arc_color(s_arc, colAccent(), LV_PART_INDICATOR);
        }
        animRingBurst(lv_obj_get_parent(s_num), colAccent(), 600);
        feedbackPlay(Sfx::Go);
        feedbackHaptic(120);
        return;
    }

    // GO! 顯示滿一秒後進場。
    killTimer();
    g_match.start();
    showScore(Nav::Fade);
}

}  // namespace

void showCountdown() {
    setCurrentScreen(ScreenId::Countdown);
    // 上一局的勝利播報可能還在播。倒數有自己的時間軸（一秒一拍），
    // 讓它排在後面的話畫面已經在數秒、聲音還落後一大截。
    feedbackStop();
    killTimer();
    s_value = 3;

    lv_obj_t* s = makeScreen();
    lv_obj_add_event_cb(s, onScreenDel, LV_EVENT_DELETE, nullptr);

    // 圓形進度環：直接吃滿圓形螢幕的形狀優勢。
    s_arc = lv_arc_create(s);
    lv_obj_set_size(s_arc, 300, 300);
    lv_obj_center(s_arc);
    lv_arc_set_rotation(s_arc, 270);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_value(s_arc, 100);
    lv_obj_remove_style(s_arc, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, colMuted(), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, colP2(), LV_PART_INDICATOR);

    s_num = makeLabel(s, "3", &lv_font_montserrat_48, colText(), 0, 0);
    animPop(s_num, 410, 256, 320);  // 第一個 3 也要有，否則第一拍會少一次

    // 進度環在 3 秒內走完一圈。
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_arc);
    lv_anim_set_values(&a, 100, 0);
    lv_anim_set_time(&a, 3000);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
        lv_arc_set_value(static_cast<lv_obj_t*>(obj), v);
    });
    lv_anim_start(&a);

    feedbackPlay(Sfx::Count3);
    feedbackHaptic(30);

    s_timer = lv_timer_create(tick, 1000, nullptr);

    loadScreen(s, Nav::Fade);
    // 頂部弧帶留給這頁自己的內容，把狀態晶片收起來。
    // 低電量時 statusChipSetHidden 會忽略這個要求。
    statusChipSetHidden(true);
}

}  // namespace ui
}  // namespace bey
