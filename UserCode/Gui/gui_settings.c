/**
  ******************************************************************************
  * @file           : gui_settings.c
  * @brief          : 系统设置界面（自研 GUI）：分组列表 + 滑条/开关/下拉选择 + 返回
  * @date           : 2026/11/13
  ******************************************************************************
  * 交互沿用原 LVGL 手感：
  *   · 编码器旋转 / 上下键 —— 在行之间移动焦点
  *   · SET / 编码器单击    —— 滑条进入编辑态、开关直接切换、下拉项展开列表、返回按钮退出
  *   · 编辑态旋转          —— 调整数值，再按一次 SET 退出编辑
  *   · MODE                —— 退出编辑 / 保存并返回首页
  ******************************************************************************
  */

#include "gui.h"
#include "cmsis_os2.h"
#include "os_handles.h"
#include "UserTask.h"
#include "UserDefineManage.h"
#include "SwitchManager.h"
#include "ST7789.h"
#include "tim.h"
#include "wave_generator.h"

#include <stdio.h>
#include <string.h>

/*=============================== 布局参数 ===============================*/
#define SET_HEAD_H        36U
#define SET_BODY_Y0       36U
#define SET_BODY_Y1       239U
#define SET_ROW_X0        8U
#define SET_ROW_X1        311U
#define SET_ROW_W         (SET_ROW_X1 - SET_ROW_X0 + 1U)
#define SET_ROW_H         44U
#define SET_TITLE_H       26U
#define SET_ROW_GAP       4U
#define SET_TEXT_DX       14U

/*=============================== 行定义 ===============================*/
typedef enum {
    ROW_TITLE = 0,
    ROW_SLIDER,
    ROW_SWITCH,
    ROW_CHOICE,
    ROW_BUTTON
} SetRowKind_t;

typedef enum {
    SK_SLEEP = 0,
    SK_BRIGHT,
    SK_FLIP,
    SK_ROTATE,
    SK_BEEP_VOL,
    SK_BEEP_TIME,
    SK_DAC_SWAP,
    SK_FAN_EN,
    SK_FAN_TEMP,
    SK_FAN_FULL,
    SK_RST_CAL,
    SK_RST_ALL,
    SK_BACK,
    SK_NONE = 0xFF
} SetKey_t;

typedef struct {
    SetRowKind_t kind;
    SetKey_t key;
    const char *label;
    int16_t min;
    int16_t max;
} SetRow_t;

static const SetRow_t s_rows[] = {
    { ROW_TITLE,  SK_NONE,      "显示设置",     0,   0 },
    { ROW_CHOICE, SK_SLEEP,     "自动休眠",     0,   5 },
    { ROW_SLIDER, SK_BRIGHT,    "屏幕亮度",     10, 100 },
    { ROW_SWITCH, SK_FLIP,      "颜色翻转",     0,   1 },
    { ROW_SWITCH, SK_ROTATE,    "屏幕旋转",     0,   1 },
    { ROW_TITLE,  SK_NONE,      "蜂鸣器设置",   0,   0 },
    { ROW_SLIDER, SK_BEEP_VOL,  "蜂鸣音量",     0,  100 },
    { ROW_SLIDER, SK_BEEP_TIME, "蜂鸣时长",     1,   8 },
    { ROW_TITLE,  SK_NONE,      "信号源设置",   0,   0 },
    { ROW_SWITCH, SK_DAC_SWAP,  "DAC 输出交换", 0,   1 },
    { ROW_TITLE,  SK_NONE,      "风扇设置",     0,   0 },
    { ROW_SWITCH, SK_FAN_EN,    "风扇使能",     0,   1 },
    { ROW_SLIDER, SK_FAN_TEMP,  "启转温度",     20,  50 },
    { ROW_SLIDER, SK_FAN_FULL,  "满速温度",     45,  80 },
    { ROW_TITLE,  SK_NONE,      "恢复设置",     0,   0 },
    { ROW_BUTTON, SK_RST_CAL,   "恢复默认校准", 0,   0 },
    { ROW_BUTTON, SK_RST_ALL,   "恢复全部参数", 0,   0 },
    { ROW_BUTTON, SK_BACK,      "返回",         0,   0 }
};

