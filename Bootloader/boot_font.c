/**
 ******************************************************************************
 * @file    boot_font.c
 * @brief   bootloader 的外部 Flash 点阵字库（BFNT 格式）读取与绘制。
 *
 *  文件格式（小端）：
 *    0  "BFNT"
 *    4  u8  version
 *    5  u8  glyph_height
 *    6  u16 flags
 *    8  u32 glyph_count
 *   12  u32 index_offset（=16）
 *   16  count × 8 字节索引：u16 码位, u8 宽, u8 步进, u32 数据偏移
 *   数据：glyph_height 行 × ceil(宽/8) 字节，行主序，高位在左，1 = 实点
 ******************************************************************************
 */
#include "boot.h"
#include "ff.h"
#include <string.h>

#define FONT_PATH       "0:/font/boot.bin"
#define FONT_BUF_MAX    (16U * 1024U)
#define FONT_MAX_W      32U
#define FONT_MAX_H      32U

static uint8_t  s_buf[FONT_BUF_MAX];
static uint32_t s_len;
static uint32_t s_count;
static uint32_t s_index_off;
static uint8_t  s_height;
static bool     s_ready;
static uint16_t s_pix[FONT_MAX_W * FONT_MAX_H];

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void Boot_Font_Init(void)
{
    FIL file;
    UINT read = 0U;

    s_ready = false;
    s_count = 0U;

    if (f_open(&file, FONT_PATH, FA_READ) != FR_OK)
    {
        return;
    }

    if (f_read(&file, s_buf, FONT_BUF_MAX, &read) != FR_OK)
    {
        (void)f_close(&file);
        return;
    }
    (void)f_close(&file);

    if (read < 16U || memcmp(s_buf, "BFNT", 4U) != 0)
    {
        return;
    }

    s_height    = s_buf[5];
    s_count     = rd32(&s_buf[8]);
    s_index_off = rd32(&s_buf[12]);

    if (s_height == 0U || s_height > FONT_MAX_H || s_count == 0U ||
        (s_index_off + (s_count * 8U)) > read)
    {
        return;
    }

    s_len   = read;
    s_ready = true;
}

bool Boot_Font_Ready(void)
{
    return s_ready;
}

static const uint8_t *find_glyph(uint16_t cp, uint8_t *width, uint8_t *advance)
{
    for (uint32_t i = 0U; i < s_count; i++)
    {
        const uint8_t *entry = &s_buf[s_index_off + (i * 8U)];

        if (rd16(entry) != cp)
        {
            continue;
        }

        *width   = entry[2];
        *advance = entry[3];
        return &s_buf[rd32(&entry[4])];
    }
    return NULL;
}

/* 解码一个 UTF-8 字符，返回消耗的字节数（0 表示非法） */
static uint8_t utf8_next(const char **text, uint16_t *cp)
{
    const uint8_t *s = (const uint8_t *)*text;

    if (s[0] < 0x80U)
    {
        *cp = s[0];
        *text += 1;
        return 1U;
    }

    if ((s[0] & 0xE0U) == 0xC0U && (s[1] & 0xC0U) == 0x80U)
    {
        *cp = (uint16_t)(((uint16_t)(s[0] & 0x1FU) << 6) | (uint16_t)(s[1] & 0x3FU));
        *text += 2;
        return 2U;
    }

    if ((s[0] & 0xF0U) == 0xE0U && (s[1] & 0xC0U) == 0x80U && (s[2] & 0xC0U) == 0x80U)
    {
        *cp = (uint16_t)(((uint16_t)(s[0] & 0x0FU) << 12) |
                         ((uint16_t)(s[1] & 0x3FU) << 6) |
                         (uint16_t)(s[2] & 0x3FU));
        *text += 3;
        return 3U;
    }

    *cp = 0U;
    *text += 1;
    return 0U;
}

uint16_t Boot_Font_TextWidth(const char *utf8)
{
    uint16_t width = 0U;

    if (!s_ready)
    {
        return 0U;
    }

    while (*utf8 != '\0')
    {
        uint16_t cp;
        uint8_t w;
        uint8_t adv;
        const char *next = utf8;

        if (utf8_next(&next, &cp) == 0U)
        {
            utf8 = next;
            continue;
        }

        if (find_glyph(cp, &w, &adv) != NULL)
        {
            width = (uint16_t)(width + adv);
        }

        utf8 = next;
    }

    return width;
}

void Boot_Font_DrawText(uint16_t x, uint16_t y, const char *utf8, uint16_t fg, uint16_t bg)
{
    uint16_t cursor = x;

    if (!s_ready)
    {
        return;
    }

    while (*utf8 != '\0')
    {
        uint16_t cp;
        uint8_t width;
        uint8_t advance;
        const char *next = utf8;
        const uint8_t *bits;
        uint32_t stride;
        uint32_t rows;

        if (utf8_next(&next, &cp) == 0U)
        {
            utf8 = next;
            continue;
        }
        utf8 = next;

        bits = find_glyph(cp, &width, &advance);
        if (bits == NULL || width == 0U || width > FONT_MAX_W)
        {
            continue;
        }

        rows   = s_height;
        stride = ((uint32_t)width + 7U) / 8U;

        for (uint32_t r = 0U; r < rows; r++)
        {
            for (uint32_t c = 0U; c < width; c++)
            {
                uint8_t byte = bits[(r * stride) + (c >> 3)];
                bool ink = (byte & (uint8_t)(0x80U >> (c & 7U))) != 0U;
                s_pix[(r * width) + c] = ink ? fg : bg;
            }
        }

        Boot_Lcd_Blit(cursor, y, width, (uint16_t)rows, s_pix);
        cursor = (uint16_t)(cursor + advance);
    }
}
