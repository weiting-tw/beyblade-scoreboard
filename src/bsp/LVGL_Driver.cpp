/*****************************************************************************
  | File        :   LVGL_Driver.c
  
  | help        : 
    The provided LVGL library file must be installed first
******************************************************************************/
#include "LVGL_Driver.h"

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
  if (touch_data.points != 0x00) {
    data->point.x = touch_data.x;
    data->point.y = touch_data.y;
    data->state = LV_INDEV_STATE_PR;
    // printf("LVGL : X=%u Y=%u points=%d\r\n",  touch_data.x , touch_data.y,touch_data.points);
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
  if (touch_data.gesture != NONE ) {    
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
