// 頁面五：主計分（規格第 6 節）
#include <cstdio>

#include "../app/app.h"
#include "../app/feedback.h"
#include "ui.h"
#include "ui_theme.h"

namespace bey {
namespace ui {

// 套用一次結果後決定下一個畫面。screen_round / screen_overlay 也會用到。
void applyResultAndAdvance(ResultType r) {
    if (!g_match.applyResult(r)) {
        return;
    }
    switch (r) {
        case ResultType::P1Burst:
        case ResultType::P2Burst:
            feedbackPlay(Sfx::ScoreBurst);
            break;
        case ResultType::P1Xtreme:
        case ResultType::P2Xtreme:
            feedbackPlay(Sfx::ScoreXtreme);
            break;
        default:
            feedbackPlay(Sfx::ScoreNormal);
            break;
    }
    feedbackHaptic(40);

    if (g_match.finished()) {
        recordFinishedMatch();
        feedbackPlay(Sfx::MatchWin);
        showComplete();
    } else {
        feedbackPlay(Sfx::RoundEnd);
        showRound();
    }
}

namespace {

void onP1Win(lv_event_t*) { showWinTypeOverlay(true); }
void onP2Win(lv_event_t*) { showWinTypeOverlay(false); }

// 長按玩家區域＝該玩家「普通勝利」，點數仍走使用者設定的規則。
void onP1LongPress(lv_event_t*) { applyResultAndAdvance(ResultType::P1Normal); }
void onP2LongPress(lv_event_t*) { applyResultAndAdvance(ResultType::P2Normal); }

void onUndo(lv_event_t*) {
    if (g_match.undo()) {
        feedbackPlay(Sfx::Undo);
        feedbackHaptic(25);
        showScore();  // 重畫以更新分數
    }
}

void doReset() {
    g_match.reset();
    g_match.start();
    showScore();
}

void onReset(lv_event_t*) {
    // 規格第 9 節：重設必須二次確認。
    showConfirmOverlay("RESET MATCH?", doReset);
}

// 覆蓋在分數上的透明長按區。
void makeLongPressArea(lv_obj_t* parent, lv_coord_t dx, lv_event_cb_t cb) {
    lv_obj_t* area = lv_obj_create(parent);
    lv_obj_set_size(area, 150, 110);
    lv_obj_align(area, LV_ALIGN_CENTER, dx, -75);
    lv_obj_set_style_bg_opa(area, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(area, 0, LV_PART_MAIN);
    lv_obj_clear_flag(area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(area, cb, LV_EVENT_LONG_PRESSED, nullptr);
}

}  // namespace

void showScore() {
    const MatchConfig& cfg = g_match.config();

    lv_obj_t* s = makeScreen();

    char buf[40];
    std::snprintf(buf, sizeof(buf), "R%d   TO %d", g_match.round(),
                  cfg.targetScore);
    makeLabel(s, buf, &lv_font_montserrat_20, colSubtle(), 0, -135);

    // 玩家名稱與分數
    lv_obj_t* n1 = makeLabel(s, cfg.p1Name, &lv_font_montserrat_14, colP1(), -78, -108);
    lv_obj_set_width(n1, 140);
    lv_label_set_long_mode(n1, LV_LABEL_LONG_DOT);
    lv_obj_align(n1, LV_ALIGN_CENTER, -78, -108);

    lv_obj_t* n2 = makeLabel(s, cfg.p2Name, &lv_font_montserrat_14, colP2(), 78, -108);
    lv_obj_set_width(n2, 140);
    lv_label_set_long_mode(n2, LV_LABEL_LONG_DOT);
    lv_obj_align(n2, LV_ALIGN_CENTER, 78, -108);

    std::snprintf(buf, sizeof(buf), "%d", g_match.scoreP1());
    makeLabel(s, buf, &lv_font_montserrat_48, colText(), -78, -60);
    std::snprintf(buf, sizeof(buf), "%d", g_match.scoreP2());
    makeLabel(s, buf, &lv_font_montserrat_48, colText(), 78, -60);

    makeLabel(s, ":", &lv_font_montserrat_36, colMuted(), 0, -60);

    // 規格第 6 節：長按玩家區域直接加 1 分。
    makeLongPressArea(s, -78, onP1LongPress);
    makeLongPressArea(s, 78, onP2LongPress);

    makeButton(s, "P1 WIN", -74, 25, 140, 56, colP1(), onP1Win, nullptr);
    makeButton(s, "P2 WIN", 74, 25, 140, 56, colP2(), onP2Win, nullptr);

    lv_obj_t* undo = makeButton(s, "UNDO", -55, 95, 104, 48, colMuted(), onUndo,
                                nullptr, &lv_font_montserrat_14);
    if (g_match.undoAvailable() == 0) {
        lv_obj_add_state(undo, LV_STATE_DISABLED);
    }
    makeButton(s, "RESET", 55, 95, 104, 48, colDanger(), onReset, nullptr,
               &lv_font_montserrat_14);

    loadScreen(s);
}

}  // namespace ui
}  // namespace bey
