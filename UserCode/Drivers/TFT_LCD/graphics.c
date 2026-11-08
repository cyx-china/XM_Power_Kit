#include "graphics.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "cmsis_os2.h"
#include "dma.h"
#include "ff.h"
#include "images.h"
#include "freertos_os2.h"
#include "ST7789.h"
#include "font.h"
#include "os_handles.h"
// -------------------------- 字体/图片文件句柄常驻缓存 -------------------------- //
// 一次 f_open 需要多次通过 SPI 读取目录项与 FAT，而文字/图片绘制是最高频的路径，
// 因此这里把用到的文件常驻打开，绘制时只做 seek/read，不再反复开关文件。
#define GFX_FILE_CACHE_NUM      4           // 同时常驻打开的文件数（字体/图片）
#define GFX_IMAGE_MAX_BYTES     1024        // 图片分块读取的缓冲上限（字节）
#define GFX_FLUSH_PIXELS        1600        // 单次刷屏缓冲最大像素数（32x50 是最大字模）

// 字模灰度缓存：字体文件在 SPI Flash 上，一次 4KB 扇区读取约 0.8ms，
// 缓存最近用到的字模后，重复刷新同样的数字/文字基本不再访问 Flash。
#ifndef GFX_GLYPH_CACHE_BYTES
#define GFX_GLYPH_CACHE_BYTES   6144
#endif
#define GFX_GLYPH_CACHE_ENTRIES 24

// RGB565通道拆分/合并宏
#define RGB565_GET_R(color)    (((color) >> 11) & 0x1F)
#define RGB565_GET_G(color)    (((color) >> 5)  & 0x3F)
#define RGB565_GET_B(color)    ((color) & 0x1F)
#define RGB565_MERGE(r, g, b)  (((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F))

typedef struct {
    FIL         fil;
    const char *path;       // 常驻字符串（字体/图片路径）
    uint32_t    stamp;      // 最近使用序号，用于淘汰
    uint8_t     used;
} GfxFileHandle_t;

typedef struct {
    const FontInfo *font;
    uint32_t        offset;     // 字模数据在 glyph_pool 中的偏移
    uint32_t        stamp;      // 最近使用序号，用于 LRU 淘汰
    uint16_t        index;      // 字模序号
    uint16_t        size;       // 字模字节数
} GfxGlyphEntry_t;

static GfxFileHandle_t  file_handles[GFX_FILE_CACHE_NUM];
static uint32_t         file_stamp;
static GfxGlyphEntry_t  glyph_entries[GFX_GLYPH_CACHE_ENTRIES];
static uint8_t          glyph_pool[GFX_GLYPH_CACHE_BYTES];
static uint16_t         glyph_pool_used;
static uint8_t          glyph_count;
static uint32_t         glyph_stamp;
static uint16_t         pixel_flush_buf[GFX_FLUSH_PIXELS];    // RGB565 刷屏缓冲
static uint16_t         blend_lut[256];                       // 灰度→RGB565 混合查表
static uint16_t         blend_lut_fg, blend_lut_bg;
static uint8_t          blend_lut_valid;

