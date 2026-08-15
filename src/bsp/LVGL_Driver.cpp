/*****************************************************************************
  | File        :   LVGL_Driver.c
  
  | help        : 
    The provided LVGL library file must be installed first
******************************************************************************/
#include "LVGL_Driver.h"

#include "../app/gesture.h"
#include "../app/screenshot.h"

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[ LVGL_BUF_LEN ];
static lv_color_t buf2[ LVGL_BUF_LEN];
// static lv_color_t* buf1 = (lv_color_t*) heap_caps_malloc(LVGL_BUF_LEN , MALLOC_CAP_SPIRAM);
// static lv_color_t* buf2 = (lv_color_t*) heap_caps_malloc(LVGL_BUF_LEN , MALLOC_CAP_SPIRAM);
    
/*  Display flushing 
    Displays LVGL content on the LCD
    This function implements associating LVGL data to the LCD screen
*/
/* 這裡刻意**不**呼叫 lv_disp_flush_ready()。
 *
 * LCD_addWindow() 底下是 esp_lcd_panel_draw_bitmap()，非同步：把 SPI 傳輸排進
 * 佇列就返回。原始碼在此立刻宣告 flush 完成，LVGL 於是把還在 DMA 中的緩衝區
 * 拿去畫下一幀，而 LCD_addWindow 又會對緩衝區原地做位元組交換 ——
 * 兩者疊加就是按按鈕時的破圖。
 *
 * 正確做法是等 on_color_trans_done 真的觸發，見下方 lvgl_flush_done()。
 */
void Lvgl_Display_LCD( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p )
{
  /* 擷取要在 LCD_addWindow 之前：它會對緩衝區原地做位元組交換，
   * 之後再讀就變成 big-endian 了。非擷取狀態下這只是一個 bool 判斷。*/
  bey::screenshotOnFlush(area, color_p);
  LCD_addWindow(area->x1, area->y1, area->x2, area->y2, ( uint16_t *)&color_p->full);
}

/* 由 SPI 傳輸完成中斷呼叫。只能做極輕量的事 —— lv_disp_flush_ready 僅是清旗標。*/
static void lvgl_flush_done(void *ctx)
{
  lv_disp_flush_ready((lv_disp_drv_t *)ctx);
}
/*Read the touchpad*/
void Lvgl_Touchpad_Read( lv_indev_drv_t * indev_drv, lv_indev_data_t * data )
{
  Touch_Read_Data();
#if BEY_DEBUG_SERIAL
  /* 只在按下那一瞬間印，不是整個按住期間 —— 觸控每 30ms 輪詢一次，
   * 按住一秒就是 33 行，反而看不出點在哪。 */
  static bool wasDown = false;
#endif
  const bool down = (touch_data.points != 0x00);
  bey::touchFeed(touch_data.x, touch_data.y, down, (uint8_t)touch_data.gesture);

  if (down) {
    data->point.x = touch_data.x;
    data->point.y = touch_data.y;
    data->state = LV_INDEV_STATE_PR;
#if BEY_DEBUG_SERIAL
    /* 連續記錄軌跡：畫圈這種手勢 CST816 判不出來，要自己從座標算角度，
     * 所以需要整條軌跡而不只是起點。移動超過 3px 才印，避免手指靜止時洗版。 */
    static uint16_t lastX = 0, lastY = 0;
    const int dx = (int)touch_data.x - (int)lastX;
    const int dy = (int)touch_data.y - (int)lastY;
    if (!wasDown) {
      printf("[down] x=%3u y=%3u\r\n", touch_data.x, touch_data.y);
      lastX = touch_data.x; lastY = touch_data.y;
    } else if (dx * dx + dy * dy > 9) {
      printf("[move] x=%3u y=%3u\r\n", touch_data.x, touch_data.y);
      lastX = touch_data.x; lastY = touch_data.y;
    }
    wasDown = true;
#endif
  } else {
    data->state = LV_INDEV_STATE_REL;
#if BEY_DEBUG_SERIAL
    if (wasDown) {
      printf("[up]\r\n");
    }
    wasDown = false;
#endif
  }
  if (touch_data.gesture != NONE ) {
#if BEY_DEBUG_SERIAL
    /* CST816 自己就會判手勢，不必從座標軌跡重算。先量它實際靈不靈敏、
     * 會不會把單純的點擊誤判成滑動，再決定要不要接進應用層。 */
    static const char *kNames[] = {
      "NONE", "UP", "DOWN", "LEFT", "RIGHT", "CLICK",
      "?6", "?7", "?8", "?9", "?A", "DOUBLE", "LONG"
    };
    const uint8_t gi = (uint8_t)touch_data.gesture;
    printf("[gesture] %s (0x%02X) x=%3u y=%3u\r\n",
           gi < (sizeof(kNames) / sizeof(kNames[0])) ? kNames[gi] : "?",
           gi, touch_data.x, touch_data.y);
#endif
  }
  touch_data.x = 0;
  touch_data.y = 0;
  touch_data.points = 0;
  touch_data.gesture = NONE;
}

void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}
void example_increase_lvgl_Loop_tick(void *arg)
{
  lv_timer_handler(); /* let the GUI do its work */
}
void Lvgl_Init(void)
{
  lv_init();
  lv_disp_draw_buf_init( &draw_buf, buf1, buf2, LVGL_BUF_LEN);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = LCD_WIDTH;
  disp_drv.ver_res = LCD_HEIGHT;
  disp_drv.flush_cb = Lvgl_Display_LCD;
  /* 原始碼寫 full_refresh = 1，但 LVGL 8.4 會在 lv_disp_drv_register() 裡把它
   * 改回 0 並印出警告 —— full_refresh 要求繪圖緩衝區至少和螢幕一樣大，而
   * LVGL_BUF_LEN 只有螢幕的 1/10。實際跑的一直是局部刷新。
   * 這裡直接寫 0，讓程式碼與實際行為一致，不要留一個會被靜默推翻的設定。 */
  disp_drv.full_refresh = 0;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  /* disp_drv 是 static，位址在整個程式生命週期內都有效，可以安全地交給中斷回呼。*/
  LCD_setFlushDoneCallback(lvgl_flush_done, &disp_drv);

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init( &indev_drv );
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = Lvgl_Touchpad_Read;
  lv_indev_drv_register( &indev_drv );

  /* 應用層自行建立畫面，此處不放官方範例的 Hello label */

  const esp_timer_create_args_t lvgl_tick_timer_args = {
    .callback = &example_increase_lvgl_tick,
    .name = "lvgl_tick"
  };
  esp_timer_handle_t lvgl_tick_timer = NULL;
  esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
  esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);

}
void Lvgl_Loop(void)
{
  lv_timer_handler(); /* let the GUI do its work */
}
