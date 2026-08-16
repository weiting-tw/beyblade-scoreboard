// Beyblade X 戰鬥計分器 — Waveshare ESP32-S3-Touch-LCD-1.85B
//
// 初始化順序完全照官方 Arduino 範例 01_lvgl_demo：
//   I2C_Init -> Backlight_Init -> LCD_Init -> Lvgl_Init
// LCD_Init() 內部會呼叫 Touch_Init()，因此不需另外初始化 CST816S。
#include <Arduino.h>

#include "Display_ST77916.h"
#include "I2C_Driver.h"
#include "LVGL_Driver.h"
#include "app/app.h"
#include "app/button.h"
#include "app/feedback.h"
#include "app/gesture.h"
#include "app/power.h"
#include "app/screenshot.h"
#include "drivers/pcf85063.h"
#include "ui/ui.h"
#include "audio/audio_bus.h"

void setup() {
    Serial.begin(115200);

    // --- 官方 BSP 初始化 ---
    I2C_Init();
    Backlight_Init();
    LCD_Init();
    Lvgl_Init();

    // --- 應用層 ---
    // 音訊匯流排要先於 feedbackInit()：音效任務直接寫這條 I2S。
    bey::audioBusBegin();

    bey::g_store.begin();
    Set_Backlight(bey::g_store.settings().brightness);
    bey::audioBusSetVolume(bey::g_store.settings().volume);
    bey::applySettingsToMatch();
    bey::feedbackInit();
    bey::buttonBegin();
    bey::rtcBegin();
    bey::power::begin();
    bey::ui::init();

    Serial.printf("[bey] ready, heap=%u psram=%u\n",
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getFreePsram()));
}

namespace {

// 邊緣旋轉調音量。一格 30 度、一圈 12 格，每格 5 ——
// 轉一圈調 60，手感接近實體旋鈕。
void applyRotation(int8_t steps) {
    if (steps == 0) {
        return;
    }
    bey::AppSettings& s = bey::g_store.mutableSettings();
    int v = static_cast<int>(s.volume) + steps * 5;
    v = (v < 0) ? 0 : ((v > 100) ? 100 : v);
    if (v == s.volume) {
        return;
    }
    s.volume = static_cast<uint8_t>(v);
    bey::audioBusSetVolume(s.volume);
    // 出一聲，否則只是無聲地改數字，使用者不知道自己轉到哪了。
    bey::feedbackPlay(bey::Sfx::Tick);
}

}  // namespace

void loop() {
#if BEY_DEBUG_SERIAL
    // 除錯用序列埠指令。板子不在手邊時，這是唯一能確認版面實際長相的方法：
    // 's' 把目前畫面傳回開發機，數字鍵切到指定畫面，'a'/'b' 模擬得分。
    // 正式韌體不編進去 —— 這些指令能繞過所有 UI 直接改比賽狀態。
    while (Serial.available() > 0) {
        switch (Serial.read()) {
            case 's':
                bey::screenshotCapture();
                break;
            case '1':
                bey::ui::showHome();
                break;
            case '2':
                bey::ui::showReady();
                break;
            case '3':
                // 開一場新的再進計分頁。只切畫面的話 g_match 還沒 start()，
                // applyResult() 會直接回 false，模擬得分等於沒按。
                bey::g_match.reset();
                bey::g_match.start();
                bey::markRoundStart();  // 正常流程由倒數觸發，這條捷徑要自己補
                bey::ui::showScore();
                break;
            case '4':
                bey::ui::showRound();
                break;
            case '5':
                bey::ui::showComplete();
                break;
            case '6':
                bey::ui::showSettings();
                break;
            case '7':
                bey::ui::showFormat();
                break;
            case '8':
                bey::ui::showHistory();
                break;
            case '9':
                bey::ui::showWinTypeOverlay(true);
                break;
            case 'c':
                bey::ui::showCountdown();
                break;
            case 'n':
                bey::ui::showNames();
                break;
            // 模擬得分，用來在沒有觸控的情況下把一場比賽跑完
            // （驗證計分流程、轉場、播報與歷史紀錄）。
            case 'a':
                bey::ui::applyResultAndAdvance(bey::ResultType::P1Spin);
                break;
            case 'b':
                bey::ui::applyResultAndAdvance(bey::ResultType::P2Burst);
                break;
            default:
                break;
        }
    }
#endif  // BEY_DEBUG_SERIAL

    // 實體按鍵（BOOT）＝這一局重來。
    //
    // 無效局在 Beyblade 很常見（沒發射好、同時出界、陀螺沒裝好），而那時候
    // 手上還拿著發射器，摸到板子側面的按鍵比在螢幕上找按鈕快得多。
    // 分數與局數都不變，只是重新倒數 —— Match::start() 只在比賽已結束時
    // 才 reset，所以比分不會被清掉。
    //
    // 只在計分頁有效：倒數中按它會打斷正在跑的倒數，其他畫面則沒有
    // 「這一局」可言。
    if (bey::buttonPressed() &&
        bey::ui::currentScreen() == bey::ui::ScreenId::Score) {
        bey::ui::showCountdown();
    }

    // 手勢。旋轉用累積格數，滑動用單一事件。
    //
    // 關掉時仍然要把事件讀空，否則重新打開的瞬間會一次湧出一堆過期手勢。
    {
        const bool on = bey::g_store.settings().enableGestures;
        const int8_t rot = bey::gestureTakeRotation();
        if (on) {
            applyRotation(rot);
        }

        bey::Gesture g;
        while (bey::gesturePoll(g)) {
            if (on && g == bey::Gesture::SwipeUp) {
                bey::ui::goBack();
            }
        }
    }

    Lvgl_Loop();
    vTaskDelay(pdMS_TO_TICKS(5));
}
