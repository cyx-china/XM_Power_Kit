/**
  ******************************************************************************
  * @file           : PdTask.c
  * @brief          : USB PD 协议检测 + PD 诱骗（CH224A）APP
  * @date           : 2026/11/14
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * 界面分四块：
  *   · 顶栏    ：标题 + CH224A 在线状态
  *   · 协议区  ：BC1.2 / QC2 / QC3 / PD / EPR / AVS 六个指示灯（来自 0x09）
  *   · 实测区  ：INA226 测到的输入电压 / 电流 / 功率
  *   · 请求区  ：5V / 9V / 12V / 15V / 20V / 28V 六档，编码器选择、SET 下发（0x0A）
  *
  * 按键：旋转/上下 = 选择档位，SET = 请求该档位，MODE 或长按编码器 = 返回主界面。
  ******************************************************************************
  */

#include <stdio.h>
#include <string.h>

#include "cmsis_os2.h"
#include "lcd_draw_api.h"
#include "font.h"
#include "format.h"
#include "UserTask.h"
#include "os_handles.h"
#include "CH224A.h"
#include "UserDefineManage.h"

/*=============================== 配色 ===============================*/
#define PD_COL_BG        0x18C5     /* 深蓝底（与自研 GUI 一致） */
#define PD_COL_HEAD      0x2126     /* 顶栏 */
#define PD_COL_CARD      0x2124     /* 卡片 */
#define PD_COL_CHIP      0x2946     /* 未选中的档位块 */
#define PD_COL_TEXT      0xFFFF
#define PD_COL_SUB       0xAD55
#define PD_COL_DIM       0x6B4D
#define PD_COL_ACC       0x2A9F     /* 强调蓝 */
#define PD_COL_OK        0x07E0     /* 协议握手成功 */
#define PD_COL_OK_BG     0x0320
#define PD_COL_OFF       0x4208     /* 未握手 */
#define PD_COL_WARN      0xFD20

/*=============================== 布局 ===============================*/
#define PD_BADGE_X0      6U
#define PD_BADGE_STEP    52U
#define PD_BADGE_W       48U
#define PD_BADGE_Y       38U
#define PD_BADGE_H       22U

#define PD_CHIP_X0       6U
#define PD_CHIP_STEP     52U
#define PD_CHIP_W        48U
#define PD_CHIP_Y        160U
#define PD_CHIP_H        30U

#define PD_VOLT_NUM      6U
#define PD_REFRESH_LOOPS 10U        /* 20ms × 10 = 200ms 刷新一次 */
#define PD_REPROBE_MS    1000U      /* 掉线后每秒重探一次 */
#define PD_PENDING_MS    6000U      /* 请求后等协商的最长时间 */

/* 界面统一使用 pd_text（16x16，ASCII + 本 APP 用到的汉字，字库 0:/font/pd_text.bin） */
#define PD_FONT_W        16U        /* 汉字/数字的固定宽度 */

/*=============================== 静态状态 ===============================*/
typedef struct {
    const char *badge;              /* 指示灯文字 */
    uint8_t mask;                   /* 对应的 0x09 状态位 */
} PdBadge_t;

static const PdBadge_t s_badges[6] = {
    { "BC",  CH224A_ST_BC12 },
    { "QC2", CH224A_ST_QC2  },
    { "QC3", CH224A_ST_QC3  },
    { "PD",  CH224A_ST_PD   },
    { "EPR", CH224A_ST_EPR_ACTIVE },
    { "AVS", CH224A_ST_AVS_EXIST  }
};

static const CH224A_VoltTypeDef s_volts[PD_VOLT_NUM] = {
    CH224A_VOLT_5V, CH224A_VOLT_9V, CH224A_VOLT_12V,
    CH224A_VOLT_15V, CH224A_VOLT_20V, CH224A_VOLT_28V
};

static uint8_t  s_sel = 0;              /* 当前选中的档位下标 */
static uint8_t  s_online = 0;           /* CH224A 是否在线 */
static uint8_t  s_status_raw = 0;       /* 上一次的 0x09 原始值（0 = 尚未读到/未握手） */
static uint16_t s_cur_limit_ma = 0;     /* 当前档位最大电流（mA，0 = 未知） */
static uint8_t  s_pending = 0;          /* 正在等待协商 */
static uint32_t s_pending_tick = 0;
static uint32_t s_reprobe_tick = 0;

/* 影子字符串：值没变就不重画，减少刷屏命令与字模读取 */
static char s_txt_volt[12] = "", s_txt_curr[12] = "", s_txt_power[12] = "";
static char s_txt_status[48] = "";

