// 快速設定面板：從螢幕頂部往下滑叫出來，調亮度與音量。
//
// 刻意不重用 screen_overlay 的那套：那是為「勝利類型選單／確認對話框」設計的
// 單例，openOverlay() 一開頭就 close() 掉前一個 —— 兩者互斥是對的，因為同時
// 出現兩個對話框沒有意義。但快設面板不屬於那個語意，硬塞進去會變成
// 「拉下面板就把確認對話框吃掉」。它自己在 lv_layer_top 上開一個獨立物件。
#pragma once

#include <lvgl.h>

namespace bey {
namespace ui {

// 開啟／關閉。已經開著時再開是 no-op。
void quickPanelOpen();
void quickPanelClose();

bool quickPanelIsOpen();

}  // namespace ui
}  // namespace bey