#define SET_ROW_NUM   ((uint8_t) (sizeof(s_rows) / sizeof(s_rows[0])))

/* 休眠时间档位（秒），与原实现一致 */
static const uint16_t s_sleep_seconds[6] = { 0, 60, 300, 600, 1800, 3600 };
static const char *const s_sleep_labels[6] = { "关闭", "1分钟", "5分钟", "10分钟", "30分钟", "1小时" };

/*=============================== 状态 ===============================*/
static uint8_t s_focus = 1;        // 当前焦点行
static bool s_edit = false;        // 是否处于编辑态
static uint16_t s_offset = 0;      // 列表滚动偏移（内容坐标）
static bool s_popup = false;       // 下拉选择面板是否展开
static uint8_t s_popup_sel = 0;    // 面板里选中的项
static uint8_t s_confirm = 0;      // 确认框：0 关 / 1 恢复默认校准 / 2 恢复全部参数
static uint8_t s_confirm_sel = 1;  // 确认框选择：0 取消 / 1 确定

/* 保存去抖：改完参数等 600ms 没人再动才落盘，避免转编码器时反复擦写文件 */
#define SET_SAVE_DELAY_MS   600U
#define SET_SAVE_RETRY_MS   3000U
#define SET_TOAST_MS        1200U

static bool s_dirty = false;            // 有未落盘的改动
static uint32_t s_save_due = 0;         // 到点保存（系统节拍）
static bool s_toast_on = false;         // 提示条是否显示
static uint32_t s_toast_until = 0;      // 提示条消失时刻
static const char *s_toast_text = "";   // 提示文本（只指向常量字符串）
static uint16_t s_toast_color = GUI_COL_TEXT;

static void set_draw_all(void);
static void set_draw_body(void);
static void set_draw_confirm(void);
static void set_draw_toast(void);
static void set_repaint(void);
static void set_mark_dirty(void);
static void set_toast_show(const char *text, uint16_t color);
static void set_apply_hardware(void);

/*=============================== 工具函数 ===============================*/
static uint16_t set_row_top(uint8_t index) {
    uint16_t top = 0;
    uint8_t i;

    for (i = 0; i < index; i++) {
        top = (uint16_t) (top + ((s_rows[i].kind == ROW_TITLE) ? SET_TITLE_H : SET_ROW_H) + SET_ROW_GAP);
    }
    return top;
}

static uint16_t set_row_height(uint8_t index) {
    return (s_rows[index].kind == ROW_TITLE) ? SET_TITLE_H : SET_ROW_H;
}

static uint16_t set_content_height(void) {
    uint16_t total = 0;
    uint8_t i;

    for (i = 0; i < SET_ROW_NUM; i++) {
        total = (uint16_t) (total + set_row_height(i) + SET_ROW_GAP);
    }
    return total;
}

static bool set_row_visible(uint8_t index) {
    int32_t y = (int32_t) SET_BODY_Y0 + (int32_t) set_row_top(index) - (int32_t) s_offset;
    int32_t bottom = y + (int32_t) set_row_height(index);

    return (bottom > (int32_t) SET_BODY_Y0) && (y < (int32_t) (SET_BODY_Y1 + 1U));
}

