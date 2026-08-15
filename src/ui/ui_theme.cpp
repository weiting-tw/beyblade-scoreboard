#include "ui_theme.h"

#include <cmath>

#include "status_chip.h"

namespace bey {
namespace ui {

lv_color_t colBg() { return lv_color_hex(0x08080F); }
lv_color_t colP1() { return lv_color_hex(0x1E90FF); }
lv_color_t colP2() { return lv_color_hex(0xFF3B30); }
lv_color_t colAccent() { return lv_color_hex(0xFFD60A); }
lv_color_t colMuted() { return lv_color_hex(0x2C2C3A); }
lv_color_t colText() { return lv_color_hex(0xF2F2F7); }
lv_color_t colSubtle() { return lv_color_hex(0x8E8E9A); }
lv_color_t colDanger() { return lv_color_hex(0xC0392B); }

lv_coord_t widthAtY(lv_coord_t dy) {
    const float r = static_cast<float>(kSafeR);
    const float y = static_cast<float>(dy < 0 ? -dy : dy);
    if (y >= r) {
        return 0;
    }
    return static_cast<lv_coord_t>(2.0f * std::sqrt(r * r - y * y));
}

lv_coord_t barWidth(lv_coord_t dy, lv_coord_t h) {
    // 長條的上下緣離中心較遠的那一邊決定可用寬度。
    const lv_coord_t top = static_cast<lv_coord_t>(dy - h / 2);
    const lv_coord_t bottom = static_cast<lv_coord_t>(dy + h / 2);
    const lv_coord_t a = widthAtY(top);
    const lv_coord_t b = widthAtY(bottom);
    return a < b ? a : b;
}

lv_obj_t* makeScreen() {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, colBg(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

void loadScreen(lv_obj_t* scr, Nav nav) {
    lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_FADE_IN;
    uint32_t ms = 220;

    switch (nav) {
        case Nav::Forward:
            anim = LV_SCR_LOAD_ANIM_MOVE_LEFT;
            break;
        case Nav::Back:
            anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
            break;
        case Nav::Replace:
            // 撤銷後重畫同一頁。任何位移都會被誤讀成「換頁了」。
            anim = LV_SCR_LOAD_ANIM_NONE;
            ms = 0;
            break;
        case Nav::Rise:
            anim = LV_SCR_LOAD_ANIM_OVER_TOP;
            ms = 320;
            break;
        case Nav::Fade:
            anim = LV_SCR_LOAD_ANIM_FADE_IN;
            break;
    }

    // 預設每個畫面都顯示狀態晶片；需要淨空頂部的畫面（計分、倒數、局結果）
    // 在自己的 show 函式裡載入後再設回隱藏。
    statusChipSetHidden(false);

    // auto_del = true：舊畫面連同其子物件一併釋放。
    // LV_MEM_SIZE 只有 64KB，畫面必須用完即丟。
    lv_scr_load_anim(scr, anim, ms, 0, true);
}

// --- 動畫工具 -----------------------------------------------------------

namespace {

void execZoom(void* obj, int32_t v) {
    lv_obj_set_style_transform_zoom(static_cast<lv_obj_t*>(obj),
                                    static_cast<lv_coord_t>(v), LV_PART_MAIN);
}

void execOpa(void* obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj),
                         static_cast<lv_opa_t>(v), LV_PART_MAIN);
}

void execCount(void* obj, int32_t v) {
    lv_label_set_text_fmt(static_cast<lv_obj_t*>(obj), "%d", static_cast<int>(v));
}

void execRing(void* obj, int32_t v) {
    lv_obj_t* o = static_cast<lv_obj_t*>(obj);
    lv_obj_set_size(o, v, v);
    lv_obj_center(o);
    // 擴散到最大時完全透明，形成「往外散去」而不是「停在那裡」。
    const int32_t fade = 255 - (v * 255) / 340;
    lv_obj_set_style_border_opa(o, static_cast<lv_opa_t>(fade < 0 ? 0 : fade),
                                LV_PART_MAIN);
}

}  // namespace

void animPop(lv_obj_t* obj, uint16_t fromZoom, uint16_t toZoom, uint32_t ms,
             uint32_t delay) {
    // transform 以 pivot 為中心。pivot 預設在左上角，不校正的話物件會一邊
    // 放大一邊往右下漂移。必須先讓 LVGL 算出尺寸才有正確的中心點。
    lv_obj_update_layout(obj);
    lv_obj_set_style_transform_pivot_x(obj, lv_obj_get_width(obj) / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(obj, lv_obj_get_height(obj) / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_zoom(obj, fromZoom, LV_PART_MAIN);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, fromZoom, toZoom);
    lv_anim_set_time(&a, ms);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_exec_cb(&a, execZoom);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_start(&a);
}

void animFadeIn(lv_obj_t* obj, uint32_t ms, uint32_t delay) {
    lv_obj_set_style_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, ms);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_exec_cb(&a, execOpa);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

void animCountUp(lv_obj_t* label, int from, int to, uint32_t ms, uint32_t delay) {
    lv_label_set_text_fmt(label, "%d", from);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, label);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, ms);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_exec_cb(&a, execCount);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

lv_obj_t* animRingBurst(lv_obj_t* parent, lv_color_t color, uint32_t ms,
                        uint32_t delay) {
    lv_obj_t* ring = lv_obj_create(parent);
    lv_obj_set_size(ring, 60, 60);
    lv_obj_center(ring);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_color(ring, color, LV_PART_MAIN);
    lv_obj_set_style_border_width(ring, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ring, 0, LV_PART_MAIN);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    // 光環純裝飾，不能吃掉底下按鈕的觸控。
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_background(ring);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ring);
    lv_anim_set_values(&a, 60, 340);
    lv_anim_set_time(&a, ms);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_exec_cb(&a, execRing);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
    return ring;
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, const lv_font_t* font,
                    lv_color_t color, lv_coord_t dx, lv_coord_t dy) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, color, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_CENTER, dx, dy);
    return lbl;
}

