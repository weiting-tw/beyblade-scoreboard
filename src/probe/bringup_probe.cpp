// 實測數據擷取韌體 —— 一次燒錄同時拿到兩份目前只能靠猜的數據。
//
//   pio run -e bringup_probe -t upload
//   .venv/bin/python tools/serial_watch.py -s 20
//
// 一、BQ27220（I2C 0x55）暫存器 dump
//     0x2C 在不同版本的 TI 文件被標成 StateOfCharge 或 ChargeVoltage，
//     照著猜寫驅動有一半機率是錯的。這裡把可疑的欄位全印出來，用實際數值判斷：
//     電量百分比會落在 0..100，充電電壓會是 4000 上下的 mV。
//     滿電與低電各跑一次，比對哪個欄位有跟著動。
//
// 二、字型實際字寬
//     版面上所有 maxR 計算都依賴 label 寬度，而中文字在各字級下的真實
//     advance width 從來沒量過，全是估的。這裡直接問 LVGL 要答案。
//
// 只讀不寫：不對 BQ27220 下任何指令，不改任何設定。
#include <Arduino.h>
#include <lvgl.h>

#include "../bsp/I2C_Driver.h"
#include "../ui/fonts.h"

namespace {

constexpr uint8_t kBqAddr = 0x55;

struct Reg {
    uint8_t addr;
    const char* name;
};

// BQ27220 是 16-bit 暫存器、little-endian。名稱後面標 ? 的就是要靠實測判斷的。
const Reg kRegs[] = {
    {0x06, "Temperature      (0.1 K)"},
    {0x08, "Voltage          (mV)"},
    {0x0A, "BatteryStatus    (bitfield)"},
    {0x0C, "Current          (mA, 有號)"},
    {0x10, "RemainingCap     (mAh)"},
    {0x12, "FullChargeCap    (mAh)"},
    {0x2C, "StateOfCharge?   (% 或 mV)"},
    {0x2E, "StateOfHealth?   (%)"},
};

// 量測對象。涵蓋計分頁上每一個目前靠估算定位的元素。
struct Sample {
    const char* text;
    const char* usage;
};

const Sample kSamples[] = {
    // 計分頁
    {"第 1 局    4 分制", "計分頁狀態列"},
    {"第 10 局    4 分制", "計分頁狀態列（兩位數）"},
    {"選手一", "預設玩家名"},
    {"BBBBBBBB", "玩家名最寬情境（ASCII）"},
    {"樂天堂哥布林", "玩家名最寬情境（中文）"},
    {"12", "比分兩位數"},
    // 首頁 —— 這兩個是沒有固定寬度的 label，寬度完全由字串決定
    {"BEYBLADE X", "首頁主標題"},
    {"BATTLE SCORE", "首頁副標題"},
    // 勝利類型 overlay
    {"選手一 勝利方式", "overlay 標題"},
    {"BBBBBBBB 勝利方式", "overlay 標題最寬情境"},
    {"XTREME  +3", "勝利類型按鈕"},
    // 局結果／準備／賽制
    {"準備", "準備頁標題"},
    {"勝利分數", "賽制頁說明"},
    {"第 1 局結束", "局結果狀態列"},
    {"下一局", "窄按鈕文字"},
};

struct FontEntry {
    const lv_font_t* font;
    const char* name;
};

bool readReg16(uint8_t reg, uint16_t& out) {
    uint8_t buf[2] = {0, 0};
    // 注意 I2C_Read 的回傳語意是反的：失敗回 -1（true），成功回 0（false）。
    if (I2C_Read(kBqAddr, reg, buf, 2)) {
        return false;
    }
    out = static_cast<uint16_t>(buf[0] | (buf[1] << 8));
    return true;
}

void scanI2C() {
    Serial.println("--- I2C 掃描 ---");
    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  0x%02X 有回應\n", addr);
            found++;
        }
    }
    Serial.printf("  共 %d 個裝置\n\n", found);
}

void dumpBattery() {
    Serial.println("--- BQ27220 @ 0x55 ---");

    Wire.beginTransmission(kBqAddr);
    if (Wire.endTransmission() != 0) {
        Serial.println("  沒有回應 —— 這顆可能沒焊，或電池沒接上。");
        Serial.println("  （後面的字型量測仍會照跑）\n");
        return;
    }

    for (const Reg& r : kRegs) {
        uint16_t v = 0;
        if (!readReg16(r.addr, v)) {
            Serial.printf("  0x%02X %-28s 讀取失敗\n", r.addr, r.name);
            continue;
        }
        Serial.printf("  0x%02X %-28s = %5u  (0x%04X, 有號 %6d)\n", r.addr,
                      r.name, v, v, static_cast<int16_t>(v));
    }
    Serial.println();
    Serial.println("  判讀方式：0x2C 若落在 0..100 就是電量百分比；");
    Serial.println("  若是 3000..4500 那是 mV，代表 SOC 在別的位址。");
    Serial.println("  充飽與快沒電時各跑一次，比對哪個欄位跟著動。\n");
}

void measureFonts() {
    Serial.println("--- 字型實際尺寸（lv_txt_get_size）---");

    const FontEntry fonts[] = {
        {&font_tc_16, "font_tc_16"},
        {&font_tc_22, "font_tc_22"},
        {&font_tc_30, "font_tc_30"},
        {&lv_font_montserrat_36, "montserrat_36"},
    };

    for (const FontEntry& f : fonts) {
        Serial.printf("\n  [%s]  line_height=%d\n", f.name,
                      static_cast<int>(f.font->line_height));
        for (const Sample& s : kSamples) {
            lv_point_t sz;
            lv_txt_get_size(&sz, s.text, f.font, 0, 0, LV_COORD_MAX,
                            LV_TEXT_FLAG_NONE);
            Serial.printf("    %4d x %3d   %-28s %s\n", sz.x, sz.y, s.text,
                          s.usage);
        }
    }
    Serial.println();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(3000);  // 等 USB CDC 連上，否則前幾行會被吞掉

    Serial.println("\n\n===== bringup probe =====\n");

    I2C_Init();
    scanI2C();
    dumpBattery();

    // 只做字型度量，不開顯示驅動 —— lv_txt_get_size 是純計算。
    lv_init();
    measureFonts();

    Serial.println("===== 結束。把上面整段貼回來即可。 =====");
}

void loop() {
    delay(1000);
}
