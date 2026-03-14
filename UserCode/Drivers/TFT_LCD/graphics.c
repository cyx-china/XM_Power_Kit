#include "graphics.h"

#include <stdlib.h>
#include <math.h>

#include "cmsis_os2.h"
#include "dma.h"
#include "ff.h"
#include "images.h"
#include "freertos_os2.h"
#include "ST7789.h"
#include "font.h"
#include "os_handles.h"
// -------------------------- 定义&声明 -------------------------- //
static uint16_t FLUSH_BUFFER[LCD_FILL_BLOCK_COUNT];
static inline uint16_t alpha_blend_565(uint16_t fg, uint16_t bg, uint8_t alpha_0255);
// -------------------------- 辅助函数 -------------------------- //
// 内联函数，用于快速将一段内存填充为相同数据（需确保缓冲区大小是8的倍数！）
__STATIC_FORCEINLINE void fast_fill_uint16(uint16_t *dst, uint16_t val, uint32_t count) {
    register uint32_t i;
    const uint32_t loop_cnt = count / 8;

    for (i = 0; i < loop_cnt; i++) {
        // 指针自增合并，使用高效的STRH指令
        *dst++ = val;
        *dst++ = val;
        *dst++ = val;
        *dst++ = val;
        *dst++ = val;
        *dst++ = val;
        *dst++ = val;
        *dst++ = val;
    }
}

// 向矩形内填充相同的颜色（支持横线） 使用DMA加速
static void LCD_FillWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    uint32_t pixel_num = (x2 - x1 + 1) * (y2 - y1 + 1); // 计算需要填充的像素数量
    uint16_t page_count = (pixel_num / LCD_FILL_BLOCK_COUNT); // 计算需要的分页数量
    // 根据像素数量，选择是否需要填充整个缓冲区
    fast_fill_uint16(FLUSH_BUFFER, color, LCD_FILL_BLOCK_COUNT);
    // 选中填充区域
    LCD_SET_WINDOWS(x1, y1, x2, y2);
    // 刷写完整页
    for (uint16_t i = 0; i < page_count; i++) {
        LCD_FLUSH_DMA((uint32_t) FLUSH_BUFFER,LCD_FILL_BLOCK_COUNT);
    }
    // 处理剩余像素
    uint16_t pixel_left = pixel_num % LCD_FILL_BLOCK_COUNT;
    if (pixel_left > 0) {
        LCD_FLUSH_DMA((uint32_t) FLUSH_BUFFER, pixel_left);
    }
}

// -------------------------- 图形绘制 -------------------------- //

// 清屏函数
void GFX_ClearScreen(uint16_t color) {
    LCD_FillWindow(0, 0, 319, 239, color);
}

// ================ 画点 =============== //
// 画点
void GFX_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    LCD_SetPoint(x, y);
    LCD_WR_DATA(color);
}

// ================ 画线 =============== //
// 画直线
void GFX_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    // 如果是直线，就直接调用 LCD_FillWindow 快速填充
    if (x1 == x2 || y1 == y2) {
        LCD_FillWindow(x1, y1, x2, y2, color);
    }
    // 如果是斜线，就逐点绘制
    else {
        int16_t dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
        int16_t dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
        int16_t err = dx + dy;

        for (;;) {
            GFX_DrawPixel(x1, y1, color);
            if (x1 == x2 && y1 == y2) break;
            int16_t e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x1 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y1 += sy;
            }
        }
    }
}

// 画虚线（支持定义 间隔长度 和 连续长度）
void GFX_DrawDottedLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color, uint16_t interval,
                        uint16_t dot_num) {
    // 计算方向向量和直线总长度
    int32_t dx = (int32_t) x2 - (int32_t) x1;
    int32_t dy = (int32_t) y2 - (int32_t) y1;
    float line_len = sqrt((float) (dx * dx + dy * dy));

    // 处理起点终点重合或无连续点
    if (line_len < 1e-6f || dot_num == 0) {
        if (dot_num > 0) GFX_DrawPixel(x1, y1, color);
        return;
    }

    // 计算单位向量（用于沿直线均匀移动）
    float ux = dx / line_len; // x方向单位步长
    float uy = dy / line_len; // y方向单位步长

    // 计算每个"点组"的总长度（连续点长度 + 间隔长度）
    float group_len = dot_num + interval;

    // 循环绘制所有点组
    for (float pos = 0; pos < line_len; pos += group_len) {
        // 绘制当前组的连续点
        for (uint16_t k = 0; k < dot_num; k++) {
            // 计算当前点在直线上的距离
            float current_pos = pos + k;
            if (current_pos >= line_len) break; // 超出直线范围则停止

            // 计算当前点的坐标
            float x = x1 + ux * current_pos;
            float y = y1 + uy * current_pos;

            // 四舍五入到整数像素并绘制
            uint16_t draw_x = (uint16_t) (x + 0.5f);
            uint16_t draw_y = (uint16_t) (y + 0.5f);
            GFX_DrawPixel(draw_x, draw_y, color);
        }
    }
}