/* 读取某行的当前值 */
static int16_t set_get(uint8_t index) {
    int16_t value = 0;
    uint8_t i;

    switch (s_rows[index].key) {
        case SK_SLEEP:
            value = 0;
            for (i = 0; i < 6; i++) {
                if (UserParam.Screen_Sleeptime == s_sleep_seconds[i]) {
                    value = (int16_t) i;
                    break;
                }
            }
            break;
        case SK_BRIGHT:    value = (int16_t) UserParam.Screen_Brightness;     break;
        case SK_FLIP:      value = (UserParam.Screen_ShowFlip == 0U) ? 1 : 0; break;  // 开关 ON = 反色开启
        case SK_BEEP_VOL:  value = (int16_t) UserParam.Beezer_Volume;         break;
        case SK_BEEP_TIME: value = (int16_t) UserParam.Beezer_Time;           break;
        case SK_FAN_EN:    value = (int16_t) UserParam.Fan_Enable;            break;
        case SK_FAN_TEMP:  value = (int16_t) UserParam.Fan_StartTemperature;  break;
        case SK_FAN_FULL:  value = (int16_t) UserParam.Fan_FullTemperature;   break;
        case SK_ROTATE:    value = (int16_t) (UserParam.Screen_Rotate180 & 0x01U); break;
        case SK_DAC_SWAP:  value = (int16_t) (UserParam.DDS_SwapDac & 0x01U);  break;
        default:           value = 0;                                        break;
    }

    if (value < s_rows[index].min) {
        value = s_rows[index].min;
    }
    if (value > s_rows[index].max) {
        value = s_rows[index].max;
    }
    return value;
}

/* 写入某行的值，并立刻作用到硬件 / 参数 */
static void set_apply(uint8_t index, int16_t value) {
    if (value < s_rows[index].min) {
        value = s_rows[index].min;
    }
    if (value > s_rows[index].max) {
        value = s_rows[index].max;
    }

    switch (s_rows[index].key) {
        case SK_SLEEP:
            UserParam.Screen_Sleeptime = s_sleep_seconds[value];
            IsSleeping = false;
            Sensor_ResetIdleTimer();        // 从"现在"重新计时
            if (UserParam.Screen_Sleeptime == 0U) {
                PWM_SetBrightness((uint8_t) UserParam.Screen_Brightness);
            }
            break;
        case SK_BRIGHT:
            UserParam.Screen_Brightness = (uint16_t) value;
            PWM_SetBrightness((uint8_t) value);
            break;
        case SK_FLIP:
            UserParam.Screen_ShowFlip = (value != 0) ? 0U : 1U;   // 0 = 反色（默认）
            if (value != 0) {
                LCD_WR_CMD(0x21);           // 开启颜色翻转
            } else {
                LCD_WR_CMD(0x20);           // 关闭颜色翻转
            }
            break;
        case SK_BEEP_VOL:
            UserParam.Beezer_Volume = (uint16_t) value;
            if (value > 0) {
                __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, (uint32_t) ((value - 1) / 2));
                StartBeezer(0);             // 试听一下新音量
            } else {
                HAL_TIM_PWM_Stop(&htim9, TIM_CHANNEL_1);   // 音量为 0：直接静音
            }
            break;
        case SK_BEEP_TIME:
            UserParam.Beezer_Time = (uint16_t) value;
            break;
        case SK_DAC_SWAP:
            // 山寨 MCU 会把两个 DAC 通道对调，打开后波形改走 DAC2、偏置走 DAC1
            if ((uint16_t) value != UserParam.DDS_SwapDac) {
                UserParam.DDS_SwapDac = (uint16_t) value;
                WaveGen_ApplyDacSwap();     // 正在输出时立刻换通道
            }
            break;
        case SK_FAN_EN:
            UserParam.Fan_Enable = (uint16_t) value;
            break;
        case SK_FAN_TEMP:
            UserParam.Fan_StartTemperature = (uint16_t) value;
            break;
        case SK_FAN_FULL:
            UserParam.Fan_FullTemperature = (uint16_t) value;
            break;
        case SK_ROTATE:
            UserParam.Screen_Rotate180 = (uint16_t) value;
            LCD_SetRotation((uint8_t) (value & 0x01));   // 只改地址映射，接着必须整屏重绘
            Gui_Redraw();
            break;
        default:
            break;
    }
    set_mark_dirty();
}

/* 行内显示值的文本 */
static void set_value_text(uint8_t index, char *buf, uint16_t size) {
    int16_t value = set_get(index);

    switch (s_rows[index].key) {
        case SK_SLEEP:
            (void) snprintf(buf, size, "%s", s_sleep_labels[value]);
            break;
        case SK_BRIGHT:
            (void) snprintf(buf, size, "%d%%", (int) value);
            break;
        case SK_BEEP_VOL:
            if (value == 0) {
                (void) snprintf(buf, size, "%s", "关闭");
            } else {
                (void) snprintf(buf, size, "%d%%", (int) value);
            }
            break;
        case SK_BEEP_TIME:
            (void) snprintf(buf, size, "%dms", (int) value * 10);
            break;
        case SK_FAN_TEMP:
            (void) snprintf(buf, size, "%d℃", (int) value);
            break;
        case SK_FAN_FULL:
            (void) snprintf(buf, size, "%d℃", (int) value);
            break;
        default:
            buf[0] = 0;
            break;
    }
}

