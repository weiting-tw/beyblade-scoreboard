#include "pcf85063.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "../bsp/I2C_Driver.h"

namespace bey {
namespace {

constexpr uint8_t kAddr = 0x51;

// 時間暫存器是連續七個：秒／分／時／日／星期／月／年
constexpr uint8_t kRegSeconds = 0x04;

// 秒暫存器的 bit7 是 OS（Oscillator Stop）。為 1 代表振盪器曾經停止，
// 也就是這顆掉過電且沒有備援 —— 裡面的時間不能信。
constexpr uint8_t kOsFlag = 0x80;

// 日期與秒數的互轉。不用 mktime：它會套上執行環境的時區，而這裡從頭到尾
// 都在同一個本地時間座標系裡（見標頭檔說明），多套一次就整個偏掉。
// 民曆日期轉 Unix 天數
// （Howard Hinnant 的 days_from_civil）。
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= (m <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? -3u : 9u)) + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

int64_t utcToEpoch(const std::tm& tm) {
    const int64_t days = daysFromCivil(tm.tm_year + 1900,
                                       static_cast<unsigned>(tm.tm_mon + 1),
                                       static_cast<unsigned>(tm.tm_mday));
    return days * 86400 + tm.tm_hour * 3600LL + tm.tm_min * 60LL + tm.tm_sec;
}

uint8_t fromBcd(uint8_t v) { return static_cast<uint8_t>((v >> 4) * 10 + (v & 0x0F)); }
uint8_t toBcd(uint8_t v) { return static_cast<uint8_t>(((v / 10) << 4) | (v % 10)); }

bool readRaw(uint8_t* buf7) {
    // I2C_Read 的回傳語意是反的：失敗回 -1（true），成功回 0（false）。
    return !I2C_Read(kAddr, kRegSeconds, buf7, 7);
}

}  // namespace

uint32_t rtcNowEpoch() {
    uint8_t b[7] = {0};
    if (!readRaw(b)) {
        return 0;
    }
    if (b[0] & kOsFlag) {
        return 0;  // 振盪器停過，時間不可信
    }

    std::tm tm{};
    tm.tm_sec = fromBcd(b[0] & 0x7F);
    tm.tm_min = fromBcd(b[1] & 0x7F);
    tm.tm_hour = fromBcd(b[2] & 0x3F);
    tm.tm_mday = fromBcd(b[3] & 0x3F);
    // b[4] 是星期，epoch 換算用不到
    tm.tm_mon = fromBcd(b[5] & 0x1F) - 1;
    // 年暫存器只有兩位數，代表 2000..2099
    tm.tm_year = fromBcd(b[6]) + 100;

    const int64_t t = utcToEpoch(tm);
    return (t < 0) ? 0 : static_cast<uint32_t>(t);
}

bool rtcSetEpoch(uint32_t epoch) {
    const time_t t = static_cast<time_t>(epoch);
    std::tm tm{};
    gmtime_r(&t, &tm);

    const uint8_t b[7] = {
        toBcd(static_cast<uint8_t>(tm.tm_sec)),  // 寫 0 進 bit7 順帶清掉 OS 旗標
        toBcd(static_cast<uint8_t>(tm.tm_min)),
        toBcd(static_cast<uint8_t>(tm.tm_hour)),
        toBcd(static_cast<uint8_t>(tm.tm_mday)),
        static_cast<uint8_t>(tm.tm_wday),
        toBcd(static_cast<uint8_t>(tm.tm_mon + 1)),
        toBcd(static_cast<uint8_t>(tm.tm_year - 100)),
    };
    return !I2C_Write(kAddr, kRegSeconds, b, 7);
}

void rtcBegin() {
    uint8_t b[7] = {0};
    if (!readRaw(b)) {
        Serial.println("[rtc] 沒有回應，歷史紀錄不會有時間");
        return;
    }

    if ((b[0] & kOsFlag) == 0) {
        Serial.printf("[rtc] 就緒，epoch=%lu\n",
                      static_cast<unsigned long>(rtcNowEpoch()));
        return;
    }

    // 振盪器停過。用韌體建置時間當起點：不準，但歷史紀錄至少會落在合理的
    // 日期附近，而不是 1970 或一串亂數。__DATE__ 形如 "Aug 15 2026"。
    //
    // __DATE__ / __TIME__ 是建置機器的本地時間，這裡就原樣存進去不做換算 ——
    // 整條鏈路都用本地時間，換算反而會讓顯示出來的時刻偏掉一個時區。
    static const char kMonths[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon[4] = {0};
    int day = 0, year = 0, hh = 0, mm = 0, ss = 0;
    std::sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    std::sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss);

    const char* p = std::strstr(kMonths, mon);
    const int monIdx = (p == nullptr) ? 0 : static_cast<int>((p - kMonths) / 3);

    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = monIdx;
    tm.tm_mday = day;
    tm.tm_hour = hh;
    tm.tm_min = mm;
    tm.tm_sec = ss;

    const int64_t t = utcToEpoch(tm);
    if (t > 0 && rtcSetEpoch(static_cast<uint32_t>(t))) {
        Serial.printf("[rtc] 振盪器停過，已用建置時間初始化：%s %s\n", __DATE__,
                      __TIME__);
    } else {
        Serial.println("[rtc] 初始化失敗，歷史紀錄不會有時間");
    }
}

}  // namespace bey