// =============== 画矩形 =============== //
// 绘制矩形 (空心/实心合并)
void GFX_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color, uint8_t is_filled) {
    if (is_filled) {
        LCD_FillWindow(x1, y1, x2, y2, color);
    } else {
        GFX_DrawLine(x1, y1, x2, y1, color); // 上边框
        GFX_DrawLine(x2, y1, x2, y2, color); // 右边框
        GFX_DrawLine(x1, y1, x1, y2, color); // 左边框
        GFX_DrawLine(x1, y2, x2, y2, color); // 下边框
    }
}

static inline uint16_t alpha_blend_565(uint16_t fg, uint16_t bg, uint8_t alpha_0255) {
    // 提取分量，注意RGB565的位布局：R(5位)G(6位)B(5位)
    uint32_t fg_r = (fg >> 11) & 0x1F;     // 红色分量：位11-15
    uint32_t fg_g = (fg >> 5)  & 0x3F;     // 绿色分量：位5-10
    uint32_t fg_b = fg & 0x1F;             // 蓝色分量：位0-4

    uint32_t bg_r = (bg >> 11) & 0x1F;
    uint32_t bg_g = (bg >> 5)  & 0x3F;
    uint32_t bg_b = bg & 0x1F;

    // 计算混合后的分量（添加四舍五入）
    uint32_t r = ((fg_r * alpha_0255) + (bg_r * (255 - alpha_0255)) + 127) >> 8;
    uint32_t g = ((fg_g * alpha_0255) + (bg_g * (255 - alpha_0255)) + 127) >> 8;
    uint32_t b = ((fg_b * alpha_0255) + (bg_b * (255 - alpha_0255)) + 127) >> 8;

    // 重新组合为RGB565
    return (uint16_t)((r << 11) | (g << 5) | b);
}

