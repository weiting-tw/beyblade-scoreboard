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
#include "app/feedback.h"
#include "app/power.h"
#include "app/screenshot.h"
#include "drivers/pcf85063.h"
#include "ui/ui.h"
#include "audio/audio_bus.h"
#include "voice/voice.h"

void setup() {
    Serial.begin(115200);

    // --- 官方 BSP 初始化 ---
    I2C_Init();
    Backlight_Init();
    LCD_Init();
    Lvgl_Init();

    // --- 應用層 ---
    // 音訊匯流排要先於語音與音效：兩者共用同一條 I2S。
    bey::audioBusBegin();

    bey::g_store.begin();
    Set_Backlight(bey::g_store.settings().brightness);
    bey::audioBusSetVolume(bey::g_store.settings().volume);
    bey::applySettingsToMatch();
    bey::feedbackInit();
    bey::rtcBegin();
    bey::power::begin();
    bey::ui::init();

    // 語音是加分項，不是必要條件。模型缺失或麥克風壞掉時只是沒有語音，
    // 觸控計分完全不受影響，所以這裡不檢查回傳值中止開機。
    bey::voiceBegin();

    Serial.printf("[bey] ready, heap=%u psram=%u voice=%s\n",
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getFreePsram()),
                  bey::voiceStatusText());
}

void loop() {
    // 語音辨識在自己的任務裡跑；命令經佇列送到這裡，
    // 才能在 LVGL 執行緒上安全地切換畫面。
    bey::VoiceCmd cmd;
    while (bey::voicePoll(cmd)) {
        // 播報期間收到的辨識結果一律丟棄。AEC 是關的，功放正放著英文，
        // 麥克風聽得見自己 —— 這裡收到的東西很可能是板子自己講的。
        // 仍然要把佇列讀空，否則播報結束後會一次湧出一串過期命令。
        if (bey::feedbackIsSpeaking()) {
            continue;
        }
        bey::ui::handleVoiceCommand(cmd);
    }

    // 除錯用序列埠指令。板子不在手邊時，這是唯一能確認版面實際長相的方法：
    // 's' 把目前畫面傳回開發機，數字鍵切到指定畫面（切畫面本來要用觸控）。
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
            default:
                break;
        }
    }

    Lvgl_Loop();
    vTaskDelay(pdMS_TO_TICKS(5));
}
