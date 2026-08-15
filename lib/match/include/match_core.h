// Beyblade X 計分核心
//
// 刻意不含任何 Arduino / ESP-IDF 標頭，讓這一層能在主機端用 `pio test -e native`
// 直接驗證。UI 與持久化都建立在這個核心之上。
//
// 規格對應：docs/SPEC.md 第 4、5、7 節。
#pragma once

#include <cstddef>
#include <cstdint>

namespace bey {

constexpr int kNameLen = 20;   // 玩家名稱含結尾 '\0'
constexpr int kUndoDepth = 8;  // 規格要求「至少 5 次」，取 8 留餘裕

// 比賽勝者。數值與 docs/SPEC.md 第 4 節的 MatchState::winner 對齊。
enum class Winner : uint8_t {
    None = 0,  // 尚未結束
    P1 = 1,
    P2 = 2,
    Draw = 3,
};

// 單局結果類型。
//
// 規格第 5 節要求「不得硬編碼成唯一模式」：這裡只定義**有哪些結果**，
// 每種結果各給幾分完全由 RuleSet 決定，使用者可在設定頁改。
enum class ResultType : uint8_t {
    P1Normal = 0,  // P1 普通勝利
    P1Burst,       // P1 爆裂
    P1Xtreme,      // P1 Xtreme Finish
    P2Normal,
    P2Burst,
    P2Xtreme,
    DoubleOut,  // 雙方同時出界
    NoContest,  // 無效局
    Count
};

constexpr int kResultCount = static_cast<int>(ResultType::Count);

// 一種結果對應的加分方式。
struct ScoreRule {
    char name[24];
    int8_t p1Points;
    int8_t p2Points;
    bool countsAsRound;  // 無效局不推進局數
};

// 全部結果的點數設定。可整包存進 NVS，也可整包還原預設。
struct RuleSet {
    ScoreRule rules[kResultCount];
};

// 規格第 5 節的預設值：普通 +1、爆裂 +2、Xtreme +3。
RuleSet defaultRuleSet();

struct MatchConfig {
    int targetScore;  // 目標分數，預設 3
    int maxRounds;    // 0 = 不限局數
    bool enableSound;
    bool saveHistory;
    char p1Name[kNameLen];
    char p2Name[kNameLen];
};

MatchConfig defaultConfig();

// 一次計分操作的快照，用於撤銷。
struct ScoreEvent {
    int roundNumber;
    ResultType result;
    int prevP1;
    int prevP2;
    int prevRound;
    bool prevFinished;
    Winner prevWinner;
};

class Match {
   public:
    Match();

    // 設定賽制與點數規則。會一併 reset()。
    void configure(const MatchConfig& cfg, const RuleSet& rules);

    void start();   // 進入比賽中（倒數結束後呼叫）
    void reset();   // 分數歸零、清空撤銷堆疊、回到未開始

    // 套用一次結果。比賽已結束或尚未開始時回傳 false 且不改變狀態。
    bool applyResult(ResultType result);

    // 撤銷最近一次計分。堆疊為空時回傳 false。
    bool undo();

    // 使用者按「結束比賽」時提前收攤，依目前分數判定勝負。
    // 尚未開始或已結束時不做事。
    void forceFinish();

    int scoreP1() const { return p1_; }
    int scoreP2() const { return p2_; }
    int round() const { return round_; }
    bool started() const { return started_; }
    bool finished() const { return finished_; }
    Winner winner() const { return winner_; }
    int undoAvailable() const { return histCount_; }

    const MatchConfig& config() const { return cfg_; }
    const RuleSet& ruleSet() const { return rules_; }
    const ScoreRule& rule(ResultType r) const {
        return rules_.rules[static_cast<int>(r)];
    }

    // 勝者的顯示名稱；未分出勝負時回傳 nullptr。
    const char* winnerName() const;

    // 最近一次計分事件；尚未計分過時回傳 nullptr。
    // 局結果畫面靠它顯示「剛才是誰、用什麼方式、得幾分」。
    const ScoreEvent* lastEvent() const {
        return histCount_ > 0 ? &hist_[histCount_ - 1] : nullptr;
    }

   private:
    void pushHistory(const ScoreEvent& e);
    void evaluate();      // 依目前分數與局數判定是否結束
    void decideByScore();  // 分數高者勝，同分為平手

    MatchConfig cfg_;
    RuleSet rules_;

    int p1_;
    int p2_;
    int round_;
    bool started_;
    bool finished_;
    Winner winner_;

    ScoreEvent hist_[kUndoDepth];
    int histCount_;
};

}  // namespace bey