// 绘制圆角矩形 (空心/实心合并)
void GFX_DrawRoundRect(uint16_t x, uint16_t y, uint16_t x_end, uint16_t y_end,
                       uint16_t r, uint16_t color, uint8_t is_filled) {
    if (x_end < x || y_end < y) return;

    uint16_t width = x_end - x + 1;
    uint16_t height = y_end - y + 1;
    uint16_t max_r = (width < height) ? (width / 2) : (height / 2);
    if (r > max_r) r = max_r;
    if (r == 0) {
        GFX_DrawRect(x, y, x_end, y_end, color, is_filled);
        return;
    }

    if (is_filled) {
        // ---------------- 实心圆角矩形 ----------------
        LCD_FillWindow(x, y + r, x_end, y_end - r, color); // 中间竖直
        LCD_FillWindow(x + r, y, x_end - r, y + r - 1, color); // 上横条
        LCD_FillWindow(x + r, y_end - r + 1, x_end - r, y_end, color); // 下横条

        int64_t rr = (int64_t) r * r; // r²

        // ==================== 左上角 ====================
        {
            int32_t cx = x + r;
            int32_t cy = y + r;
            int32_t dx = r;

            for (int32_t dy = 0; dy <= r; dy++) {
                while (dx > 0 && (int64_t) dx * dx + (int64_t) dy * dy > rr) dx--;

                if (dx < 0) continue;

                uint16_t row = (uint16_t) (cy - dy);
                uint16_t x_left = (uint16_t) (cx - dx); // 当前行圆弧最左
                uint16_t x_right = (uint16_t) cx;

                // 快速填充到整数边界
                LCD_FillWindow(x_left, row, x_right, row, color);

                // 边界外侧 1~2 个像素做动态 alpha 混合
                for (int i = 1; i <= 2; i++) {
                    uint16_t px = (uint16_t) (cx - dx - i);
                    if (px < x) break;

                    int64_t dist2 = (int64_t) (dx + i) * (dx + i) + (int64_t) dy * dy;
                    int64_t diff = dist2 - rr;
                    if (diff > 600) continue; // 太远，不画

                    uint8_t alpha = (uint8_t) (255 - (diff * 255LL) / (4LL * r + 10));
                    if (alpha == 0) continue;

                    uint16_t bg = LCD_ReadPoint(px, row);
                    uint16_t mixed = alpha_blend_565(color, bg, alpha);
                    GFX_DrawPixel(px, row, mixed);
                }
            }
        }

        // ==================== 右上角 ====================
        {
            int32_t cx = x_end - r;
            int32_t cy = y + r;
            int32_t dx = r;

            for (int32_t dy = 0; dy <= r; dy++) {
                while (dx > 0 && (int64_t) dx * dx + (int64_t) dy * dy > rr) dx--;

                if (dx < 0) continue;

                uint16_t row = (uint16_t) (cy - dy);
                uint16_t x_left = (uint16_t) cx;
                uint16_t x_right = (uint16_t) (cx + dx);

                LCD_FillWindow(x_left, row, x_right, row, color);

                for (int i = 1; i <= 2; i++) {
                    uint16_t px = (uint16_t) (cx + dx + i);
                    if (px > x_end) break;

                    int64_t dist2 = (int64_t) (dx + i) * (dx + i) + (int64_t) dy * dy;
                    int64_t diff = dist2 - rr;
                    if (diff > 600) continue;

                    uint8_t alpha = (uint8_t) (255 - (diff * 255LL) / (4LL * r + 10));
                    if (alpha == 0) continue;

                    uint16_t bg = LCD_ReadPoint(px, row);
                    uint16_t mixed = alpha_blend_565(color, bg, alpha);
                    GFX_DrawPixel(px, row, mixed);
                }
            }
        }

        // ==================== 左下角 ====================
        {
            int32_t cx = x + r;
            int32_t cy = y_end - r;
            int32_t dx = r;

            for (int32_t dy = 0; dy <= r; dy++) {
                while (dx > 0 && (int64_t) dx * dx + (int64_t) dy * dy > rr) dx--;

                if (dx < 0) continue;

                uint16_t row = (uint16_t) (cy + dy);
                uint16_t x_left = (uint16_t) (cx - dx);
                uint16_t x_right = (uint16_t) cx;

                LCD_FillWindow(x_left, row, x_right, row, color);

                for (int i = 1; i <= 2; i++) {
                    uint16_t px = (uint16_t) (cx - dx - i);
                    if (px < x) break;

                    int64_t dist2 = (int64_t) (dx + i) * (dx + i) + (int64_t) dy * dy;
                    int64_t diff = dist2 - rr;
                    if (diff > 600) continue;

                    uint8_t alpha = (uint8_t) (255 - (diff * 255LL) / (4LL * r + 10));
                    if (alpha == 0) continue;

                    uint16_t bg = LCD_ReadPoint(px, row);
                    uint16_t mixed = alpha_blend_565(color, bg, alpha);
                    GFX_DrawPixel(px, row, mixed);
                }
            }
        }

        // ==================== 右下角 ====================
        {
            int32_t cx = x_end - r;
            int32_t cy = y_end - r;
            int32_t dx = r;

            for (int32_t dy = 0; dy <= r; dy++) {
                while (dx > 0 && (int64_t) dx * dx + (int64_t) dy * dy > rr) dx--;

                if (dx < 0) continue;

                uint16_t row = (uint16_t) (cy + dy);
                uint16_t x_left = (uint16_t) cx;
                uint16_t x_right = (uint16_t) (cx + dx);

                LCD_FillWindow(x_left, row, x_right, row, color);

                for (int i = 1; i <= 2; i++) {
                    uint16_t px = (uint16_t) (cx + dx + i);
                    if (px > x_end) break;

                    int64_t dist2 = (int64_t) (dx + i) * (dx + i) + (int64_t) dy * dy;
                    int64_t diff = dist2 - rr;
                    if (diff > 600) continue;

                    uint8_t alpha = (uint8_t) (255 - (diff * 255LL) / (4LL * r + 10));
                    if (alpha == 0) continue;

                    uint16_t bg = LCD_ReadPoint(px, row);
                    uint16_t mixed = alpha_blend_565(color, bg, alpha);
                    GFX_DrawPixel(px, row, mixed);
                }
            }
        }
    } else {
        // ---------------- 空心圆角矩形绘制逻辑 ----------------
        // 1. 绘制四条直线边（避开圆角区域） (不变)
        GFX_DrawLine(x + r, y, x_end - r, y, color);      // 上边
        GFX_DrawLine(x + r, y_end, x_end - r, y_end, color); // 下边
        GFX_DrawLine(x, y + r, x, y_end - r, color);      // 左边
        GFX_DrawLine(x_end, y + r, x_end, y_end - r, color); // 右边

        // 2. 绘制四个圆角（使用 Bresenham 圆弧算法，确保连续）
        // 修改：替换 sqrt 方法为整数 Bresenham 算法，绘制第二象限弧（左上）
        int32_t cx = x + r;  // 左上圆心 x
        int32_t cy = y + r;  // 左上圆心 y
        int32_t px = 0;
        int32_t py = r;
        int32_t pd = 3 - 2 * r;  // 初始决策参数 (有符号)
        while (px <= py) {
            GFX_DrawPixel(cx - px, cy - py, color);  // 第二象限点 (对称于 x/y)
            GFX_DrawPixel(cx - py, cy - px, color);  // 补充对称点，确保弧覆盖
            if (pd < 0) {
                pd += 4 * px + 6;
            } else {
                pd += 4 * (px - py) + 10;
                py--;
            }
            px++;
        }

        // 右上角圆角（第一象限）
        cx = x_end - r;
        cy = y + r;
        px = 0;
        py = r;
        pd = 3 - 2 * r;
        while (px <= py) {
            GFX_DrawPixel(cx + px, cy - py, color);
            GFX_DrawPixel(cx + py, cy - px, color);
            if (pd < 0) {
                pd += 4 * px + 6;
            } else {
                pd += 4 * (px - py) + 10;
                py--;
            }
            px++;
        }

        // 左下角圆角（第三象限）
        cx = x + r;
        cy = y_end - r;
        px = 0;
        py = r;
        pd = 3 - 2 * r;
        while (px <= py) {
            GFX_DrawPixel(cx - px, cy + py, color);
            GFX_DrawPixel(cx - py, cy + px, color);
            if (pd < 0) {
                pd += 4 * px + 6;
            } else {
                pd += 4 * (px - py) + 10;
                py--;
            }
            px++;
        }

        // 右下角圆角（第四象限）
        cx = x_end - r;
        cy = y_end - r;
        px = 0;
        py = r;
        pd = 3 - 2 * r;
        while (px <= py) {
            GFX_DrawPixel(cx + px, cy + py, color);
            GFX_DrawPixel(cx + py, cy + px, color);
            if (pd < 0) {
                pd += 4 * px + 6;
            } else {
                pd += 4 * (px - py) + 10;
                py--;
            }
            px++;
        }
    }
}