// -------------------------- 字模/图片文件访问 -------------------------- //
// 打开（或复用）文件，path 必须是常驻字符串
static FIL *gfx_file_get(const char *path) {
    if (path == NULL) { return NULL; }

    for (uint8_t i = 0; i < GFX_FILE_CACHE_NUM; i++) {
        if (file_handles[i].used && strcmp(file_handles[i].path, path) == 0) {
            file_handles[i].stamp = ++file_stamp;
            return &file_handles[i].fil;
        }
    }

    // 找空槽，没有空槽就淘汰最久未使用的
    uint8_t slot = 0;
    for (uint8_t i = 0; i < GFX_FILE_CACHE_NUM; i++) {
        if (!file_handles[i].used) { slot = i; break; }
        if (file_handles[i].stamp < file_handles[slot].stamp) { slot = i; }
    }
    if (file_handles[slot].used) {
        f_close(&file_handles[slot].fil);
        file_handles[slot].used = 0;
    }
    if (f_open(&file_handles[slot].fil, path, FA_READ) != FR_OK) { return NULL; }

    file_handles[slot].path  = path;
    file_handles[slot].stamp = ++file_stamp;
    file_handles[slot].used  = 1;
    return &file_handles[slot].fil;
}
// -------------------------- 定义&声明 -------------------------- //
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
    // 复用刷屏缓冲（原实现另有一个 2KB 的专用填充缓冲，这里复用 pixel_flush_buf 省下 2KB RAM；
    // 填充与字模绘制都是同步完成，不会互相踩踏）
    const uint32_t block = GFX_FLUSH_PIXELS;
    uint32_t pixel_num = (x2 - x1 + 1) * (y2 - y1 + 1); // 计算需要填充的像素数量
    uint32_t page_count = (pixel_num / block);          // 计算需要的分页数量
    // 根据像素数量，选择是否需要填充整个缓冲区
    fast_fill_uint16(pixel_flush_buf, color, block);
    // 选中填充区域
    LCD_SET_WINDOWS(x1, y1, x2, y2);
    // 刷写完整页
    for (uint32_t i = 0; i < page_count; i++) {
        LCD_FLUSH_DMA((uint32_t) pixel_flush_buf, block);
    }
    // 处理剩余像素
    uint32_t pixel_left = pixel_num % block;
    if (pixel_left > 0) {
        LCD_FLUSH_DMA((uint32_t) pixel_flush_buf, pixel_left);
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


// =============== 画等边三角 =============== //
void GFX_DrawIsoscelesTriangle(uint16_t x0, uint16_t y0, uint16_t direction, uint16_t height, uint16_t color) {
    int16_t x1, y1, x2, y2, c;
    for (uint16_t i = 0; i < height; i++) {
        // 根据方向计算线段端点
        switch (direction) {
            case 0: x1 = x0 - i; y1 = y0 + i; x2 = x0 + i; y2 = y0 + i; break; // 上
            case 1: x1 = x0 - i; y1 = y0 - i; x2 = x0 - i; y2 = y0 + i; break; // 右
            case 2: x1 = x0 - i; y1 = y0 - i; x2 = x0 + i; y2 = y0 - i; break; // 下
            case 3: x1 = x0 + i; y1 = y0 - i; x2 = x0 + i; y2 = y0 + i; break; // 左
            default: return;
        }

        // 统一裁剪逻辑 (X轴)
        if (x1 > x2) { c = x1; x1 = x2; x2 = c; }
        if (x2 < 0 || x1 > 319) continue;
        x1 = (x1 < 0) ? 0 : x1; x2 = (x2 > 319) ? 319 : x2;

        // 统一裁剪逻辑 (Y轴)
        if (y1 > y2) { c = y1; y1 = y2; y2 = c; }
        if (y2 < 0 || y1 > 239) continue;
        y1 = (y1 < 0) ? 0 : y1; y2 = (y2 > 239) ? 239 : y2;

        GFX_DrawLine(x1, y1, x2, y2, color);
    }
}


// -------------------------- 图片绘制 -------------------------- //
void GFX_DrawImage(const ImageInfo *pic, uint16_t x, uint16_t y) {
    if (pic == NULL || pic->width == 0 || pic->height == 0) {
        return;
    }

    uint32_t total_bytes = (uint32_t) pic->width * pic->height * 2; // 每像素2字节（RGB565）
    if (total_bytes == 0) { return; }

    // 图片只在切页时绘制，不占用字体缓存槽位
    static FIL image_fil;
    if (f_open(&image_fil, pic->path, FA_READ) != FR_OK) { return; }
    if (f_size(&image_fil) != total_bytes) {
        f_close(&image_fil);
        return;
    }

    // 复用刷屏缓冲，不再每次 malloc/free
    uint8_t *buffer = (uint8_t *) pixel_flush_buf;
    uint32_t row_bytes = (uint32_t) pic->width * 2; // 每行数据字节数
    uint32_t buffer_size = sizeof(pixel_flush_buf);
    if (buffer_size > GFX_IMAGE_MAX_BYTES) { buffer_size = GFX_IMAGE_MAX_BYTES; }
    if (buffer_size < row_bytes) { buffer_size = row_bytes; } // 至少能装下一行
    if (buffer_size > sizeof(pixel_flush_buf)) {
        f_close(&image_fil);
        return; // 图片单行超出缓冲
    }
    if (total_bytes < buffer_size) { buffer_size = total_bytes; }
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
        if (bytes_this_time == 0) { bytes_this_time = row_bytes; } // 理论上不会发生

        if (f_lseek(&image_fil, (FSIZE_t) offset) != FR_OK) { break; }
        if (f_read(&image_fil, buffer, bytes_this_time, &br) != FR_OK || br != bytes_this_time) { break; }

        uint16_t block_height = (uint16_t) (bytes_this_time / row_bytes); // 本次能绘制的完整行数
        LCD_SET_WINDOWS(x, cur_y, x + pic->width - 1, cur_y + block_height - 1);
        LCD_FLUSH_DMA((uint32_t) buffer, bytes_this_time / 2); // 像素数 = 字节数/2

        offset += bytes_this_time;
        bytes_left -= bytes_this_time;
        cur_y += block_height;
    }

    f_close(&image_fil);
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

// 在字体映射表中查找字符的索引（先比较首字节，避免大量 memcmp 调用）
static int16_t find_char_index(const FontInfo *font, const char *string) {
    uint8_t len = get_utf8_len(string);
    if (len == 0) { return -1; }

    for (uint16_t i = 0; i < font->font_num; i++) {
        const uint8_t *head = (const uint8_t *) font->font_map[i];
        if (head[0] != (uint8_t) string[0]) { continue; }
        if (len == 1) { return (int16_t) i; }
        if (memcmp(head + 1, string + 1, len - 1) == 0) { return (int16_t) i; }
    }
    return -1; // 未找到，返回-1
}

// 计算小数点的有效显示宽度（只显示左半边）
static inline uint8_t get_dot_effective_width(const FontInfo *font) {
    return (uint8_t) ((font->font_width * 6) / 10 + 1);
}

// 预先算好 前景色/背景色 的灰度混合结果，逐像素时只查表，不再做乘除法
static void gfx_blend_lut_prepare(uint16_t ftColor, uint16_t bgColor) {
    if (blend_lut_valid && blend_lut_fg == ftColor && blend_lut_bg == bgColor) { return; }

    register uint8_t ft_r = RGB565_GET_R(ftColor);
    register uint8_t ft_g = RGB565_GET_G(ftColor);
    register uint8_t ft_b = RGB565_GET_B(ftColor);
    register uint8_t bg_r = RGB565_GET_R(bgColor);
    register uint8_t bg_g = RGB565_GET_G(bgColor);
    register uint8_t bg_b = RGB565_GET_B(bgColor);

    for (uint32_t g = 0; g < 256; g++) {
        uint32_t r  = bg_r * (255 - g) + ft_r * g;
        uint32_t gg = bg_g * (255 - g) + ft_g * g;
        uint32_t b  = bg_b * (255 - g) + ft_b * g;
        blend_lut[g] = RGB565_MERGE((r + 127) / 255, (gg + 127) / 255, (b + 127) / 255);
    }
    blend_lut_fg = ftColor;
    blend_lut_bg = bgColor;
    blend_lut_valid = 1;
}

// 淘汰一个缓存项，并把后面的字模数据前移，避免产生碎片
static void gfx_glyph_remove(uint8_t idx) {
    uint32_t off = glyph_entries[idx].offset;
    uint16_t size = glyph_entries[idx].size;

    if (off + size < glyph_pool_used) {
        memmove(&glyph_pool[off], &glyph_pool[off + size], glyph_pool_used - off - size);
    }
    glyph_pool_used -= size;

    for (uint8_t i = 0; i < glyph_count; i++) {
        if (i != idx && glyph_entries[i].offset > off) {
            glyph_entries[i].offset -= size;
        }
    }
    for (uint8_t i = idx; i + 1 < glyph_count; i++) {
        glyph_entries[i] = glyph_entries[i + 1];
    }
    glyph_count--;
}

// 取字模灰度数据（带缓存），返回 font_width*font_height 字节的字模起始地址
static const uint8_t *gfx_glyph_get(const FontInfo *font, uint16_t index) {
    for (uint8_t i = 0; i < glyph_count; i++) {
        if (glyph_entries[i].font == font && glyph_entries[i].index == index) {
            glyph_entries[i].stamp = ++glyph_stamp;
            return &glyph_pool[glyph_entries[i].offset];
        }
    }

    uint32_t size = (uint32_t) font->font_width * font->font_height;
    if (size > GFX_GLYPH_CACHE_BYTES || size > 0xFFFF) { return NULL; }

    // 空间不足时按 LRU 淘汰最久未用的字模
    while (glyph_count >= GFX_GLYPH_CACHE_ENTRIES ||
           (uint32_t) glyph_pool_used + size > GFX_GLYPH_CACHE_BYTES) {
        if (glyph_count == 0) { return NULL; }
        uint8_t oldest = 0;
        for (uint8_t i = 1; i < glyph_count; i++) {
            if (glyph_entries[i].stamp < glyph_entries[oldest].stamp) { oldest = i; }
        }
        gfx_glyph_remove(oldest);
    }

    FIL *fp = gfx_file_get(font->path);
    if (fp == NULL) { return NULL; }
    if (f_lseek(fp, (FSIZE_t) index * size) != FR_OK) { return NULL; }

    uint8_t *dst = &glyph_pool[glyph_pool_used];
    if (f_read(fp, dst, size, &br) != FR_OK || br != size) { return NULL; }

    glyph_entries[glyph_count].font   = font;
    glyph_entries[glyph_count].index  = index;
    glyph_entries[glyph_count].offset = glyph_pool_used;
    glyph_entries[glyph_count].size   = (uint16_t) size;
    glyph_entries[glyph_count].stamp  = ++glyph_stamp;
    glyph_count++;
    glyph_pool_used += (uint16_t) size;
    return dst;
}

// 绘制单个字模：灰度 → RGB565 → 一次 DMA 刷屏
static void gfx_draw_glyph(const FontInfo *font, uint16_t index, uint16_t x, uint16_t y, uint8_t is_dot) {
    const uint8_t *gray = gfx_glyph_get(font, index);
    if (gray == NULL) { return; }

    uint16_t width  = font->font_width;
    uint16_t height = font->font_height;
    uint16_t draw_w = is_dot ? get_dot_effective_width(font) : width;
    if (draw_w > width) { draw_w = width; }

    uint32_t pixel_num = (uint32_t) draw_w * height;
    if (pixel_num > GFX_FLUSH_PIXELS) { return; }

    for (uint16_t row = 0; row < height; row++) {
        const uint8_t *src = gray + (uint32_t) row * width;
        uint16_t *dst = pixel_flush_buf + (uint32_t) row * draw_w;
        uint16_t col = 0;
        for (; col + 4 <= draw_w; col += 4) {
            dst[col]     = blend_lut[src[col]];
            dst[col + 1] = blend_lut[src[col + 1]];
            dst[col + 2] = blend_lut[src[col + 2]];
            dst[col + 3] = blend_lut[src[col + 3]];
        }
        for (; col < draw_w; col++) {
            dst[col] = blend_lut[src[col]];
        }
    }

    LCD_SET_WINDOWS(x, y, x + draw_w - 1, y + height - 1);
    LCD_FLUSH_DMA((uint32_t) pixel_flush_buf, pixel_num);
}

void GFX_DrawString(uint16_t x, uint16_t y, const char *str, const FontInfo *font,
                    uint16_t ftColor, uint16_t bgColor, int8_t spacing) {
    if (str == NULL || font == NULL || *str == '\0') {
        return; // 参数检查，确保指针有效
    }
    if ((uint32_t) font->font_width * font->font_height > GFX_FLUSH_PIXELS) {
        return; // 字模超出缓冲，避免越界
    }
    if (gfx_file_get(font->path) == NULL) {
        return; // 字体文件不可用
    }

    gfx_blend_lut_prepare(ftColor, bgColor);

    uint16_t cursor_x = x;
    const char *p = str;

    while (*p) {
        uint8_t utf8_len = get_utf8_len(p);
        if (utf8_len == 0) { break; } // 无效UTF-8编码，停止处理

        int16_t char_index = find_char_index(font, p);
        uint8_t is_dot = (utf8_len == 1 && (uint8_t) *p == 0x2E) ? 1 : 0;
        uint16_t char_step;

        if (char_index < 0) {
            char_step = (uint16_t) ((font->font_width * 7 + 9) / 10); // 未收录：按 0.7 倍宽度前进
        } else if (is_dot) {
            char_step = get_dot_effective_width(font); // 小数点：只占左半边
        } else {
            char_step = font->font_width; // 普通字符
        }

        if (char_index >= 0) {
            gfx_draw_glyph(font, (uint16_t) char_index, cursor_x, y, is_dot);
        }

        // 字体间隔绘制
        if (spacing > 0 && *(p + utf8_len) != '\0') {
            int16_t spacing_x1 = (int16_t) (cursor_x + char_step);
            int16_t spacing_x2 = (int16_t) (spacing_x1 + spacing - 1);
            uint16_t spacing_y1 = y;
            uint16_t spacing_y2 = y + font->font_height - 1;

            GFX_DrawRect(spacing_x1, spacing_y1, spacing_x2, spacing_y2, bgColor, 1);
        }

        // 移动游标：字符步进值 + 字间距（兼容正负）
        cursor_x = (uint16_t) ((int32_t) cursor_x + (int32_t) char_step + spacing);
        p += utf8_len; // 移动到下一个UTF-8字符
    }
}

/* ==================== 文件 / 字模缓存失效（供上位机文件管理使用） ==================== */
// graphics 的绘制全部在 LcdFlushTask 里执行，因此这里只做"请求"，
// 真正的关闭文件与清缓存由 LcdFlushTask 在自己的循环里调用 GFX_ServiceCacheInvalidate() 完成，
// 避免绘图线程正在读字模时把 FIL 关掉。
static volatile uint8_t s_gfx_invalid_req;
static volatile uint8_t s_gfx_invalid_done = 1U;

void GFX_RequestCacheInvalidate(void) {
    s_gfx_invalid_done = 0U;
    s_gfx_invalid_req  = 1U;
}

uint8_t GFX_CacheInvalidateDone(void) {
    return s_gfx_invalid_done;
}

void GFX_ServiceCacheInvalidate(void) {
    if (s_gfx_invalid_req == 0U) {
        return;
    }

    for (uint8_t i = 0; i < GFX_FILE_CACHE_NUM; i++) {
        if (file_handles[i].used) {
            (void)f_close(&file_handles[i].fil);
            file_handles[i].used = 0;
            file_handles[i].path = NULL;
            file_handles[i].stamp = 0;
        }
    }
    file_stamp = 0;

    glyph_count = 0;
    glyph_pool_used = 0;
    glyph_stamp = 0;
    blend_lut_valid = 0;

    s_gfx_invalid_req  = 0U;
    s_gfx_invalid_done = 1U;
}

