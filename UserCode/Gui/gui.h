/**
  ******************************************************************************
  * @file           : gui.h
  * @brief          : 自研轻量 GUI 框架（替代 LVGL）：屏栈、按键分发、绘制助手、配色定义
  * @date           : 2026/11/13
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * 本框架只使用项目自研的 lcd_draw_* 绘图接口（命令队列 + 单刷屏线程 + DMA），
  * 不依赖任何第三方 GUI 库；所有绘制都是异步入队，按入队顺序执行。
  ******************************************************************************
  */

#ifndef XM_POWER_KIT_GUI_H
#define XM_POWER_KIT_GUI_H

#include <stdbool.h>
#include <stdint.h>

#include "font.h"
#include "images.h"
#include "lcd_draw_api.h"

/* 配色：宏按 8-8-8 定义，编译期转成 RGB565（屏幕是 16 位色） */
#define GUI_RGB(r, g, b) ((uint16_t) ((((uint16_t) (r) & 0xF8U) << 8) | \
                                      (((uint16_t) (g) & 0xFCU) << 3) | \
                                      (((uint16_t) (b) & 0xF8U) >> 3)))

/*======================== 配色（沿用原 LVGL 界面） ========================*/
#define GUI_COL_BG           GUI_RGB( 26,  26,  46)      // 页面底色
#define GUI_COL_CARD         GUI_RGB( 22,  33,  62)      // 卡片/行底色
#define GUI_COL_ACCENT       GUI_RGB( 33, 149, 246)      // 主题蓝
#define GUI_COL_ACCENT_DARK  GUI_RGB( 15,  52,  96)      // 深蓝（进度条轨道）
#define GUI_COL_TITLE        GUI_RGB( 14, 165, 233)      // 标题蓝
#define GUI_COL_TEXT         GUI_RGB(255, 255, 255)      // 主文字
#define GUI_COL_TEXT_SUB     GUI_RGB(170, 170, 170)      // 次级文字
#define GUI_COL_TEXT_DIM     GUI_RGB(108, 122, 150)      // 分组标题
#define GUI_COL_OK           GUI_RGB( 34, 197,  94)      // 绿色（开关打开）

/*=============================== 界面字体 ===============================*/
// 三个字库都由 图片与字体/小工具/字体取模/gen_ui_font.py 生成，放在外部 flash 的 0:/font/
#include "ui_fonts.h"

/*=============================== 按键事件 ===============================*/
typedef enum {
    GUI_KEY_NONE = 0,
    GUI_KEY_UP,         // 物理上键 / 编码器左转（导航态 = 上一个）
    GUI_KEY_DOWN,       // 物理下键 / 编码器右转（导航态 = 下一个）
    GUI_KEY_LEFT,       // 编码器左转（编辑态 = 减）
    GUI_KEY_RIGHT,      // 编码器右转（编辑态 = 加）
    GUI_KEY_ENTER,      // SET / 编码器单击
    GUI_KEY_BACK        // MODE
} GuiKey_t;

// 一个界面（页面）的描述：进入/退出/按键/整屏重绘
typedef struct GuiScreen {
    void (*on_enter)(void);
    void (*on_exit)(void);
    void (*on_key)(GuiKey_t key);
    void (*on_draw)(void);
} GuiScreen;

/*=============================== 框架 API ===============================*/
void Gui_Init(void);                            // 框架初始化，进入首页
void Gui_Dispatch(GuiKey_t key);                // 把按键事件交给当前界面
void Gui_Navigate(const GuiScreen *screen);     // 进入新界面（可返回）
void Gui_Back(void);                            // 返回上一界面（栈空则回首页）
void Gui_Redraw(void);                          // 请求当前界面整屏重绘

/*=============================== 绘制助手 ===============================*/
void Gui_Clear(uint16_t color);
void Gui_Fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void Gui_RoundFill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r, uint16_t color);
void Gui_RoundFrame(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r,
                    uint16_t color, uint8_t thickness);
void Gui_Line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

uint16_t Gui_TextWidth(const char *text, const FontInfo *font);
void Gui_Text(uint16_t x, uint16_t y, const char *text, const FontInfo *font,
              uint16_t color, uint16_t bg);
void Gui_TextCenter(uint16_t cx, uint16_t y, const char *text, const FontInfo *font,
                    uint16_t color, uint16_t bg);
void Gui_TextRight(uint16_t x_right, uint16_t y, const char *text, const FontInfo *font,
                   uint16_t color, uint16_t bg);

void Gui_Slider(uint16_t x1, uint16_t y, uint16_t w, uint16_t h, uint8_t percent,
                uint16_t track_color, uint16_t fill_color);
void Gui_Switch(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on);

extern const GuiScreen gui_screen_home;
extern const GuiScreen gui_screen_settings;
extern const GuiScreen gui_screen_about;

#endif // XM_POWER_KIT_GUI_H