// 绘制圆形 (空心/实心合并)
void GFX_DrawCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color, uint8_t is_filled) {
    if (is_filled) {
        // (不变，实心模式已正常)
        for (uint16_t y = 0; y <= r; y++) {
            uint16_t x = (int16_t) sqrt((double) (r * r - y * y));
            LCD_FillWindow(x0 - x, y0 - y, x0 + x, y0 - y, color);
            if (y != 0) {
                LCD_FillWindow(x0 - x, y0 + y, x0 + x, y0 + y, color);
            }
        }
    } else {
        // 修改：将 d 改为 int32_t，支持负值
        uint16_t x = 0;
        uint16_t y = r;
        int32_t d = 3 - 2 * r; // 修改为 int32_t

        while (x <= y) {
            // 利用圆的八对称性绘制8个对称点 (不变)
            GFX_DrawPixel(x0 + x, y0 + y, color);
            GFX_DrawPixel(x0 - x, y0 + y, color);
            GFX_DrawPixel(x0 + x, y0 - y, color);
            GFX_DrawPixel(x0 - x, y0 - y, color);
            GFX_DrawPixel(x0 + y, y0 + x, color);
            GFX_DrawPixel(x0 - y, y0 + x, color);
            GFX_DrawPixel(x0 + y, y0 - x, color);
            GFX_DrawPixel(x0 - y, y0 - x, color);

            // 中点圆算法迭代 (不变，但 d 支持负)
            if (d < 0) {
                d = d + 4 * x + 6;
            } else {
                d = d + 4 * (x - y) + 10;
                y--;
            }
            x++;
        }
    }
}