/*=============================== 绘制 ===============================*/
static void set_draw_row(uint8_t index) {
    const SetRow_t *row = &s_rows[index];
    int32_t y32 = (int32_t) SET_BODY_Y0 + (int32_t) set_row_top(index) - (int32_t) s_offset;
    uint16_t y;
    uint16_t card_bg;
    bool fits;

    if (!set_row_visible(index)) {
        return;
    }
    if (y32 < (int32_t) SET_BODY_Y0) {          // 顶部被窗口裁掉的行：只画露出来的卡片残块
        uint16_t clip_end = (uint16_t) (y32 + SET_ROW_H - 1);

        if (clip_end > 239U) {
            clip_end = 239U;
        }
        Gui_Fill(SET_ROW_X0, SET_BODY_Y0, SET_ROW_X1, clip_end, GUI_COL_CARD);
        return;
    }
    y = (uint16_t) y32;
    card_bg = (s_edit && index == s_focus) ? GUI_RGB( 30,  43,  77) : GUI_COL_CARD;
    fits = ((uint16_t) (y + SET_ROW_H)) <= 240U;

    if (row->kind == ROW_TITLE) {
        Gui_Text(SET_ROW_X0 + 6U, (uint16_t) (y + 4U), row->label, &ui_text, GUI_COL_TEXT_DIM, GUI_COL_BG);
        return;
    }

    /* 卡片底：最后一行可能被窗口底部裁掉一部分，只画可见部分 */
    {
        uint16_t y_end = (uint16_t) (y + SET_ROW_H - 1U);

        if (y_end > 239U) {
            y_end = 239U;
        }
        /* 整行（含焦点框外扩的 2px）先刷底色，避免上一行焦点框留下残影 */
        Gui_Fill((uint16_t) (SET_ROW_X0 - 2U), (uint16_t) (y - 2U),
                 (uint16_t) (SET_ROW_X1 + 2U), (uint16_t) (y_end + 2U), GUI_COL_BG);
        Gui_RoundFill(SET_ROW_X0, y, SET_ROW_X1, y_end, fits ? 7U : 3U, card_bg);
    }

    if ((uint16_t) (y + 26U) <= 239U) {         // 被窗口底部裁掉大半的行不画文字
        Gui_Text((uint16_t) (SET_ROW_X0 + SET_TEXT_DX), (uint16_t) (y + 6U), row->label, &ui_text,
                 GUI_COL_TEXT, card_bg);
    }

    /* 右侧内容（裁剪行里超出可见区的部分不画，避免溢出屏外） */
    switch (row->kind) {
        case ROW_SLIDER: {
            int16_t v = set_get(index);
            uint8_t percent = (uint8_t) (((int32_t) (v - row->min) * 100) /
                                         (int32_t) (row->max - row->min));
            char value_text[12];

            set_value_text(index, value_text, sizeof(value_text));
            if ((uint16_t) (y + 26U) <= 239U) {
                Gui_TextRight(SET_ROW_X1 - SET_TEXT_DX, (uint16_t) (y + 6U), value_text, &ui_text,
                              GUI_COL_ACCENT, card_bg);
            }
            if ((uint16_t) (y + 38U) <= 239U) {
                Gui_Slider((uint16_t) (SET_ROW_X0 + SET_TEXT_DX), (uint16_t) (y + 30U),
                           (uint16_t) (SET_ROW_W - 2U * SET_TEXT_DX), 8U, percent,
                           GUI_COL_ACCENT_DARK, GUI_COL_ACCENT);
            }
            break;
        }
        case ROW_SWITCH:
            if ((uint16_t) (y + 31U) <= 239U) {
                Gui_Switch((uint16_t) (SET_ROW_X1 - SET_TEXT_DX - 44U), (uint16_t) (y + 13U), 44U, 18U,
                           set_get(index) != 0);
            }
            break;
        case ROW_CHOICE: {
            char value_text[12];

            set_value_text(index, value_text, sizeof(value_text));
            if ((uint16_t) (y + 33U) <= 239U) {
                Gui_TextRight(SET_ROW_X1 - SET_TEXT_DX, (uint16_t) (y + 13U), value_text, &ui_text,
                              GUI_COL_ACCENT, card_bg);
            }
            break;
        }
        case ROW_BUTTON:
            if ((uint16_t) (y + 33U) <= 239U) {
                Gui_TextRight(SET_ROW_X1 - SET_TEXT_DX, (uint16_t) (y + 13U),
                              (row->key == SK_BACK) ? "确定" : "执行", &ui_text, GUI_COL_ACCENT, card_bg);
            }
            break;
        default:
            break;
    }

    if (index == s_focus) {
        uint16_t y_end = (uint16_t) (y + SET_ROW_H - 1U);

        if (y_end > 239U) {
            y_end = 239U;
        }
        Gui_RoundFrame((uint16_t) (SET_ROW_X0 - 2U), (uint16_t) (y - 2U),
                       (uint16_t) (SET_ROW_X1 + 2U), y_end, 9, GUI_COL_ACCENT, 2);
    }
}

