#include "quick_panel.h"

#include "../app/app.h"
#include "../app/feedback.h"
#include "../app/settings_store.h"
#include "Display_ST77916.h"
#include "audio/audio_bus.h"
#include "ui_theme.h"

namespace bey {
namespace ui {
namespace {

lv_obj_t* s_panel = nullptr;
lv_obj_t* s_brightVal = nullptr;
lv_obj_t* s_volVal = nullptr;

void onBrightness(lv_event_t* e) {
    const int v = lv_slider_get_value(lv_event_get_target(e));
    g_store.mutableSettings().brightness = static_cast<uint8_t>(v);
    Set_Backlight(static_cast<uint8_t>(v));  // 立刻套用，否則只是在看數字動
    lv_label_set_text_fmt(s_brightVal, "%d", v);
}

void onVolume(lv_event_t* e) {
    const int v = lv_slider_get_value(lv_event_get_target(e));
    g_store.mutableSettings().volume = static_cast<uint8_t>(v);
    audioBusSetVolume(static_cast<uint8_t>(v));
    lv_label_set_text_fmt(s_volVal, "%d", v);
    feedbackPlay(Sfx::Tick);  // 出一聲才知道調到哪
}

void onClose(lv_event_t*) { quickPanelClose(); }

// 滑桿加上它的標籤與數值。dy 是滑桿本身的位置，標籤在上面 26px。
lv_obj_t* makeSlider(lv_obj_t* parent, const char* label, lv_coord_t dy,
                     int min, int max, int value, lv_event_cb_t cb,
                     lv_obj_t** valueLabel) {
    makeLabel(parent, label, &font_tc_16, colSubtle(), -70, dy - 26);

    *valueLabel = makeLabel(parent, "", &font_tc_16, colText(), 78, dy - 26);
    lv_label_set_text_fmt(*valueLabel, "%d", value);

    lv_obj_t* s = lv_slider_create(parent);
    lv_obj_set_size(s, 200, 12);
    lv_obj_align(s, LV_ALIGN_CENTER, 0, dy);
    lv_slider_set_range(s, min, max);
    lv_slider_set_value(s, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s, colMuted(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s, colP1(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, colText(), LV_PART_KNOB);
    lv_obj_add_event_cb(s, cb, LV_EVENT_VALUE_CHANGED, nullptr);
    return s;
}

}  // namespace

bool quickPanelIsOpen() { return s_panel != nullptr; }

void quickPanelOpen() {
    if (s_panel != nullptr) {
        return;
    }

    // 全螢幕半透明底：同時當遮罩用，擋住底下畫面的觸控。
    lv_obj_t* o = lv_obj_create(lv_layer_top());
    lv_obj_set_size(o, kScreenW, kScreenH);
    lv_obj_center(o);
    lv_obj_set_style_bg_color(o, colBg(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    s_panel = o;

    const AppSettings& s = g_store.settings();
    makeSlider(o, "亮度", -30, 10, 100, s.brightness, onBrightness, &s_brightVal);
    makeSlider(o, "音量", 40, 0, 100, s.volume, onVolume, &s_volVal);

    makeButton(o, "關閉", 0, 118, 120, 48, colMuted(), onClose, nullptr,
               &font_tc_16);
}

void quickPanelClose() {
    if (s_panel == nullptr) {
        return;
    }
    // 與 screen_overlay 同一個理由：close 會從面板內按鈕的事件回呼被呼叫，
    // 同步 lv_obj_del 會刪掉正在派送事件的物件的祖先。
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_del_async(s_panel);
    s_panel = nullptr;
    s_brightVal = nullptr;
    s_volVal = nullptr;

    // 面板關掉才存：拖滑桿的過程會產生很多次 VALUE_CHANGED，
    // 每次都寫 NVS 是在磨損 flash。
    g_store.save();
}

}  // namespace ui
}  // namespace bey
