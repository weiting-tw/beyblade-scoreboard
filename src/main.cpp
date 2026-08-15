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
#include "ui/ui.h"

void setup() {
    Serial.begin(115200);

    // --- 官方 BSP 初始化 ---
    I2C_Init();
    Backlight_Init();
    LCD_Init();
    Lvgl_Init();

    // --- 應用層 ---
    bey::g_store.begin();
    Set_Backlight(bey::g_store.settings().brightness);
    bey::applySettingsToMatch();
    bey::feedbackInit();
    bey::ui::init();

    Serial.printf("[bey] ready, heap=%u psram=%u\n",
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getFreePsram()));
}

void loop() {
    Lvgl_Loop();
    vTaskDelay(pdMS_TO_TICKS(5));
}