static void set_draw_body(void) {
    uint8_t i;

    Gui_Fill(0, SET_BODY_Y0, 319, SET_BODY_Y1, GUI_COL_BG);
    for (i = 0; i < SET_ROW_NUM; i++) {
        if (set_row_visible(i)) {
            set_draw_row(i);
        }
    }
}

static void set_draw_popup(void) {
    const uint16_t x0 = 78, x1 = 242, y0 = 46, item_h = 24, pad = 10;
    uint16_t y = (uint16_t) (y0 + pad);
    uint8_t i;
    uint16_t panel_h = (uint16_t) (pad * 2U + item_h * 6U);

    Gui_RoundFill(x0, y0, x1, (uint16_t) (y0 + panel_h), 10, GUI_COL_CARD);
    Gui_RoundFrame(x0, y0, x1, (uint16_t) (y0 + panel_h), 10, GUI_COL_ACCENT, 2);

    for (i = 0; i < 6; i++) {
        if (i == s_popup_sel) {
            Gui_RoundFill((uint16_t) (x0 + 6U), y, (uint16_t) (x1 - 6U), (uint16_t) (y + item_h - 2U), 6,
                          GUI_COL_ACCENT_DARK);
        }
        Gui_TextCenter((uint16_t) ((x0 + x1) / 2U), (uint16_t) (y + 4U), s_sleep_labels[i], &ui_text,
                       (i == s_popup_sel) ? GUI_COL_TEXT : GUI_COL_TEXT_SUB,
                       (i == s_popup_sel) ? GUI_COL_ACCENT_DARK : GUI_COL_CARD);
        y = (uint16_t) (y + item_h);
    }
}

/*=============================== 提示条 ===============================*/
static void set_draw_toast(void) {
    const uint16_t x0 = 76, y0 = 194, x1 = 244, y1 = 222;

    if (!s_toast_on) {
        return;
    }
    Gui_RoundFill(x0, y0, x1, y1, 8, GUI_COL_CARD);
    Gui_RoundFrame(x0, y0, x1, y1, 8, GUI_COL_ACCENT, 1);
    Gui_TextCenter(160, (uint16_t) (y0 + 4U), s_toast_text, &ui_text, s_toast_color, GUI_COL_CARD);
}

static void set_repaint(void) {
    set_draw_body();
    set_draw_toast();
}