lv_obj_t* makeButton(lv_obj_t* parent, const char* text, lv_coord_t dx,
                     lv_coord_t dy, lv_coord_t w, lv_coord_t h,
                     lv_color_t color, lv_event_cb_t cb, void* userData,
                     const lv_font_t* font) {
    if (font == nullptr) {
        font = &font_tc_22;
    }
    if (w < kMinBtnW) {
        w = kMinBtnW;
    }
    if (h < kMinBtnH) {
        h = kMinBtnH;
    }

    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_align(btn, LV_ALIGN_CENTER, dx, dy);
    lv_obj_set_style_bg_color(btn, color, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, h / 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);

    // 觸控回饋（規格第 9 節）：按下時變暗並縮小。
    lv_obj_set_style_bg_color(btn, lv_color_darken(color, LV_OPA_30),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(btn, -4, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(btn, -4, LV_PART_MAIN | LV_STATE_PRESSED);
    // 這裡曾經寫 lv_obj_set_style_transition(btn, nullptr, LV_PART_MAIN)，
    // 想表達「不要轉場」—— 那會讓按下按鈕必當機。
    // LVGL 在 lv_obj_set_state() 裡的判斷是：
    //     if(lv_style_get_prop_inlined(style, LV_STYLE_TRANSITION, &v) != FOUND) continue;
    //     const lv_style_transition_dsc_t *tr = v.ptr;
    //     for(j = 0; tr->props[j] != 0; j++) ...
    // 設成 nullptr 會讓屬性「存在但值為 NULL」，於是 LVGL 略過 continue
    // 直接解參考 NULL -> LoadProhibited。
    // 要「沒有轉場」的正確做法就是**完全不設這個屬性**。

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, colText(), LV_PART_MAIN);
    lv_obj_center(lbl);

    if (cb != nullptr) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, userData);
    }
    return btn;
}

}  // namespace ui
}  // namespace bey