// =============== 画椭圆 =============== //
// 绘制椭圆 (空心/实心合并)
void GFX_DrawEllipse(uint16_t x0, uint16_t y0, uint16_t a, uint16_t b, uint16_t color, uint8_t is_filled) {
    if (is_filled) {
        int32_t b2 = (int32_t) b * b;

        // 遍历Y方向，计算每个y对应的x范围并填充水平线
        for (int32_t y = -b; y <= b; y++) {
            // 计算当前y对应的x最大值（椭圆方程：x²/a² + y²/b² = 1 → x = a√(1 - y²/b²)）
            int32_t y_sq = y * y;
            if (y_sq > b2) continue; // 超出短轴范围，跳过
            int32_t x_max = (int32_t) (a * sqrt(1.0f - (float) y_sq / b2) + 0.5f);

            // 绘制当前y对应的水平线段（上下对称，y=0时只画一次）
            uint16_t draw_y = y0 + y;
            if (draw_y < LCD_LINE_COUNT) {
                // 边界检查
                LCD_FillWindow(x0 - x_max, draw_y, x0 + x_max, draw_y, color);
            }
        }
    } else {
        int32_t x = 0;
        int32_t y = b;
        // 椭圆初始判别式（第一象限，从(0,b)开始）
        int32_t a2 = (int32_t) a * a; // a²
        int32_t b2 = (int32_t) b * b; // b²
        int32_t two_a2 = 2 * a2; // 2a²
        int32_t two_b2 = 2 * b2; // 2b²
        int32_t d = b2 - (a2 * b) + (a2 / 4); // 初始决策参数

        while (two_b2 * x <= two_a2 * y) {
            // 绘制四象限对称点
            GFX_DrawPixel(x0 + x, y0 + y, color);
            GFX_DrawPixel(x0 - x, y0 + y, color);
            GFX_DrawPixel(x0 + x, y0 - y, color);
            GFX_DrawPixel(x0 - x, y0 - y, color);

            x++;
            if (d < 0) {
                // 选择上方点，更新判别式
                d += two_b2 * x + b2;
            } else {
                // 选择右下方点，更新判别式
                y--;
                d += two_b2 * x - two_a2 * y + b2;
            }
        }

        // 处理椭圆第二部分（斜率绝对值 > 1 的区域）
        d = b2 * (x + 0.5) * (x + 0.5) + a2 * (y - 1) * (y - 1) - a2 * b2;
        while (y >= 0) {
            // 绘制四象限对称点
            GFX_DrawPixel(x0 + x, y0 + y, color);
            GFX_DrawPixel(x0 - x, y0 + y, color);
            GFX_DrawPixel(x0 + x, y0 - y, color);
            GFX_DrawPixel(x0 - x, y0 - y, color);

            y--;
            if (d > 0) {
                // 选择右方点，更新判别式
                d += a2 - two_a2 * y;
            } else {
                // 选择右下方点，更新判别式
                x++;
                d += two_b2 * x - two_a2 * y + a2;
            }
        }
    }
}

#define IMAGE_MAX_BUFFER_SIZE  1024

