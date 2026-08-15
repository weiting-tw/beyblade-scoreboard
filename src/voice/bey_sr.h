// ---------------------------------------------------------------------------
// 改編自 Arduino ESP32 core 3.2.0 的 libraries/ESP_SR/src/esp32-hal-sr.{c,h}
//
// 為什麼要複製一份而不直接用 ESP_SR 函式庫：
//   原版把模型寫死成英文 ——
//     esp_srmodel_filter(models, ESP_WN_PREFIX, "hiesp")
//     esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH)
//   我們要「你好小智」中文喚醒詞與中文命令詞，這兩行必須改。
//
// 與原版的差異只有三處：
//   1. 喚醒詞 "hiesp"      -> "nihaoxiaozhi"
//   2. 命令詞 ESP_MN_ENGLISH -> ESP_MN_CHINESE
//   3. 公開符號加上 bey_ 前綴，避免與 Arduino ESP_SR 函式庫撞名
//
// 其餘邏輯（AFE 設定、feed/detect 任務、事件群組）保持原樣，
// 這是已驗證過的實作，不要順手「優化」。
// 上游更新時請重新套用這三處差異。
// ---------------------------------------------------------------------------
/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32S3 && (CONFIG_USE_WAKENET || CONFIG_USE_MULTINET)

#include "driver/i2s_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SR_CMD_STR_LEN_MAX     64
#define SR_CMD_PHONEME_LEN_MAX 64

typedef struct bey_sr_cmd_t {
  int command_id;
  char str[SR_CMD_STR_LEN_MAX];
  char phoneme[SR_CMD_PHONEME_LEN_MAX];
} bey_sr_cmd_t;

typedef enum {
  BEY_SR_EVENT_WAKEWORD,          //WakeWord Detected
  BEY_SR_EVENT_WAKEWORD_CHANNEL,  //WakeWord Channel Verified
  BEY_SR_EVENT_COMMAND,           //Command Detected
  BEY_SR_EVENT_TIMEOUT,           //Command Timeout
  BEY_SR_EVENT_MAX
} bey_sr_event_t;

typedef enum {
  BEY_SR_MODE_OFF,       //Detection Off
  BEY_SR_MODE_WAKEWORD,  //WakeWord Detection
  BEY_SR_MODE_COMMAND,   //Command Detection
  BEY_SR_MODE_MAX
} bey_sr_mode_t;

typedef enum {
  BEY_SR_CHANNELS_MONO,
  BEY_SR_CHANNELS_STEREO,
  BEY_SR_CHANNELS_MAX
} bey_sr_channels_t;

typedef void (*bey_sr_event_cb)(void *arg, bey_sr_event_t event, int command_id, int phrase_id);
typedef esp_err_t (*bey_sr_fill_cb)(void *arg, void *out, size_t len, size_t *bytes_read, uint32_t timeout_ms);

esp_err_t bey_sr_start(
  bey_sr_fill_cb fill_cb, void *fill_cb_arg, bey_sr_channels_t rx_chan, bey_sr_mode_t mode, const bey_sr_cmd_t *sr_commands, size_t cmd_number, bey_sr_event_cb cb, void *cb_arg
);
esp_err_t bey_sr_stop(void);
esp_err_t bey_sr_pause(void);
esp_err_t bey_sr_resume(void);
esp_err_t bey_sr_set_mode(bey_sr_mode_t mode);

// static const bey_sr_cmd_t sr_commands[] = {
//     {0, "Turn On the Light", "TkN nN jc LiT"},
//     {0, "Switch On the Light", "SWgp nN jc LiT"},
//     {1, "Switch Off the Light", "SWgp eF jc LiT"},
//     {1, "Turn Off the Light", "TkN eF jc LiT"},
//     {2, "Turn Red", "TkN RfD"},
//     {3, "Turn Green", "TkN GRmN"},
//     {4, "Turn Blue", "TkN BLo"},
//     {5, "Customize Color", "KcSTcMiZ KcLk"},
//     {6, "Sing a song", "Sgl c Sel"},
//     {7, "Play Music", "PLd MYoZgK"},
//     {8, "Next Song", "NfKST Sel"},
//     {9, "Pause Playing", "PeZ PLdgl"},
// };

#ifdef __cplusplus
}
#endif

#endif  // CONFIG_IDF_TARGET_ESP32S3
