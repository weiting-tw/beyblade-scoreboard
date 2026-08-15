// 頁面五：主計分（規格第 6 節）
#include <cstdio>

#include "../app/app.h"
#include "../app/feedback.h"
#include "status_chip.h"
#include "ui.h"
#include "ui_theme.h"

namespace bey {
namespace ui {

namespace {

Voice announceWho(ResultType r) {
    switch (r) {
        case ResultType::P1Spin:
        case ResultType::P1Over:
        case ResultType::P1Burst:
        case ResultType::P1Xtreme:
            return Voice::PlayerOne;
        case ResultType::P2Spin:
        case ResultType::P2Over:
        case ResultType::P2Burst:
        case ResultType::P2Xtreme:
            return Voice::PlayerTwo;
        default:
            return Voice::None;  // 雙出局／無效局沒有得分者
    }
}

Voice announceWhat(ResultType r) {
    switch (r) {
        case ResultType::P1Over:
        case ResultType::P2Over:
            return Voice::OverFinish;
        case ResultType::P1Burst:
        case ResultType::P2Burst:
            return Voice::BurstFinish;
        case ResultType::P1Xtreme:
        case ResultType::P2Xtreme:
            return Voice::XtremeFinish;
        case ResultType::P1Spin:
        case ResultType::P2Spin:
            return Voice::SpinFinish;
        default:
            return Voice::None;
    }
}

}  // namespace

// 套用一次結果後決定下一個畫面。screen_round / screen_overlay 也會用到。
void applyResultAndAdvance(ResultType r) {
    if (!g_match.applyResult(r)) {
        return;
    }
    switch (r) {
        // 場外與爆裂同為 2 分，共用同一組音階；音效本來就是照分數高低設計的。
        case ResultType::P1Over:
        case ResultType::P2Over:
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
        // 先播這一分怎麼拿的，再播誰贏 ——「Xtreme Finish, Player One, Wins」。
        // 決勝的那一分往往是整場最精彩的（極限勝一次三分直接結束），
        // 只播勝者等於把它吞掉。三段放同一個佇列項目，不會被拆散。
        switch (g_match.winner()) {
            case Winner::P1:
                feedbackAnnounce(announceWhat(r), Voice::PlayerOne, Voice::Wins);
                break;
            case Winner::P2:
                feedbackAnnounce(announceWhat(r), Voice::PlayerTwo, Voice::Wins);
                break;
            default:
                break;  // 平手沒有錄對應的片段，維持只有勝利音效
        }
        showComplete();
    } else {
        feedbackAnnounce(announceWho(r), announceWhat(r));
        // 開了播報就不再疊局結束提示音，兩個接在一起太吵。
        if (!g_store.settings().enableAnnounce) {
            feedbackPlay(Sfx::RoundEnd);
        }
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
void onP1LongPress(lv_event_t*) { applyResultAndAdvance(ResultType::P1Spin); }
void onP2LongPress(lv_event_t*) { applyResultAndAdvance(ResultType::P2Spin); }

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

void doEnd() {
    // 跟局結果頁的「結束」同一個意思：依目前比分判勝負並記錄。
    // 但 0:0 沒有任何比賽內容，記一場平手進歷史紀錄只是雜訊，直接回首頁。
    if (g_match.scoreP1() == 0 && g_match.scoreP2() == 0) {
        g_match.reset();
        showHome();
        return;
    }
    g_match.forceFinish();
    recordFinishedMatch();
    showComplete();
}

// 沒有東西可撤銷時，這個位置改放「結束」。仍然要二次確認 —— 撤銷步數用完
// （堆疊只有五步）時分數不見得是 0，誤觸就整場沒了。
void onEnd(lv_event_t*) { showConfirmOverlay("結束比賽？", doEnd); }

void onReset(lv_event_t*) {
    // 規格第 9 節：重設必須二次確認。
    showConfirmOverlay("重設比賽？", doReset);
}

// 本局計時。Label 屬於畫面，畫面重建時會被 auto_del 釋放，所以 timer 必須
// 跟著畫面一起收掉 —— 不然它會繼續對著已經釋放的記憶體寫字。
lv_obj_t* s_clock = nullptr;
lv_timer_t* s_clockTimer = nullptr;

void updateClock(lv_timer_t*) {
    if (s_clock == nullptr) {
        return;
    }
    const uint32_t sec = roundElapsedMs() / 1000;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u:%02u", static_cast<unsigned>(sec / 60),
                  static_cast<unsigned>(sec % 60));
    lv_label_set_text(s_clock, buf);
}

void onScoreScreenDel(lv_event_t*) {
    if (s_clockTimer != nullptr) {
        lv_timer_del(s_clockTimer);
        s_clockTimer = nullptr;
    }
    s_clock = nullptr;
}

// 覆蓋在分數上的透明長按區。
void makeLongPressArea(lv_obj_t* parent, lv_coord_t dx, lv_event_cb_t cb) {
    lv_obj_t* area = lv_obj_create(parent);
    // 原本是 150x110 @ dy -75，外角距圓心 200.8 —— 有一大塊落在面板（半徑 180）
    // 之外，而且下緣蓋到狀態列底部（-121.5）8.5px，長按那條帶會誤加分。
    // 收成 120x64 @ dy -60 後外角 165.9 < kSafeR(166)，範圍剛好包住比分數字。
    // 代價：不再涵蓋上方的玩家名，長按名字不會加分了。
    lv_obj_set_size(area, 120, 64);
    lv_obj_align(area, LV_ALIGN_CENTER, dx, -60);
    lv_obj_set_style_bg_opa(area, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(area, 0, LV_PART_MAIN);
    lv_obj_clear_flag(area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(area, cb, LV_EVENT_LONG_PRESSED, nullptr);
}

}  // namespace

void showScore(Nav nav) {
    setCurrentScreen(ScreenId::Score);
    const MatchConfig& cfg = g_match.config();

    lv_obj_t* s = makeScreen();

    char buf[40];
    std::snprintf(buf, sizeof(buf), "第 %d 局    %d 分制", g_match.round(),
                  cfg.targetScore);
    makeLabel(s, buf, &font_tc_22, colSubtle(), 0, -135);

    // 玩家名稱與分數
    // 寬度 88 是量出來的：font_tc_16 下八個 ASCII 字元剛好 88px，六個漢字 66px。
    // 原本設 140 是估的，容器外角距圓心 189，超出面板半徑 180（雖然文字置中後
    // 沒被切到，超出去的是空白）。88 + dy -103 讓外角落在 166.0 = kSafeR。
    // 更長的名字仍會被 LONG_DOT 截斷 —— 140 的時候也會，只是門檻不同。
    lv_obj_t* n1 = makeLabel(s, cfg.p1Name, &font_tc_16, colP1(), -78, -103);
    lv_obj_set_width(n1, 88);
    lv_label_set_long_mode(n1, LV_LABEL_LONG_DOT);
    lv_obj_align(n1, LV_ALIGN_CENTER, -78, -103);

    lv_obj_t* n2 = makeLabel(s, cfg.p2Name, &font_tc_16, colP2(), 78, -103);
    lv_obj_set_width(n2, 88);
    lv_label_set_long_mode(n2, LV_LABEL_LONG_DOT);
    lv_obj_align(n2, LV_ALIGN_CENTER, 78, -103);

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

    // 第一局沒有任何東西可以撤銷，放一顆永遠按不下去的鈕只是佔位置。
    // 這時改放「結束」，讓這個位置在任何情況下都有用。
    if (g_match.undoAvailable() > 0) {
        makeButton(s, "撤銷", -55, 95, 104, 48, colMuted(), onUndo, nullptr,
                   &font_tc_16);
    } else {
        makeButton(s, "結束", -55, 95, 104, 48, colMuted(), onEnd, nullptr,
                   &font_tc_16);
    }
    makeButton(s, "重設", 55, 95, 104, 48, colDanger(), onReset, nullptr,
               &font_tc_16);

    // 本局計時放最底下。dy 143、字高 19，外角距圓心 152.7，在 kSafeR 內；
    // 用 montserrat 的數字與冒號，不新增任何中文字。
    // colMuted 是設計來當背景的（0x2C2C3A），拿來寫字在黑底上幾乎看不見。
    s_clock = makeLabel(s, "0:00", &lv_font_montserrat_14, colSubtle(), 0, 143);
    lv_obj_add_event_cb(s, onScoreScreenDel, LV_EVENT_DELETE, nullptr);
    updateClock(nullptr);
    s_clockTimer = lv_timer_create(updateClock, 500, nullptr);

    loadScreen(s, nav);
    // 頂部弧帶留給這頁自己的內容，把狀態晶片收起來。
    // 低電量時 statusChipSetHidden 會忽略這個要求。
    statusChipSetHidden(true);
}

}  // namespace ui
}  // namespace bey