/*=============================== 绘制 ===============================*/
static void pd_draw_badges(void) {
    for (uint8_t i = 0; i < 6; i++) {
        uint16_t x = (uint16_t) (PD_BADGE_X0 + i * PD_BADGE_STEP);
        uint8_t on = (s_online && ((s_status_raw & s_badges[i].mask) != 0U)) ? 1U : 0U;

        lcd_draw_round_rect(x, PD_BADGE_Y, (uint16_t) (x + PD_BADGE_W), (uint16_t) (PD_BADGE_Y + PD_BADGE_H),
                            6, on ? PD_COL_OK_BG : PD_COL_CARD, 1);
        lcd_draw_round_rect(x, PD_BADGE_Y, (uint16_t) (x + PD_BADGE_W), (uint16_t) (PD_BADGE_Y + PD_BADGE_H),
                            6, on ? PD_COL_OK : PD_COL_OFF, 0);

        uint8_t n = (uint8_t) strlen(s_badges[i].badge);
        uint16_t text_w = (uint16_t) (n * PD_FONT_W) - (uint16_t) (2U * (n - 1U));
        uint16_t tx = (uint16_t) (x + (PD_BADGE_W - text_w) / 2U);
        lcd_draw_string(tx, (uint16_t) (PD_BADGE_Y + 3U), s_badges[i].badge, &pd_text,
                        on ? PD_COL_OK : PD_COL_DIM, on ? PD_COL_OK_BG : PD_COL_CARD, -2);
    }
}

static void pd_draw_chip(uint8_t index) {
    uint16_t x = (uint16_t) (PD_CHIP_X0 + index * PD_CHIP_STEP);
    uint8_t selected = (index == s_sel) ? 1U : 0U;
    uint8_t dim = (index == (PD_VOLT_NUM - 1U) && !(s_status_raw & CH224A_ST_EPR_EXIST)) ? 1U : 0U;
    const char *text = CH224A_VoltText(s_volts[index]);

    lcd_draw_round_rect(x, PD_CHIP_Y, (uint16_t) (x + PD_CHIP_W), (uint16_t) (PD_CHIP_Y + PD_CHIP_H),
                        8, PD_COL_CHIP, 1);
    lcd_draw_round_rect(x, PD_CHIP_Y, (uint16_t) (x + PD_CHIP_W), (uint16_t) (PD_CHIP_Y + PD_CHIP_H),
                        8, selected ? PD_COL_ACC : PD_COL_CHIP, 0);
    if (selected) {
        lcd_draw_round_rect(x, PD_CHIP_Y, (uint16_t) (x + PD_CHIP_W), (uint16_t) (PD_CHIP_Y + PD_CHIP_H),
                            8, PD_COL_ACC, 0);
    }

    uint8_t n = (uint8_t) strlen(text);
    uint16_t text_w = (uint16_t) (n * PD_FONT_W) - (uint16_t) (2U * (n - 1U));
    uint16_t tx = (uint16_t) (x + (PD_CHIP_W - text_w) / 2U);
    uint16_t color = selected ? PD_COL_ACC : (dim ? PD_COL_DIM : PD_COL_TEXT);
    lcd_draw_string(tx, (uint16_t) (PD_CHIP_Y + 7U), text, &pd_text, color, PD_COL_CHIP, -2);
}

static void pd_draw_page(void) {
    lcd_draw_rect(0, 0, 319, 239, PD_COL_BG, 1);

    /* 顶栏 */
    lcd_draw_rect(0, 0, 319, 31, PD_COL_HEAD, 1);
    lcd_draw_string(8, 7, "USB PD 检测", &pd_text, PD_COL_TEXT, PD_COL_HEAD, 0);
    lcd_draw_string(252, 8, s_online ? "在线" : "离线", &pd_text,
                    s_online ? PD_COL_OK : PD_COL_WARN, PD_COL_HEAD, 0);

    pd_draw_badges();

    /* 实测区卡片 */
    lcd_draw_round_rect(6, 66, 313, 136, 8, PD_COL_CARD, 1);
    lcd_draw_string(16, 72, "电压", &pd_text, PD_COL_SUB, PD_COL_CARD, 0);
    lcd_draw_string(16, 96, "电流", &pd_text, PD_COL_SUB, PD_COL_CARD, 0);
    lcd_draw_string(16, 120, "功率", &pd_text, PD_COL_SUB, PD_COL_CARD, 0);

    /* 请求区 */
    lcd_draw_string(10, 142, "请求电压", &pd_text, PD_COL_SUB, PD_COL_BG, 0);
    for (uint8_t i = 0; i < PD_VOLT_NUM; i++) {
        pd_draw_chip(i);
    }

    /* 底部提示 */
    lcd_draw_string(32, 222, "SET 请求    MODE 返回", &pd_text, PD_COL_DIM, PD_COL_BG, 0);

    /* 影子串置空，强制下一次刷新全部重画 */
    s_txt_volt[0] = 0;
    s_txt_curr[0] = 0;
    s_txt_power[0] = 0;
    s_txt_status[0] = 0;
}