/*=============================== 确认框 ===============================*/
static void set_draw_confirm(void) {
    const uint16_t x0 = 30, x1 = 289, y0 = 66, y1 = 174;
    const uint16_t btn_y0 = 132, btn_y1 = 162;

    Gui_RoundFill(x0, y0, x1, y1, 10, GUI_COL_CARD);
    Gui_RoundFrame(x0, y0, x1, y1, 10, GUI_COL_ACCENT, 2);
    Gui_TextCenter(160, 82, (s_confirm == 1U) ? "恢复默认校准？" : "恢复全部参数？", &ui_text,
                   GUI_COL_TEXT, GUI_COL_CARD);
    Gui_TextCenter(160, 108, (s_confirm == 1U) ? "电源 / 示波器 / 万用表 / 信号源"
                                               : "含屏幕、蜂鸣器、风扇设置", &ui_text,
                   GUI_COL_TEXT_SUB, GUI_COL_CARD);

    Gui_RoundFill(46, btn_y0, 148, btn_y1, 8,
                  (s_confirm_sel == 0U) ? GUI_COL_ACCENT : GUI_COL_ACCENT_DARK);
    Gui_TextCenter(97, (uint16_t) (btn_y0 + 6U), "取消", &ui_text, GUI_COL_TEXT,
                   (s_confirm_sel == 0U) ? GUI_COL_ACCENT : GUI_COL_ACCENT_DARK);

    Gui_RoundFill(172, btn_y0, 274, btn_y1, 8,
                  (s_confirm_sel == 1U) ? GUI_COL_ACCENT : GUI_COL_ACCENT_DARK);
    Gui_TextCenter(223, (uint16_t) (btn_y0 + 6U), "确定", &ui_text, GUI_COL_TEXT,
                   (s_confirm_sel == 1U) ? GUI_COL_ACCENT : GUI_COL_ACCENT_DARK);
}

/* 把参数里的硬件相关项重新作用一遍（恢复默认后调用） */
static void set_apply_hardware(void) {
    uint8_t brightness = (uint8_t) UserParam.Screen_Brightness;
    uint16_t volume = UserParam.Beezer_Volume;

    if ((brightness < 10U) || (brightness > 100U)) {
        brightness = 100U;
    }
    PWM_SetBrightness(brightness);
    LCD_WR_CMD((UserParam.Screen_ShowFlip == 0U) ? 0x21 : 0x20);
    LCD_SetRotation((uint8_t) (UserParam.Screen_Rotate180 & 0x01U));

    if ((volume < 1U) || (volume > 100U)) {
        volume = 100U;
    }
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, (uint32_t) ((volume - 1U) / 2U));

    Sensor_ResetIdleTimer();
}

static void set_confirm_apply(void) {
    uint8_t type = s_confirm;
    HAL_StatusTypeDef status;

    s_confirm = 0;
    status = (type == 1U) ? UserParam_ResetCalibrationDefaults() : UserParam_ResetAllDefaults();
    if (status != HAL_OK) {
        set_toast_show("保存失败", GUI_RGB(248, 113, 113));
    } else {
        set_apply_hardware();
        s_dirty = false;
        set_toast_show("已恢复默认", GUI_COL_TEXT);
    }
    set_draw_all();
}

static void set_confirm_key(GuiKey_t key) {
    switch (key) {
        case GUI_KEY_LEFT:
        case GUI_KEY_UP:
            s_confirm_sel = 0;
            set_draw_confirm();
            break;
        case GUI_KEY_RIGHT:
        case GUI_KEY_DOWN:
            s_confirm_sel = 1;
            set_draw_confirm();
            break;
        case GUI_KEY_ENTER:
            if (s_confirm_sel == 1U) {
                set_confirm_apply();
            } else {
                s_confirm = 0;
                set_repaint();
            }
            break;
        case GUI_KEY_BACK:
        default:
            s_confirm = 0;
            set_repaint();
            break;
    }
}

/*=============================== 保存去抖 ===============================*/
static void set_toast_show(const char *text, uint16_t color) {
    s_toast_text = text;
    s_toast_color = color;
    s_toast_on = true;
    s_toast_until = osKernelGetTickCount() + SET_TOAST_MS;
}

static void set_mark_dirty(void) {
    s_dirty = true;
    s_save_due = osKernelGetTickCount() + SET_SAVE_DELAY_MS;
}