// -------------------------- 图片绘制 -------------------------- //
void GFX_DrawImage(const ImageInfo *pic, uint16_t x, uint16_t y) {
    if (pic == NULL || pic->width == 0 || pic->height == 0) {
        return;
    }

    uint32_t total_bytes = (uint32_t) pic->width * pic->height * 2; // 每像素2字节（RGB565）
    if (total_bytes == 0) return;

    res = f_open(&fil, pic->path, FA_READ);
    if (res != FR_OK) return;

    if (f_size(&fil) != total_bytes) {
        f_close(&fil);
        return;
    }

    // 分配缓冲区（不超过最大限制）
    uint32_t buffer_size = (total_bytes < IMAGE_MAX_BUFFER_SIZE) ? total_bytes : IMAGE_MAX_BUFFER_SIZE;
    uint8_t *buffer = malloc(buffer_size);
    if (buffer == NULL) {
        f_close(&fil);
        return;
    }

    uint32_t row_bytes = pic->width * 2;          // 每行数据字节数
    uint32_t offset = 0;
    uint32_t bytes_left = total_bytes;
    uint16_t cur_y = y;

    while (bytes_left > 0) {
        // 本次期望读取的字节数（不超过缓冲区大小）
        uint32_t bytes_this_time = (bytes_left < buffer_size) ? bytes_left : buffer_size;

        // 调整为行字节数的整数倍，确保数据与行边界对齐
        if (bytes_this_time > row_bytes) {
            bytes_this_time = (bytes_this_time / row_bytes) * row_bytes;
        }

        // 如果调整后为0（理论上不会发生，但为了安全，强制读取一行）
        if (bytes_this_time == 0) {
            bytes_this_time = row_bytes;
        }

        // 定位到文件偏移并读取数据
        res = f_lseek(&fil, offset);
        if (res != FR_OK) break;

        res = f_read(&fil, buffer, bytes_this_time, &br);
        if (res != FR_OK || br != bytes_this_time) break;

        // 计算本次能绘制的完整行数
        uint16_t block_height = bytes_this_time / row_bytes;

        // 设置LCD窗口并刷入数据
        LCD_SET_WINDOWS(x, cur_y, x + pic->width - 1, cur_y + block_height - 1);
        LCD_FLUSH_DMA((uint32_t) buffer, bytes_this_time / 2); // 像素数 = 字节数/2

        // 更新偏移、剩余字节和当前行
        offset += bytes_this_time;
        bytes_left -= bytes_this_time;
        cur_y += block_height;
    }

    free(buffer);
    f_close(&fil);
}
// -------------------------- 文字绘制 -------------------------- //
// 获取utf-8编码的编码有效长度
static inline uint8_t get_utf8_len(const char *str) {
    uint8_t len = 0;
    if ((*str & 0x80) == 0) {
        len = 1; // ASCII
    } else if ((*str & 0xE0) == 0xC0) {
        len = 2; // 2字节UTF-8
    } else if ((*str & 0xF0) == 0xE0) {
        len = 3; // 3字节UTF-8
    } else if ((*str & 0xF8) == 0xF0) {
        len = 4; // 4字节UTF-8
    }
    return len;
}

// 在字体映射表中查找字符的索引
static int16_t find_char_index(const FontInfo *font, const char *string) {
    uint8_t len = get_utf8_len(string);
    if (len == 0) return 0;

    for (uint16_t i = 0; i < font->font_num; i++) {
        uint8_t *head = (uint8_t *) font->font_map[i];
        if (memcmp(head, string, len) == 0) {
            return i;
        }
    }
    return -1; // 未找到，返回-1
}

// RGB565通道拆分/合并宏
#define RGB565_GET_R(color)    (((color) >> 11) & 0x1F)
#define RGB565_GET_G(color)    (((color) >> 5)  & 0x3F)
#define RGB565_GET_B(color)    ((color) & 0x1F)
#define RGB565_MERGE(r, g, b)  (((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F))

