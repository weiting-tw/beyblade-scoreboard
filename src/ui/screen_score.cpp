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

// 上一次畫出來的比分。畫面每次都重建，狀態只能留在這裡。
// -1 代表還沒畫過，此時不做任何強調。
int s_shownP1 = -1;
int s_shownP2 = -1;

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
    showConfirmOverlay("重設比賽？", doReset);
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

void showScore(Nav nav) {
    const MatchConfig& cfg = g_match.config();

    lv_obj_t* s = makeScreen();

    char buf[40];
    std::snprintf(buf, sizeof(buf), "第 %d 局    %d 分制", g_match.round(),
                  cfg.targetScore);
    makeLabel(s, buf, &font_tc_22, colSubtle(), 0, -135);

    // 玩家名稱與分數
    lv_obj_t* n1 = makeLabel(s, cfg.p1Name, &font_tc_16, colP1(), -78, -108);
    lv_obj_set_width(n1, 140);
    lv_label_set_long_mode(n1, LV_LABEL_LONG_DOT);
    lv_obj_align(n1, LV_ALIGN_CENTER, -78, -108);

    lv_obj_t* n2 = makeLabel(s, cfg.p2Name, &font_tc_16, colP2(), 78, -108);
    lv_obj_set_width(n2, 140);
    lv_label_set_long_mode(n2, LV_LABEL_LONG_DOT);
    lv_obj_align(n2, LV_ALIGN_CENTER, 78, -108);

    // 只讓「分數有變動」的那一邊彈跳。兩邊都動的話等於沒有指向性，
    // 使用者反而看不出剛才加到誰身上。
    const int p1 = g_match.scoreP1();
    const int p2 = g_match.scoreP2();
    const bool fresh = (g_match.undoAvailable() == 0);  // 全新一場，不強調
    const bool pop1 = !fresh && s_shownP1 >= 0 && s_shownP1 != p1;
    const bool pop2 = !fresh && s_shownP2 >= 0 && s_shownP2 != p2;

    std::snprintf(buf, sizeof(buf), "%d", p1);
    lv_obj_t* sc1 = makeLabel(s, buf, &lv_font_montserrat_48, colText(), -78, -60);
    std::snprintf(buf, sizeof(buf), "%d", p2);
    lv_obj_t* sc2 = makeLabel(s, buf, &lv_font_montserrat_48, colText(), 78, -60);
    if (pop1) {
        animPop(sc1, 380, 256, 360);
    }
    if (pop2) {
        animPop(sc2, 380, 256, 360);
    }
    s_shownP1 = p1;
    s_shownP2 = p2;

    makeLabel(s, ":", &lv_font_montserrat_36, colMuted(), 0, -60);

    // 規格第 6 節：長按玩家區域直接加 1 分。
    makeLongPressArea(s, -78, onP1LongPress);
    makeLongPressArea(s, 78, onP2LongPress);

    makeButton(s, "P1 勝", -74, 25, 140, 56, colP1(), onP1Win, nullptr);
    makeButton(s, "P2 勝", 74, 25, 140, 56, colP2(), onP2Win, nullptr);

    lv_obj_t* undo = makeButton(s, "撤銷", -55, 95, 104, 48, colMuted(), onUndo,
                                nullptr, &font_tc_16);
    if (g_match.undoAvailable() == 0) {
        lv_obj_add_state(undo, LV_STATE_DISABLED);
    }
    makeButton(s, "重設", 55, 95, 104, 48, colDanger(), onReset, nullptr,
               &font_tc_16);

    loadScreen(s, nav);
}

}  // namespace ui
}  // namespace bey