static void set_save_pending(void) {
    HAL_StatusTypeDef status;

    if (!s_dirty) {
        return;
    }
    status = UserParam_SaveAllValues();
    if (status == HAL_OK) {
        s_dirty = false;
        set_toast_show("已保存", GUI_COL_TEXT);
    } else {                                    // 落盘失败：红字提示，过几秒再试
        s_save_due = osKernelGetTickCount() + SET_SAVE_RETRY_MS;
        set_toast_show("保存失败", GUI_RGB(248, 113, 113));
    }
    set_repaint();
}

static void set_draw_all(void) {
    Gui_Fill(0, 0, 319, SET_HEAD_H - 1U, GUI_COL_BG);
    Gui_TextCenter(160, 8, "系 统 设 置", &ui_text, GUI_COL_TITLE, GUI_COL_BG);
    Gui_Line(0, SET_HEAD_H - 1U, 319, SET_HEAD_H - 1U, GUI_COL_CARD);
    set_draw_body();
    set_draw_toast();
}

/*=============================== 滚动 ===============================*/
/* 把偏移对齐到行首；末尾不足一页时直接停到最大偏移，保证最后一行完整可见 */
static uint16_t set_align_offset(uint16_t desired) {
    uint16_t view_h = (uint16_t) (SET_BODY_Y1 - SET_BODY_Y0 + 1U);
    uint16_t content = set_content_height();
    uint8_t i;

    if (content <= view_h) {
        return 0;
    }
    if (desired > (uint16_t) (content - view_h)) {
        desired = (uint16_t) (content - view_h);
    }
    for (i = 0; i < SET_ROW_NUM; i++) {                     // 不小于 desired 的最小行首
        uint16_t aligned = set_row_top(i);

        if (aligned >= desired) {
            if ((uint32_t) aligned + view_h <= (uint32_t) content) {
                return aligned;
            }
            break;                                          // 该行首放不下整屏，退回最大偏移
        }
    }
    return desired;
}

/* 让焦点行完整可见，返回偏移是否变化 */
static bool set_ensure_visible(uint8_t index) {
    uint16_t top = set_row_top(index);
    uint16_t bottom = (uint16_t) (top + set_row_height(index));
    uint16_t view_h = (uint16_t) (SET_BODY_Y1 - SET_BODY_Y0 + 1U);
    uint16_t old = s_offset;
    uint16_t desired = s_offset;

    if (top < (uint16_t) (s_offset + 4U)) {
        desired = (top > 4U) ? (uint16_t) (top - 4U) : 0U;
    } else if ((uint16_t) (bottom + 4U) > (uint16_t) (s_offset + view_h)) {
        desired = (uint16_t) (bottom + 4U - view_h);
    }
    s_offset = set_align_offset(desired);
    return old != s_offset;
}

/*=============================== 交互 ===============================*/
static uint8_t set_next_focus(uint8_t from, int8_t dir) {
    int16_t index = (int16_t) from;

    do {
        index = (int16_t) (index + dir);
        if (index < 0) {
            index = (int16_t) (SET_ROW_NUM - 1U);
        } else if (index >= (int16_t) SET_ROW_NUM) {
            index = 0;
        }
    } while (s_rows[index].kind == ROW_TITLE);

    return (uint8_t) index;
}

static void set_focus_move(int8_t dir) {
    uint8_t old = s_focus;

    if (s_edit) {
        return;
    }
    s_focus = set_next_focus(s_focus, dir);
    if (set_ensure_visible(s_focus) || s_toast_on) {
        set_repaint();
    } else {
        set_draw_row(old);
        set_draw_row(s_focus);
    }
}

static void set_step(int8_t dir) {
    int16_t value = set_get(s_focus);

    if (s_rows[s_focus].kind != ROW_SLIDER && s_rows[s_focus].kind != ROW_SWITCH) {
        return;
    }
    value = (int16_t) (value + dir);
    if (value < s_rows[s_focus].min) {
        value = s_rows[s_focus].min;
    }
    if (value > s_rows[s_focus].max) {
        value = s_rows[s_focus].max;
    }
    if (value == set_get(s_focus)) {
        return;
    }
    set_apply(s_focus, value);
    if (s_toast_on) {
        set_repaint();
    } else {
        set_draw_row(s_focus);
    }
}

