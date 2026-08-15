// 頁面八：設定（規格第 6 節）
//
// 圓形螢幕放不下十來個設定項，因此用一個 230×210 的可捲動容器承載。
// 容器四角 (115, 105) 距圓心 156 < kSafeR，捲動時內容不會被圓周切到。
//
// NVS 只在離開本頁時寫入一次，避免每按一下 +/- 就寫一次 flash。
#include <cstdio>

#include "../app/app.h"
#include "Display_ST77916.h"  // Set_Backlight()
#include "ui.h"
#include "ui_theme.h"

namespace bey {
namespace ui {
namespace {

enum class Field : uint8_t {
    Target,
    Normal,
    Burst,
    Xtreme,
    Brightness,
    Count,
};

struct StepAction {
    Field field;
    int8_t delta;
};

// 每個欄位一組 ±1，位址固定，可安全當成 event user_data。
const StepAction kSteps[] = {
    {Field::Target, -1},     {Field::Target, +1},
    {Field::Normal, -1},     {Field::Normal, +1},
    {Field::Burst, -1},      {Field::Burst, +1},
    {Field::Xtreme, -1},     {Field::Xtreme, +1},
    {Field::Brightness, -5}, {Field::Brightness, +5},
};

lv_obj_t* s_valueLabels[static_cast<int>(Field::Count)] = {nullptr};

void onScreenDel(lv_event_t*) {
    for (auto& p : s_valueLabels) {
        p = nullptr;
    }
}

int8_t* pointsOf(Field f, AppSettings& s) {
    switch (f) {
        case Field::Normal:
            return &s.rules.rules[static_cast<int>(ResultType::P1Normal)].p1Points;
        case Field::Burst:
            return &s.rules.rules[static_cast<int>(ResultType::P1Burst)].p1Points;
        case Field::Xtreme:
            return &s.rules.rules[static_cast<int>(ResultType::P1Xtreme)].p1Points;
        default:
            return nullptr;
    }
}

int readField(Field f) {
    AppSettings& s = g_store.mutableSettings();
    switch (f) {
        case Field::Target:
            return s.match.targetScore;
        case Field::Brightness:
            return s.brightness;
        default: {
            const int8_t* p = pointsOf(f, s);
            return (p != nullptr) ? *p : 0;
        }
    }
}

void writeField(Field f, int v) {
    AppSettings& s = g_store.mutableSettings();
    switch (f) {
        case Field::Target:
            s.match.targetScore = (v < 1) ? 1 : ((v > 20) ? 20 : v);
            break;
        case Field::Brightness: {
            const int b = (v < 10) ? 10 : ((v > 100) ? 100 : v);
            s.brightness = static_cast<uint8_t>(b);
            Set_Backlight(static_cast<uint8_t>(b));  // 立即套用，看得到效果
            break;
        }
        default: {
            int8_t* p = pointsOf(f, s);
            if (p == nullptr) {
                return;
            }
            const int c = (v < 0) ? 0 : ((v > 9) ? 9 : v);
            *p = static_cast<int8_t>(c);
            // P1/P2 的同名結果保持對稱，否則兩位玩家規則會不一致。
            ResultType mirror = ResultType::P2Normal;
            if (f == Field::Burst) {
                mirror = ResultType::P2Burst;
            } else if (f == Field::Xtreme) {
                mirror = ResultType::P2Xtreme;
            }
            s.rules.rules[static_cast<int>(mirror)].p2Points = static_cast<int8_t>(c);
            break;
        }
    }
}

void refresh(Field f) {
    lv_obj_t* lbl = s_valueLabels[static_cast<int>(f)];
    if (lbl == nullptr) {
        return;
    }
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d", readField(f));
    lv_label_set_text(lbl, buf);
}

void onStep(lv_event_t* e) {
    const auto* a = static_cast<const StepAction*>(lv_event_get_user_data(e));
    writeField(a->field, readField(a->field) + a->delta);
    refresh(a->field);
}

void onToggleSound(lv_event_t* e) {
    g_store.mutableSettings().match.enableSound =
        lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
}
void onToggleVibration(lv_event_t* e) {
    g_store.mutableSettings().enableVibration =
        lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
}
void onToggleHistory(lv_event_t* e) {
    g_store.mutableSettings().match.saveHistory =
        lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
}

void doClearHistory() {
    g_store.clearHistory();
    showSettings();
}
void onClearHistory(lv_event_t*) { showConfirmOverlay("CLEAR HISTORY?", doClearHistory); }

void doRestoreDefaults() {
    g_store.restoreDefaults();
    Set_Backlight(g_store.settings().brightness);
    applySettingsToMatch();
    showSettings();
}
void onRestoreDefaults(lv_event_t*) {
    showConfirmOverlay("RESTORE DEFAULTS?", doRestoreDefaults);
}

void onBack(lv_event_t*) {
    g_store.save();  // 整頁的變更在此一次寫入 NVS
    applySettingsToMatch();
    showHome();
}

// --- 列的建構 ---------------------------------------------------------

lv_obj_t* makeRow(lv_obj_t* parent) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, 206, 44);
    lv_obj_set_style_bg_color(row, colMuted(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(row, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 4, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

lv_obj_t* rowLabel(lv_obj_t* row, const char* text) {
    lv_obj_t* l = lv_label_create(row);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, colText(), LV_PART_MAIN);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    return l;
}

// 設定頁的 ± 按鈕比規格第 9 節的 60×45 小：這裡是低頻操作且誤觸代價低，
// 換得一頁能放下全部設定。比分相關按鈕仍維持規格尺寸。
lv_obj_t* stepBtn(lv_obj_t* row, const char* text, lv_coord_t xOfs,
                  const StepAction* action) {
    lv_obj_t* b = lv_btn_create(row);
    lv_obj_set_size(b, 42, 34);
    lv_obj_align(b, LV_ALIGN_RIGHT_MID, xOfs, 0);
    lv_obj_set_style_bg_color(b, colP1(), LV_PART_MAIN);
    lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, onStep, LV_EVENT_CLICKED, const_cast<StepAction*>(action));
    return b;
}

void makeStepperRow(lv_obj_t* parent, const char* text, Field f, int stepIndex) {
    lv_obj_t* row = makeRow(parent);
    rowLabel(row, text);

    stepBtn(row, "-", -96, &kSteps[stepIndex]);
    stepBtn(row, "+", 0, &kSteps[stepIndex + 1]);

    lv_obj_t* val = lv_label_create(row);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, colAccent(), LV_PART_MAIN);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, -52, 0);
    s_valueLabels[static_cast<int>(f)] = val;
    refresh(f);
}

void makeSwitchRow(lv_obj_t* parent, const char* text, bool on,
                   lv_event_cb_t cb) {
    lv_obj_t* row = makeRow(parent);
    rowLabel(row, text);
    lv_obj_t* sw = lv_switch_create(row);
    lv_obj_set_size(sw, 50, 28);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(sw, colP2(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (on) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, nullptr);
}

void makeActionRow(lv_obj_t* parent, const char* text, lv_color_t col,
                   lv_event_cb_t cb) {
    lv_obj_t* row = makeRow(parent);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_t* b = lv_btn_create(row);
    lv_obj_set_size(b, 198, 38);
    lv_obj_center(b);
    lv_obj_set_style_bg_color(b, col, LV_PART_MAIN);
    lv_obj_set_style_radius(b, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
}

}  // namespace

void showSettings() {
    const AppSettings& s = g_store.settings();

    lv_obj_t* scr = makeScreen();
    lv_obj_add_event_cb(scr, onScreenDel, LV_EVENT_DELETE, nullptr);
    makeLabel(scr, "SETTINGS", &lv_font_montserrat_20, colAccent(), 0, -145);

    lv_obj_t* cont = lv_obj_create(scr);
    lv_obj_set_size(cont, 230, 210);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, -5);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(cont, 6, LV_PART_MAIN);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);

    makeStepperRow(cont, "TARGET", Field::Target, 0);
    makeStepperRow(cont, "NORMAL", Field::Normal, 2);
    makeStepperRow(cont, "BURST", Field::Burst, 4);
    makeStepperRow(cont, "XTREME", Field::Xtreme, 6);
    makeStepperRow(cont, "BRIGHT", Field::Brightness, 8);

    makeSwitchRow(cont, "SOUND", s.match.enableSound, onToggleSound);
    makeSwitchRow(cont, "VIBRATE", s.enableVibration, onToggleVibration);
    makeSwitchRow(cont, "HISTORY", s.match.saveHistory, onToggleHistory);

    makeActionRow(cont, "CLEAR HISTORY", colDanger(), onClearHistory);
    makeActionRow(cont, "RESTORE DEFAULTS", colDanger(), onRestoreDefaults);

    makeButton(scr, "BACK", 0, 128, 110, 44, colMuted(), onBack, nullptr,
               &lv_font_montserrat_14);

    loadScreen(scr);
}

}  // namespace ui
}  // namespace bey
