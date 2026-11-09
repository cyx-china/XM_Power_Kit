/**
 ******************************************************************************
 * @file    boot_ui.c
 * @brief   bootloader 的屏幕界面，两种布局：
 *          1) 自检/出厂初始化界面：6 行状态表 + 状态行 + 提示行
 *          2) 升级界面：升级包信息 + 进度条 + 状态/错误行
 *          文字全部走点阵字库（优先外部 Flash 的 0:/font/bootloader/boot.bin，
 *          缺失时用内嵌的那份），字库彻底不可用时只显示进度条与红色方块。
 ******************************************************************************
 */
#include "boot.h"
#include <stddef.h>

/* ---------------------------- 配色 ---------------------------- */
#define COL_BG      0x0841U
#define COL_PANEL   0x18E3U
#define COL_FRAME   0x2945U
#define COL_TEXT    0xFFFFU
#define COL_TITLE   0xFD20U
#define COL_SUB     0x8410U
#define COL_OK      0x07E0U
#define COL_BAR     0x3C1EU     /* 蓝色进度条（刻意避开青色） */
#define COL_WARN    0xF800U

/* ---------------------------- 布局 ---------------------------- */
#define PAD_X       16U
#define ROW_FOOTER  216U

/* 升级界面 */
#define ROW_VERSION     40U
#define ROW_SIZE        62U
#define ROW_CRC         84U
#define BAR_X0          16U
#define BAR_X1          264U
#define BAR_Y0          106U
#define BAR_Y1          125U
#define ROW_STATUS_OTA  140U
#define ROW_ERROR_OTA   166U

/* 自检界面 */
#define ITEM_Y0         44U
#define ITEM_DY         22U
#define ITEM_VALUE_X    108U
#define ROW_STATUS_INIT 176U
#define ROW_HINT        196U

/* ---------------------------- 文案（字库生成脚本会扫描 Bootloader 下的 .c 字符串） ---------------------------- */
const char Boot_Txt_Title[]      = "XM Power Kit 固件升级";
const char Boot_Txt_InitTitle[]  = "XM Power Kit 设备初始化";
const char Boot_Txt_Version[]    = "版本";
const char Boot_Txt_Size[]       = "大小";
const char Boot_Txt_Crc[]        = "校验";
const char Boot_Txt_Footer[]     = "请勿断电，完成后自动重启";
const char Boot_Txt_InitFooter[] = "初始化完成后自动进入系统";
const char Boot_Txt_NoFont[]     = "屏幕字库缺失";
const char Boot_Txt_Verify[]     = "正在校验升级包";
const char Boot_Txt_Erase[]      = "正在擦除内部 Flash";
const char Boot_Txt_Write[]      = "正在写入固件";
const char Boot_Txt_Check[]      = "正在校验写入结果";
const char Boot_Txt_Reboot[]     = "升级完成，正在重启";
const char Boot_Txt_Rollback[]   = "正在回滚到上一版本";
const char Boot_Txt_NoPkg[]      = "未找到升级包";
const char Boot_Txt_BadPkg[]     = "升级包校验失败";
const char Boot_Txt_WriteFail[]  = "写入失败";
const char Boot_Txt_RollbackFail[] = "回滚失败，请用 ST-Link 重新烧录";
const char Boot_Txt_Wait[]       = "等待固件包";
const char Boot_Txt_Ready[]      = "准备就绪";

const char Boot_Txt_Formatting[] = "正在格式化外部 Flash";
const char Boot_Txt_Provision[]  = "正在写入系统文件";
const char Boot_Txt_SelfTest[]   = "正在执行开机自检";
const char Boot_Txt_Formatted[]  = "外部 Flash 已格式化";
const char Boot_Txt_FormatFail[] = "格式化失败，请检查外部 Flash";
const char Boot_Txt_AllOk[]      = "自检通过";
const char Boot_Txt_SomeFail[]   = "自检未通过";
const char Boot_Txt_Stay[]       = "已停留维护界面";
const char Boot_Txt_StayHint[]   = "请用上位机或 ST-Link 处理";
const char Boot_Txt_JumpIn[]     = "秒后进入系统，按键停留";
const char Boot_Txt_Entering[]   = "正在进入系统";
const char Boot_Txt_UsbWait[]    = "维护模式：请连接上位机 USB";
const char Boot_Txt_UsbOk[]      = "上位机已连接，可升级或传文件";
const char Boot_Txt_ForceHint[]  = "一次性工具：格式化后请烧回正式固件";
const char Boot_Txt_KeyHold[]    = "按住按键：已停在维护模式";

