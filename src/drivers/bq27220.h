// BQ27220 電量計（I2C 0x55）唯讀介面。
//
// 只讀四個暫存器，不下任何指令、不寫任何設定 —— 這顆的 chem ID 與 golden
// image 是否燒過並不確定（實測 RemainingCapacity 與 FullChargeCapacity 都是
// 500 mAh 整數且相等，很像出廠預設而非實測容量），亂寫可能讓電量估算更糟。
//
// 0x2C 是 StateOfCharge：不同版本的 TI 文件把這個位址標成 StateOfCharge 或
// ChargeVoltage，實測滿電時讀到 100（而非 4000 上下的 mV），故採前者。
#pragma once

#include <cstdint>

namespace bey {

struct BatteryState {
    bool present = false;      // 這顆有沒有回應
    uint8_t percent = 0;       // 0..100
    uint16_t milliVolts = 0;   // 電池電壓
    int16_t milliAmps = 0;     // 正 = 充電、負 = 放電
    bool external = false;     // 外部供電中（含充飽後仍插著）
};

// 探測 0x55 是否回應。回傳 false 代表這顆沒焊或電池沒接，
// 呼叫端應該整個放棄電量顯示，而不是顯示 0%。
bool bq27220Present();

// 讀一次。失敗時不動 out。
bool bq27220Read(BatteryState& out);

}  // namespace bey
