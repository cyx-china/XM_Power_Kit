#ifndef XM_POWER_KIT_GRAPHICS_H
#define XM_POWER_KIT_GRAPHICS_H

#include <stdint.h>

#include "font.h"
#include "images.h"

// 分块大小，即DMA传输同一颜色时，uint16_t缓冲区的大小
#define LCD_FILL_BLOCK_COUNT    1024

// 清屏
void GFX_ClearScreen(uint16_t color);

// 单个像素绘制
void GFX_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

// 直线（支持水平/垂直/斜线）
void GFX_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

// 虚线（可控制点数与间隔）
void GFX_DrawDottedLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                        uint16_t color, uint16_t interval, uint16_t dot_num);

// 矩形（实心/空心）
void GFX_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                  uint16_t color, uint8_t is_filled);

// 圆角矩形（实心/空心）
void GFX_DrawRoundRect(uint16_t x, uint16_t y, uint16_t x_end, uint16_t y_end,
                       uint16_t r, uint16_t color, uint8_t is_filled);

// 圆（实心/空心）
void GFX_DrawCircle(uint16_t x0, uint16_t y0, uint16_t r,
                    uint16_t color, uint8_t is_filled);

// 椭圆（实心/空心）
void GFX_DrawEllipse(uint16_t x0, uint16_t y0, uint16_t a, uint16_t b,
                     uint16_t color, uint8_t is_filled);

void GFX_DrawIsoscelesTriangle(uint16_t x0, uint16_t y0, uint16_t direction, uint16_t height, uint16_t color);

// 图片绘制（从文件系统加载并绘制，支持任意尺寸）
void GFX_DrawImage(const ImageInfo* pic, uint16_t x, uint16_t y);

void GFX_DrawString(uint16_t x, uint16_t y, const char* str, const FontInfo* font,
            uint16_t ftColor, uint16_t bgColor, int8_t spacing);


#endif // XM_POWER_KIT_GRAPHICS_H