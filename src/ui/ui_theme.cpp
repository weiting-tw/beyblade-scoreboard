#include "ui_theme.h"

#include <cmath>

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

void loadScreen(lv_obj_t* scr) {
    // auto_del = true：舊畫面連同其子物件一併釋放。
    // LV_MEM_SIZE 只有 64KB，畫面必須用完即丟。
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_IN, 180, 0, true);
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
        font = &lv_font_montserrat_20;
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
    lv_obj_set_style_transition(btn, nullptr, LV_PART_MAIN);

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