// 批量灰度图转RGB565
static void batch_gray_to_rgb565(const uint8_t *gray_buf, uint16_t *flush_buf, uint32_t pixel_num, uint16_t ftColor,
                                 uint16_t bgColor) {
    register uint8_t ft_r = RGB565_GET_R(ftColor);
    register uint8_t ft_g = RGB565_GET_G(ftColor);
    register uint8_t ft_b = RGB565_GET_B(ftColor);
    register uint8_t bg_r = RGB565_GET_R(bgColor);
    register uint8_t bg_g = RGB565_GET_G(bgColor);
    register uint8_t bg_b = RGB565_GET_B(bgColor);

    volatile uint32_t i;
    uint32_t remain = pixel_num % 4; // 处理不能被4整除的剩余像素
    uint32_t main_num = pixel_num - remain;

    // 主循环：每次处理4个像素，减少循环次数
    for (i = 0; i < main_num; i += 4) {
        // 像素1
        uint8_t g1 = gray_buf[i];
        uint32_t r1 = bg_r * (255 - g1) + ft_r * g1;
        uint32_t g1_ = bg_g * (255 - g1) + ft_g * g1;
        uint32_t b1 = bg_b * (255 - g1) + ft_b * g1;
        flush_buf[i] = RGB565_MERGE((r1+127)/255, (g1_+127)/255, (b1+127)/255);

        // 像素2
        uint8_t g2 = gray_buf[i + 1];
        uint32_t r2 = bg_r * (255 - g2) + ft_r * g2;
        uint32_t g2_ = bg_g * (255 - g2) + ft_g * g2;
        uint32_t b2 = bg_b * (255 - g2) + ft_b * g2;
        flush_buf[i + 1] = RGB565_MERGE((r2+127)/255, (g2_+127)/255, (b2+127)/255);

        // 像素3
        uint8_t g3 = gray_buf[i + 2];
        uint32_t r3 = bg_r * (255 - g3) + ft_r * g3;
        uint32_t g3_ = bg_g * (255 - g3) + ft_g * g3;
        uint32_t b3 = bg_b * (255 - g3) + ft_b * g3;
        flush_buf[i + 2] = RGB565_MERGE((r3+127)/255, (g3_+127)/255, (b3+127)/255);

        // 像素4
        uint8_t g4 = gray_buf[i + 3];
        uint32_t r4 = bg_r * (255 - g4) + ft_r * g4;
        uint32_t g4_ = bg_g * (255 - g4) + ft_g * g4;
        uint32_t b4 = bg_b * (255 - g4) + ft_b * g4;
        flush_buf[i + 3] = RGB565_MERGE((r4+127)/255, (g4_+127)/255, (b4+127)/255);
    }

    // 处理剩余像素
    for (; i < pixel_num; i++) {
        uint8_t g = gray_buf[i];
        uint32_t r = bg_r * (255 - g) + ft_r * g;
        uint32_t g_ = bg_g * (255 - g) + ft_g * g;
        uint32_t b = bg_b * (255 - g) + ft_b * g;
        flush_buf[i] = RGB565_MERGE((r+127)/255, (g_+127)/255, (b+127)/255);
    }
}

// 计算小数点的有效显示宽度（左半边：font_width/2 +1）
static inline uint8_t get_dot_effective_width(const FontInfo *font) {
    return (font->font_width * 0.6 ) + 1;
}

// 绘制小数点（仅左半边显示）专门优化版本
static void GFX_DrawDotChar(FIL *FIL, uint16_t x, uint16_t y, const char *ch, const FontInfo *font,
                            uint16_t ftColor, uint16_t bgColor, uint8_t *gray_buffer, uint16_t *flush_buffer) {
    int16_t index = find_char_index(font, ch); // 获取字符索引
    if (index == -1) { return; } // 字符未找到，直接返回

    uint32_t full_pixel_num = font->font_width * font->font_height; // 完整字符像素数
    uint8_t dot_width = get_dot_effective_width(font); // 小数点有效宽度
    uint32_t dot_pixel_num = dot_width * font->font_height; // 小数点有效像素数

    if (FIL == NULL || gray_buffer == NULL || flush_buffer == NULL) {
        return; // 参数检查，确保指针有效
    }

    // 从文件中读取完整的小数点字模数据
    res = f_lseek(FIL, index * full_pixel_num); // 定位到字模数据位置
    if (res != FR_OK) { return; }

    res = f_read(FIL, gray_buffer, full_pixel_num, &br); // 读取完整字模数据
    if (res != FR_OK || br != full_pixel_num) { return; } // 读取失败或字节数不匹配

    // 裁剪灰度数据：仅保留左半边（逐行截取前dot_width个像素）
    for (uint16_t row = 0; row < font->font_height; row++) {
        uint8_t *src_row = &gray_buffer[row * font->font_width]; // 源行起始
        uint8_t *dst_row = &gray_buffer[row * dot_width]; // 目标行起始
        memcpy(dst_row, src_row, dot_width); // 复制左半边像素
    }

    // 灰度转RGB565（仅处理有效像素数）
    batch_gray_to_rgb565(gray_buffer, flush_buffer, dot_pixel_num, ftColor, bgColor);

    // 设置LCD窗口（仅左半边区域）并发送数据
    LCD_SET_WINDOWS(x, y, x + dot_width - 1, y + font->font_height - 1);
    LCD_FLUSH_DMA((uint32_t) flush_buffer, dot_pixel_num);
}