/* 值变化才重画，避免每 200ms 无脑刷屏 */
static void pd_draw_value(uint16_t x, uint16_t y, const char *text, char *shadow, size_t shadow_size,
                          uint16_t color, uint16_t bg) {
    if (strncmp(shadow, text, shadow_size) == 0) {
        return;
    }
    (void) snprintf(shadow, shadow_size, "%s", text);
    lcd_draw_string(x, y, text, &pd_text, color, bg, -2);
}

static void pd_draw_status(const char *text, uint16_t color) {
    if (strncmp(s_txt_status, text, sizeof(s_txt_status)) == 0) {
        return;
    }
    (void) snprintf(s_txt_status, sizeof(s_txt_status), "%s", text);
    lcd_draw_rect(6, 196, 313, 218, PD_COL_BG, 1);
    lcd_draw_string(12, 198, text, &pd_text, color, PD_COL_BG, 0);
}

/*=============================== 状态文本 ===============================*/
static void pd_update_status_text(void) {
    char buf[48];
    uint32_t now = osKernelGetTickCount();

    if (!s_online) {
        pd_draw_status("未检测到 CH224A", PD_COL_WARN);
        return;
    }

    if (s_status_raw == 0U) {
        pd_draw_status("未握手，请接快充适配器", PD_COL_WARN);
        return;
    }

    if (s_pending && ((uint32_t) (now - s_pending_tick) < PD_PENDING_MS)) {
        (void) snprintf(buf, sizeof(buf), "协商中 %s ...", CH224A_VoltText(s_volts[s_sel]));
        pd_draw_status(buf, PD_COL_ACC);
        return;
    }
    s_pending = 0;

    if (s_status_raw & CH224A_ST_PD) {
        if (s_cur_limit_ma > 0U) {
            (void) snprintf(buf, sizeof(buf), "PD 握手成功  %u.%uA", (unsigned) (s_cur_limit_ma / 1000U),
                            (unsigned) ((s_cur_limit_ma % 1000U) / 100U));
        } else {
            (void) snprintf(buf, sizeof(buf), "PD 握手成功");
        }
        pd_draw_status(buf, PD_COL_OK);
    } else if (s_status_raw & CH224A_ST_QC3) {
        pd_draw_status("QC3.0 握手成功", PD_COL_OK);
    } else if (s_status_raw & CH224A_ST_QC2) {
        pd_draw_status("QC2.0 握手成功", PD_COL_OK);
    } else {
        pd_draw_status("BC1.2 握手成功", PD_COL_OK);
    }
}

/*=============================== 刷新 ===============================*/
static void pd_refresh(void) {
    uint32_t now = osKernelGetTickCount();
    char buf[12];

    /* 掉线后每秒重新探测一次，插拔适配器/换板子都能自愈 */
    if (!s_online && (uint32_t) (now - s_reprobe_tick) >= PD_REPROBE_MS) {
        s_reprobe_tick = now;
        s_online = CH224A_Probe();
        if (s_online) {
            lcd_draw_string(252, 8, "在线", &pd_text, PD_COL_OK, PD_COL_HEAD, 0);
            s_status_raw = 0;
        }
    }

    if (s_online) {
        CH224A_Status_t st;
        if (CH224A_ReadStatus(&st) == HAL_OK) {
            if (st.raw != s_status_raw) {
                s_status_raw = st.raw;
                pd_draw_badges();
            }
        } else {
            s_online = 0;
            s_status_raw = 0;
            lcd_draw_string(252, 8, "离线", &pd_text, PD_COL_WARN, PD_COL_HEAD, 0);
            pd_draw_badges();
        }

        uint16_t ma = 0;
        if (CH224A_ReadMaxCurrentMa(&ma) == HAL_OK) {
            s_cur_limit_ma = ma;
        } else {
            s_cur_limit_ma = 0;
        }
    } else {
        s_status_raw = 0;
    }

    /* 实测值：直接取传感器任务算好的结果 */
    (void) fmt_fixed_unit(buf, sizeof(buf), Input_Voltage, 2, 0, 0, "V");
    pd_draw_value(120, 68, buf, s_txt_volt, sizeof(s_txt_volt), PD_COL_TEXT, PD_COL_CARD);

    (void) fmt_fixed_unit(buf, sizeof(buf), Input_Current, 2, 0, 0, "A");
    pd_draw_value(120, 92, buf, s_txt_curr, sizeof(s_txt_curr), PD_COL_TEXT, PD_COL_CARD);

    (void) fmt_fixed_unit(buf, sizeof(buf), Input_Power, 2, 0, 0, "W");
    pd_draw_value(120, 116, buf, s_txt_power, sizeof(s_txt_power), PD_COL_TEXT, PD_COL_CARD);

    pd_update_status_text();
}