static uint16_t s_last_percent = 0xFFFFU;
static uint16_t s_status_y     = ROW_STATUS_OTA;
static uint16_t s_error_y      = ROW_ERROR_OTA;
static bool     s_with_bar     = true;

/* ---------------------------- 数字格式化（bootloader 不链 printf） ---------------------------- */
static void fmt_u32(char *out, uint32_t value)
{
    char tmp[11];
    uint32_t n = 0U;

    if (value == 0U)
    {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value > 0U && n < sizeof(tmp))
    {
        tmp[n++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    uint32_t i = 0U;
    while (n > 0U)
    {
        out[i++] = tmp[--n];
    }
    out[i] = '\0';
}

static void fmt_percent(char *out, uint16_t percent)
{
    char digits[11];

    fmt_u32(digits, percent);
    uint32_t i = 0U;
    while (digits[i] != '\0')
    {
        out[i] = digits[i];
        i++;
    }
    out[i++] = '%';
    out[i] = '\0';
}

static void fmt_kb(char *out, uint32_t bytes)
{
    uint32_t whole = bytes / 1024U;
    uint32_t tenth = ((bytes % 1024U) * 10U) / 1024U;
    char digits[11];

    fmt_u32(digits, whole);
    uint32_t i = 0U;
    while (digits[i] != '\0')
    {
        out[i] = digits[i];
        i++;
    }
    out[i++] = '.';
    out[i++] = (char)('0' + tenth);
    out[i++] = ' ';
    out[i++] = 'K';
    out[i++] = 'B';
    out[i] = '\0';
}

static void fmt_crc(char *out, uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";

    out[0] = '0';
    out[1] = 'x';
    for (uint32_t i = 0U; i < 8U; i++)
    {
        out[2U + i] = hex[(value >> ((7U - i) * 4U)) & 0x0FU];
    }
    out[10] = '\0';
}

/* ---------------------------- 界面绘制 ---------------------------- */
static void draw_row(uint16_t y, const char *label, const char *value, uint16_t color)
{
    Boot_Ui_ClearLine(y);

    if (label != NULL)
    {
        Boot_Font_DrawText(PAD_X, y, label, COL_SUB, COL_BG);
    }
    if (value != NULL)
    {
        Boot_Font_DrawText((uint16_t)(PAD_X + 40U), y, value, color, COL_BG);
    }
}

void Boot_Ui_ClearLine(uint16_t y)
{
    Boot_Lcd_Fill(PAD_X, y, (uint16_t)(PAD_X + 296U), (uint16_t)(y + 19U), COL_BG);
}

/**
 * @brief  整屏重建：标题栏 + 底栏 + 可选进度条外框
 * @param  with_bar  true=升级界面布局 false=自检界面布局
 */
void Boot_Ui_Screen(const char *title, const char *footer, bool with_bar)
{
    Boot_Lcd_Fill(0U, 0U, 319U, 239U, COL_BG);

    /* 标题栏 */
    Boot_Lcd_Fill(0U, 0U, 319U, 29U, COL_PANEL);
    Boot_Lcd_Fill(0U, 29U, 319U, 30U, COL_FRAME);
    if (title != NULL)
    {
        Boot_Font_DrawText(10U, 7U, title, COL_TITLE, COL_PANEL);
    }

    /* 底栏 */
    if (footer != NULL)
    {
        Boot_Font_DrawText(PAD_X, ROW_FOOTER, footer, COL_SUB, COL_BG);
    }

    if (with_bar)
    {
        Boot_Lcd_Fill(BAR_X0, BAR_Y0, BAR_X1, BAR_Y1, COL_FRAME);
        Boot_Lcd_Fill((uint16_t)(BAR_X0 + 1U), (uint16_t)(BAR_Y0 + 1U),
                      (uint16_t)(BAR_X1 - 1U), (uint16_t)(BAR_Y1 - 1U), COL_PANEL);
        s_status_y = ROW_STATUS_OTA;
        s_error_y  = ROW_ERROR_OTA;
    }
    else
    {
        s_status_y = ROW_STATUS_INIT;
        s_error_y  = ROW_STATUS_INIT;
    }
    s_with_bar = with_bar;

    if (!Boot_Font_Ready())
    {
        /* 字库彻底不可用：用红色方块提示，进度条与自检行仍然可用 */
        Boot_Lcd_Fill(PAD_X, ITEM_Y0, (uint16_t)(PAD_X + 19U), (uint16_t)(ITEM_Y0 + 19U), COL_WARN);
    }

    s_last_percent = 0xFFFFU;
}

/**
 * @brief  自检表一行
 * @param  state BOOT_ITEM_PENDING/OK/FAIL，决定右边文字的颜色
 */
void Boot_Ui_Item(uint8_t index, const char *label, const char *value, uint8_t state)
{
    uint16_t y = (uint16_t)(ITEM_Y0 + ((uint16_t)index * ITEM_DY));
    uint16_t color = COL_SUB;

    if (state == BOOT_ITEM_OK)
    {
        color = COL_OK;
    }
    else if (state == BOOT_ITEM_FAIL)
    {
        color = COL_WARN;
    }

    Boot_Lcd_Fill(PAD_X, y, (uint16_t)(PAD_X + 288U), (uint16_t)(y + 19U), COL_BG);
    Boot_Font_DrawText(PAD_X, y, label, COL_TEXT, COL_BG);
    if ((value != NULL) && (value[0] != '\0'))
    {
        Boot_Font_DrawText(ITEM_VALUE_X, y, value, color, COL_BG);
    }
}

bool Boot_Ui_HasBar(void)
{
    return s_with_bar;
}

void Boot_Ui_Package(const char *version, uint32_t size, uint32_t crc)
{
    char buf[24];

    draw_row(ROW_VERSION, Boot_Txt_Version, version, COL_TEXT);

    fmt_kb(buf, size);
    draw_row(ROW_SIZE, Boot_Txt_Size, buf, COL_TEXT);

    fmt_crc(buf, crc);
    draw_row(ROW_CRC, Boot_Txt_Crc, buf, COL_TEXT);
}

void Boot_Ui_Progress(uint16_t percent)
{
    char buf[8];
    uint32_t span = (uint32_t)(BAR_X1 - BAR_X0 - 1U);

    if (percent > 100U)
    {
        percent = 100U;
    }

    if (percent == s_last_percent)
    {
        return;
    }
    s_last_percent = percent;

    Boot_Lcd_Fill((uint16_t)(BAR_X0 + 1U), (uint16_t)(BAR_Y0 + 1U),
                  (uint16_t)(BAR_X1 - 1U), (uint16_t)(BAR_Y1 - 1U), COL_PANEL);

    if (percent > 0U)
    {
        uint16_t width = (uint16_t)((span * percent) / 100U);
        if (width > 0U)
        {
            Boot_Lcd_Fill((uint16_t)(BAR_X0 + 1U), (uint16_t)(BAR_Y0 + 1U),
                          (uint16_t)(BAR_X0 + width), (uint16_t)(BAR_Y1 - 1U), COL_BAR);
        }
    }

    fmt_percent(buf, percent);
    Boot_Font_DrawText((uint16_t)(BAR_X1 + 10U), (uint16_t)(BAR_Y0 + 2U), buf, COL_TEXT, COL_BG);
}

void Boot_Ui_Status(const char *text, bool ok)
{
    Boot_Ui_ClearLine(s_status_y);
    if (text != NULL)
    {
        Boot_Font_DrawText(PAD_X, s_status_y, text, ok ? COL_TEXT : COL_WARN, COL_BG);
    }
}

void Boot_Ui_Error(const char *text)
{
    Boot_Ui_ClearLine(s_error_y);
    if ((text != NULL) && (text[0] != '\0'))
    {
        Boot_Font_DrawText(PAD_X, s_error_y, text, COL_WARN, COL_BG);
    }
}

void Boot_Ui_Hint(const char *text, bool warn)
{
    Boot_Lcd_Fill(PAD_X, ROW_HINT, (uint16_t)(PAD_X + 288U), (uint16_t)(ROW_HINT + 19U), COL_BG);
    if (text != NULL)
    {
        Boot_Font_DrawText(PAD_X, ROW_HINT, text, warn ? COL_WARN : COL_SUB, COL_BG);
    }
}