static void set_enter(void) {
    switch (s_rows[s_focus].kind) {
        case ROW_SLIDER:
            s_edit = true;
            set_draw_row(s_focus);
            break;
        case ROW_SWITCH:
            set_apply(s_focus, (set_get(s_focus) != 0) ? 0 : 1);
            set_draw_row(s_focus);
            break;
        case ROW_CHOICE:
            s_popup = true;
            s_popup_sel = (uint8_t) set_get(s_focus);
            set_draw_popup();
            break;
        case ROW_BUTTON:
            if (s_rows[s_focus].key == SK_BACK) {
                Gui_Back();
            } else {
                s_confirm = (s_rows[s_focus].key == SK_RST_CAL) ? 1U : 2U;
                s_confirm_sel = 1;          // 默认停在"确定"上，转一格就能取消
                set_draw_confirm();
            }
            break;
        default:
            break;
    }
}

static void set_popup_key(GuiKey_t key) {
    switch (key) {
        case GUI_KEY_UP:
        case GUI_KEY_LEFT:
            s_popup_sel = (uint8_t) ((s_popup_sel + 5U) % 6U);
            set_draw_popup();
            break;
        case GUI_KEY_DOWN:
        case GUI_KEY_RIGHT:
            s_popup_sel = (uint8_t) ((s_popup_sel + 1U) % 6U);
            set_draw_popup();
            break;
        case GUI_KEY_ENTER:
            s_popup = false;
            set_apply(s_focus, (int16_t) s_popup_sel);
            set_draw_all();
            break;
        case GUI_KEY_BACK:
        default:
            s_popup = false;
            set_draw_all();
            break;
    }
}

static void set_on_key(GuiKey_t key) {
    if (s_confirm != 0U) {
        set_confirm_key(key);
        return;
    }

    if (s_popup) {
        set_popup_key(key);
        return;
    }

    if (s_edit) {
        switch (key) {
            case GUI_KEY_LEFT:
            case GUI_KEY_UP:
                set_step(-1);
                break;
            case GUI_KEY_RIGHT:
            case GUI_KEY_DOWN:
                set_step(1);
                break;
            case GUI_KEY_ENTER:
            case GUI_KEY_BACK:
                s_edit = false;
                set_draw_row(s_focus);
                break;
            default:
                break;
        }
        return;
    }

    switch (key) {
        case GUI_KEY_UP:
        case GUI_KEY_LEFT:
            set_focus_move(-1);
            break;
        case GUI_KEY_DOWN:
        case GUI_KEY_RIGHT:
            set_focus_move(1);
            break;
        case GUI_KEY_ENTER:
            set_enter();
            break;
        case GUI_KEY_BACK:
            Gui_Back();
            break;
        default:
            break;
    }
}

static void set_on_enter(void) {
    s_focus = 1;
    s_edit = false;
    s_offset = 0;
    s_popup = false;
    s_confirm = 0;
    s_dirty = false;
    s_toast_on = false;
    set_ensure_visible(s_focus);
    set_draw_all();
}

static void set_on_exit(void) {
    s_popup = false;
    s_confirm = 0;
    s_edit = false;
    s_toast_on = false;
    if (s_dirty) {
        (void) UserParam_SaveAllValues();       // 离开时把没落盘的改动立刻写掉
        s_dirty = false;
    }
}

/* 周期回调：保存去抖到期就落盘，提示条到期就擦掉 */
static void set_on_tick(uint32_t now_ms) {
    if (s_dirty && ((int32_t) (now_ms - s_save_due) >= 0)) {
        set_save_pending();
    }
    if (s_toast_on && ((int32_t) (now_ms - s_toast_until) >= 0)) {
        s_toast_on = false;
        set_repaint();
    }
}

const GuiScreen gui_screen_settings = {
    .on_enter = set_on_enter,
    .on_exit  = set_on_exit,
    .on_key   = set_on_key,
    .on_tick  = set_on_tick,
    .on_draw  = set_draw_all
};