/*=============================== 按键 ===============================*/
static void pd_go_home(void) {
    AppListType app = APP_HOME;
    StartBeezer(0);
    osMessageQueuePut(AppSwitchQueueHandle, &app, 0, 10);
}

static void pd_select(int8_t dir) {
    int8_t next = (int8_t) s_sel + dir;
    if (next < 0) {
        next = (int8_t) (PD_VOLT_NUM - 1U);
    }
    if (next >= (int8_t) PD_VOLT_NUM) {
        next = 0;
    }
    if (next == (int8_t) s_sel) {
        return;
    }
    s_sel = (uint8_t) next;
    for (uint8_t i = 0; i < PD_VOLT_NUM; i++) {
        pd_draw_chip(i);            /* 只重画这一行，改动成本很低 */
    }
    StartBeezer(0);
}

static void pd_apply(void) {
    CH224A_VoltTypeDef volt = s_volts[s_sel];

    if (!s_online) {
        pd_draw_status("未检测到 CH224A", PD_COL_WARN);
        StartBeezer(0);
        return;
    }

    if ((volt == CH224A_VOLT_28V) && ((s_status_raw & CH224A_ST_EPR_EXIST) == 0U)) {
        pd_draw_status("适配器不支持 EPR 28V", PD_COL_WARN);
        StartBeezer(0);
        return;
    }

    if (CH224A_RequestVoltage(volt) != HAL_OK) {
        pd_draw_status("请求失败，检查连接", PD_COL_WARN);
        StartBeezer(0);
        return;
    }

    s_pending = 1;
    s_pending_tick = osKernelGetTickCount();
    s_txt_status[0] = 0;            /* 强制重画状态行 */
    pd_update_status_text();
    StartBeezer(0);
}

static void pd_key_handler(KeyEventMsg_t msg) {
    if ((msg.key == KEY_DOWN) || (msg.event == ENCODER_EVENT_RIGHT) || (msg.event == ENCODER_EVENT_PRESS_RIGHT)) {
        pd_select(1);
    } else if ((msg.key == KEY_UP) || (msg.event == ENCODER_EVENT_LEFT) || (msg.event == ENCODER_EVENT_PRESS_LEFT)) {
        pd_select(-1);
    } else if (((msg.key == KEY_SET) || (msg.key == KEY_ENCODER)) && (msg.event == KEY_EVENT_CLICK)) {
        pd_apply();
    } else if ((msg.key == KEY_MODE) && (msg.event == KEY_EVENT_CLICK || msg.event == KEY_EVENT_LONG_PRESS)) {
        pd_go_home();
    } else if ((msg.key == KEY_ENCODER) && (msg.event == KEY_EVENT_LONG_PRESS)) {
        pd_go_home();
    } else {
        /* 其它事件忽略 */
    }
}

/*=============================== 任务 ===============================*/
void Start_PdTask(void *argument) {
    (void) argument;

    /* 与其它 APP 一样：入口先自挂起，由 PageSelectTask 恢复 */
    osThreadSuspend(PdTaskHandle);

    s_sel = 0;
    s_pending = 0;
    s_cur_limit_ma = 0;
    s_status_raw = 0;
    s_online = CH224A_Probe();
    s_reprobe_tick = osKernelGetTickCount();

    pd_draw_page();

    KeyEventMsg_t msg;
    uint8_t loops = 0;

    for (;;) {
        if (osMessageQueueGet(KeyEventQueueHandle, &msg, NULL, 20U) == osOK) {
            pd_key_handler(msg);
        }

        if (++loops >= PD_REFRESH_LOOPS) {
            loops = 0;
            pd_refresh();
        }
    }
}

void Suspend_PdTask(void) {
    Suspend_IndevDetectTask();
    osThreadSuspend(PdTaskHandle);
}

void Resume_PdTask(void) {
    /* 重新进 APP：重探一次芯片（拔插过适配器也能刷到新状态），整页重画 */
    s_pending = 0;
    s_cur_limit_ma = 0;
    s_online = CH224A_Probe();
    s_status_raw = 0;
    s_reprobe_tick = osKernelGetTickCount();

    pd_draw_page();

    osThreadResume(PdTaskHandle);
    Resume_IndevDetectTask();
}
