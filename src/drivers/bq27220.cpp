#include "bq27220.h"

#include "../bsp/I2C_Driver.h"

namespace bey {
namespace {

constexpr uint8_t kAddr = 0x55;

constexpr uint8_t kRegBatteryStatus = 0x0A;
constexpr uint8_t kRegVoltage = 0x08;
constexpr uint8_t kRegCurrent = 0x0C;
constexpr uint8_t kRegStateOfCharge = 0x2C;

// BatteryStatus bit0 = DSG：1 代表正在放電，0 代表外部供電（含充飽）。
constexpr uint16_t kStatusDischarging = 0x0001;

bool readReg16(uint8_t reg, uint16_t& out) {
    uint8_t buf[2] = {0, 0};
    // I2C_Read 的回傳語意是反的：失敗回 -1（true），成功回 0（false）。
    // 這是 BSP 既有的寫法，不在這裡改，但每個呼叫點都得記得反過來判斷。
    if (I2C_Read(kAddr, reg, buf, 2)) {
        return false;
    }
    out = static_cast<uint16_t>(buf[0] | (buf[1] << 8));
    return true;
}

}  // namespace

bool bq27220Present() {
    uint16_t v = 0;
    // 拿電壓當探針：讀得到而且是合理的鋰電池電壓才算數。
    // 只看 I2C ACK 不夠 —— 位址撞到別的裝置也會 ACK。
    if (!readReg16(kRegVoltage, v)) {
        return false;
    }
    return v >= 2500 && v <= 4500;
}

bool bq27220Read(BatteryState& out) {
    uint16_t volts = 0;
    uint16_t soc = 0;
    if (!readReg16(kRegVoltage, volts) || !readReg16(kRegStateOfCharge, soc)) {
        return false;
    }

    out.present = true;
    out.milliVolts = volts;
    out.percent = soc > 100 ? 100 : static_cast<uint8_t>(soc);

    // 電流與狀態讀不到不算失敗 —— 電量百分比才是顯示必需的。
    uint16_t cur = 0;
    if (readReg16(kRegCurrent, cur)) {
        out.milliAmps = static_cast<int16_t>(cur);
    }
    uint16_t status = 0;
    if (readReg16(kRegBatteryStatus, status)) {
        out.external = (status & kStatusDischarging) == 0;
    }

    return true;
}

}  // namespace bey