// 绘制单个字符(作为绘制字符串的底层函数，外部接口为 GFX_DrawString)
static void GFX_DrawChar(FIL *FIL, uint16_t x, uint16_t y, const char *ch, const FontInfo *font,
                         uint16_t ftColor, uint16_t bgColor, uint8_t *gray_buffer, uint16_t *flush_buffer) {
    int16_t index = find_char_index(font, ch); // 获取字符索引
    if (index == -1) { return; } // 字符未找到，直接返回

    uint32_t pixel_num = font->font_width * font->font_height; // 计算像素总数

    if (FIL == NULL || gray_buffer == NULL || flush_buffer == NULL) {
        return; // 参数检查，确保指针有效
    }

    // 从文件中读取字模数据到灰度缓冲区
    res = f_lseek(FIL, index * pixel_num); // 定位到字模数据位置
    if (res != FR_OK) { return; }

    res = f_read(FIL, gray_buffer, pixel_num, &br); // 读取字模数据
    if (res != FR_OK || br != pixel_num) { return; } // 读取失败或字节数不匹配

    // 根据灰度数据，融合前景色和背景色生成最终的RGB565数据到Flush_Buffer
    batch_gray_to_rgb565(gray_buffer, flush_buffer, pixel_num, ftColor, bgColor);

    // 设置LCD窗口并使用DMA发送Flush_Buffer数据到LCD
    LCD_SET_WINDOWS(x, y, x + font->font_width - 1, y + font->font_height - 1);
    LCD_FLUSH_DMA((uint32_t) flush_buffer, pixel_num);
}

void GFX_DrawString(uint16_t x, uint16_t y, const char *str, const FontInfo *font,
                    uint16_t ftColor, uint16_t bgColor, int8_t spacing) {
    if (str == NULL || font == NULL) {
        return; // 参数检查，确保指针有效
    }

    res = f_open(&fil, font->path, FA_READ);
    if (res != FR_OK) {
        return; // 打开字体文件失败
    }

    uint32_t pixel_num = font->font_width * font->font_height;
    uint8_t *gray_buffer = malloc(pixel_num); // 字模灰度缓冲区
    uint16_t *flush_buffer = malloc(pixel_num * 2); // RGB565缓冲区

    if (gray_buffer == NULL || flush_buffer == NULL) {
        f_close(&fil);
        if (gray_buffer) free(gray_buffer);
        if (flush_buffer) free(flush_buffer);
        return; // 内存分配失败
    }

    uint16_t cursor_x = x;
    const char *p = str;

    while (*p) {
        uint8_t utf8_len = get_utf8_len(p);
        if (utf8_len == 0) break; // 无效UTF-8编码，停止处理

        // 1. 先获取字符索引（提前判断是否找到字符）
        int16_t char_index = find_char_index(font, p);
        // 2. 判断是否为小数点（.）：仅单字节ASCII 0x2E
        uint8_t is_dot = (utf8_len == 1 && (uint8_t) *p == 0x2E) ? 1 : 0;
        uint16_t char_step = 0; // 初始化字符步进值

        // 3. 核心逻辑：根据字符是否找到 + 是否为小数点，计算步进值
        if (char_index == -1) {
            // 字符未找到：步进值 = 0.7 * 字体宽度 向上取整
            char_step = (uint16_t) ceil(font->font_width * 0.7f);
        } else if (is_dot) {
            // 小数点：使用原有有效宽度
            char_step = get_dot_effective_width(font);
        } else {
            // 普通字符（找到）：使用原有完整宽度
            char_step = font->font_width;
        }

        // 4. 仅当字符找到时，才绘制字符
        if (char_index != -1) {
            if (is_dot) {
                GFX_DrawDotChar(&fil, cursor_x, y, p, font, ftColor, bgColor, gray_buffer, flush_buffer);
            } else {
                GFX_DrawChar(&fil, cursor_x, y, p, font, ftColor, bgColor, gray_buffer, flush_buffer);
            }
        }

        // 5. 字体间隔绘制（原有逻辑保留）
        if (spacing > 0 && *(p + utf8_len) != '\0') {
            int16_t spacing_x1 = cursor_x + char_step;
            int16_t spacing_x2 = spacing_x1 + spacing - 1;
            uint16_t spacing_y1 = y;
            uint16_t spacing_y2 = y + font->font_height - 1;

            GFX_DrawRect(spacing_x1, spacing_y1, spacing_x2, spacing_y2, bgColor, 1);
        }

        // 6. 移动游标：字符步进值 + 字间距（兼容正负）
        cursor_x += (char_step + spacing);
        p += utf8_len; // 移动到下一个UTF-8字符
    }

    free(gray_buffer);
    free(flush_buffer);
    f_close(&fil);
}
