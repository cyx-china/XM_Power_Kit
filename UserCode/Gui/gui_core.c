/**
  ******************************************************************************
  * @file           : gui_core.c
  * @brief          : 自研 GUI 框架核心：屏栈管理、按键分发、基础绘制助手
  * @date           : 2026/11/13
  ******************************************************************************
  */

#include "gui.h"

/*=============================== 屏栈 ===============================*/
#define GUI_STACK_DEPTH     4

static const GuiScreen *s_current = NULL;
static const GuiScreen *s_stack[GUI_STACK_DEPTH];
static uint8_t s_depth = 0;

static void gui_activate(const GuiScreen *screen) {
    if (screen == NULL) {
        return;
    }
    if (s_current == screen) {              // 同一界面：只重绘
        if (screen->on_draw != NULL) {
            screen->on_draw();
        }
        return;
    }
    if (s_current != NULL && s_current->on_exit != NULL) {
        s_current->on_exit();
    }
    s_current = screen;
    if (screen->on_enter != NULL) {
        screen->on_enter();
    } else if (screen->on_draw != NULL) {
        screen->on_draw();
    }
}

void Gui_Init(void) {
    s_depth = 0;
    s_current = NULL;
    Gui_Clear(GUI_COL_BG);
    gui_activate(&gui_screen_home);
}

void Gui_Navigate(const GuiScreen *screen) {
    if (screen == NULL) {
        return;
    }
    if (s_depth < GUI_STACK_DEPTH) {
        s_stack[s_depth++] = s_current;
    }
    gui_activate(screen);
}

void Gui_Back(void) {
    while (s_depth > 0) {
        const GuiScreen *prev = s_stack[--s_depth];
        if (prev != NULL) {
            gui_activate(prev);
            return;
        }
    }
    gui_activate(&gui_screen_home);          // 栈空时兜底回首页
}

void Gui_Dispatch(GuiKey_t key) {
    if (key == GUI_KEY_NONE || s_current == NULL) {
        return;
    }
    if (s_current->on_key != NULL) {
        s_current->on_key(key);
    }
}

void Gui_Tick(uint32_t now_ms) {
    if (s_current != NULL && s_current->on_tick != NULL) {
        s_current->on_tick(now_ms);
    }
}

void Gui_Redraw(void) {
    if (s_current != NULL && s_current->on_draw != NULL) {
        s_current->on_draw();
    }
}

/*=============================== 绘制助手 ===============================*/
void Gui_Clear(uint16_t color) {
    (void) lcd_draw_clear_screen(color);
}

void Gui_Fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    (void) lcd_draw_rect(x1, y1, x2, y2, color, 1);
}

void Gui_RoundFill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r, uint16_t color) {
    (void) lcd_draw_round_rect(x1, y1, x2, y2, r, color, 1);
}

void Gui_RoundFrame(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r,
                    uint16_t color, uint8_t thickness) {
    uint8_t i;
    for (i = 0; i < thickness; i++) {
        uint16_t rr = (r > i) ? (uint16_t) (r - i) : 0U;
        if ((x1 + i) >= (x2 - i) || (y1 + i) >= (y2 - i)) {
            break;
        }
        (void) lcd_draw_round_rect((uint16_t) (x1 + i), (uint16_t) (y1 + i),
                                   (uint16_t) (x2 - i), (uint16_t) (y2 - i), rr, color, 0);
    }
}

void Gui_Line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    (void) lcd_draw_line(x1, y1, x2, y2, color);
}

/* 文本宽度：与 graphics.c 的字符步进规则保持一致
   （普通字符 = font_width，'.' 更窄，其余字符一律按 font_width 估算） */
uint16_t Gui_TextWidth(const char *text, const FontInfo *font) {
    uint16_t width = 0;
    const uint8_t *p = (const uint8_t *) text;

    if (text == NULL || font == NULL) {
        return 0;
    }
    while (*p != 0) {
        if (*p == '.') {
            width = (uint16_t) (width + (font->font_width * 6U) / 10U + 1U);
            p++;
        } else if (*p < 0x80U) {
            width = (uint16_t) (width + font->font_width);
            p++;
        } else {
            width = (uint16_t) (width + font->font_width);
            if ((*p & 0xE0U) == 0xC0U) {
                p += 2;
            } else if ((*p & 0xF0U) == 0xE0U) {
                p += 3;
            } else if ((*p & 0xF8U) == 0xF0U) {
                p += 4;
            } else {
                p += 1;
            }
        }
    }
    return width;
}

void Gui_Text(uint16_t x, uint16_t y, const char *text, const FontInfo *font,
              uint16_t color, uint16_t bg) {
    if (text == NULL || *text == 0) {
        return;
    }
    (void) lcd_draw_string(x, y, text, font, color, bg, 0);
}

void Gui_TextCenter(uint16_t cx, uint16_t y, const char *text, const FontInfo *font,
                    uint16_t color, uint16_t bg) {
    uint16_t w = Gui_TextWidth(text, font);
    uint16_t x = (cx > (w / 2U)) ? (uint16_t) (cx - w / 2U) : 0U;
    Gui_Text(x, y, text, font, color, bg);
}

void Gui_TextRight(uint16_t x_right, uint16_t y, const char *text, const FontInfo *font,
                   uint16_t color, uint16_t bg) {
    uint16_t w = Gui_TextWidth(text, font);
    uint16_t x = (x_right > w) ? (uint16_t) (x_right - w) : 0U;
    Gui_Text(x, y, text, font, color, bg);
}

/* 进度条：轨道 + 填充 + 圆头。percent 0~100 */
void Gui_Slider(uint16_t x1, uint16_t y, uint16_t w, uint16_t h, uint8_t percent,
                uint16_t track_color, uint16_t fill_color) {
    uint16_t x2 = (uint16_t) (x1 + w - 1U);
    uint16_t fill_w;

    if (percent > 100U) {
        percent = 100U;
    }
    Gui_RoundFill(x1, y, x2, (uint16_t) (y + h - 1U), (uint16_t) (h / 2U), track_color);

    fill_w = (uint16_t) ((uint32_t) w * percent / 100U);
    if (fill_w < h) {
        fill_w = h;                                   // 太短时至少画出一个圆头
    }
    if (fill_w > 0U) {
        Gui_RoundFill(x1, y, (uint16_t) (x1 + fill_w - 1U), (uint16_t) (y + h - 1U),
                      (uint16_t) (h / 2U), fill_color);
    }
}

/* 开关：胶囊底 + 圆形滑块 */
void Gui_Switch(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) {
    uint16_t knob_r = (uint16_t) (h / 2U - 2U);
    uint16_t knob_cx;

    Gui_RoundFill(x, y, (uint16_t) (x + w - 1U), (uint16_t) (y + h - 1U),
                  (uint16_t) (h / 2U), on ? GUI_COL_ACCENT : GUI_RGB( 42,  58,  94));
    knob_cx = on ? (uint16_t) (x + w - h / 2U) : (uint16_t) (x + h / 2U);
    (void) lcd_draw_circle(knob_cx, (uint16_t) (y + h / 2U), knob_r, GUI_RGB(255, 255, 255), 1);
}
