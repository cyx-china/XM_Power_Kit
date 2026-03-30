/**
******************************************************************************
  * @file           : CalibrateTask.c
  * @brief          : 校准任务的核心程序，只实现主界面，并分发到各个校准模块。
  * @date           : 2026/3/14
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  *
  * This AWG core task module is independently developed by the author.
  * It is released under the CC-BY-NC-SA 4.0 open source license.
  * And the author's right of attribution is reserved.
  ******************************************************************************
  */


#include <inttypes.h>
#include <stdio.h>

#include "lcd_draw_api.h"
#include "UserTask.h"
#include "os_handles.h"
#include "PID.h"
#include "SwitchManager.h"

#include "DPS_Calibrate.h"
#include "DSO_Calibrate.h"
#include "AWG_Calibrate.h"
#include "DMM_V_Calibrate.h"
#include "DMM_A_Calibrate.h"
#include "DMM_R_Calibrate.h"

// 外部函数声明
void CB_DPS_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);
void DPS_ADC_DeInit(void);
void DPS_ADC_Init(void);

static void DrawCalMainBasicElement(void);
void cal_main_page_handler(KeyEventMsg_t msg);

// 校准页面枚举
typedef enum {
    PAGE_MAIN = 0,      // 主界面（默认页面）
    PAGE_DPS_CAL,       // 数控电源校准
    PAGE_DSO_CAL,       // 示波器校准
    PAGE_AWG_CAL,       // 波形发生器校准
    PAGE_DMM_V_CAL,     // 万用表电压档校准
    PAGE_DMM_A_CAL,     // 万用表电流档校准
    PAGE_DMM_R_CAL,     // 万用表电阻档校准
    PAGE_NUM            // 页面总数
} CAL_AppPage_t;

// 当前激活页面
CAL_AppPage_t CAL_current_page = PAGE_MAIN;
CAL_AppPage_t CAL_previous_page = PAGE_MAIN;

/* ========================== 页面处理函数回调表 ========================== */
static void (*page_handlers[PAGE_NUM])(KeyEventMsg_t) = {
    [PAGE_MAIN]      = cal_main_page_handler,
    [PAGE_DPS_CAL]   = dps_cal_handler,
    [PAGE_DSO_CAL]   = dso_cal_handler,
    [PAGE_AWG_CAL]   = awg_cal_handler,
    [PAGE_DMM_V_CAL] = dmm_v_cal_handler,
    [PAGE_DMM_A_CAL] = dmm_a_cal_handler,
    [PAGE_DMM_R_CAL] = dmm_r_cal_handler,
};

/* ========================== 主界面光标 ========================== */
typedef enum {
    Main_DPS = 0,
    Main_DSO,
    Main_AWG,
    Main_Voltage,
    Main_Current,
    Main_Resistance,
} MainPageCursor_e;

MainPageCursor_e CAL_main_page_cursor_current = Main_DPS;
MainPageCursor_e CAL_main_page_cursor_past = Main_DPS;

// 卡片参数表
typedef struct {
    uint16_t x1; uint16_t y1;
    uint16_t x2; uint16_t y2;
    uint16_t charx; uint16_t chary;
    char* name;
} CalItem_t;

static const CalItem_t CalItems[6] = {
    {10,40,154,96,36,59,"电源校准"},
    {165,40,309,96,179,59,"示波器校准"},
    {10,107,154,163,24,126,"信号源校准"},
    {165,107,309,163,179,126,"万用表电压"},
    {10,174,154,230,24,193,"万用表电流"},
    {165,174,309,230,179,193,"万用表电阻"}
};

#define CardFtColor             0x1908
#define CardOutlineColor        0x21aa
#define CardOutlineSelectColor  0xe22c
#define CardTextFtColor         0xef7d
#define CardTextBgColor         0x1908
#define CardTextIntervalSize    4

static void DrawCalMainBasicElement(void) {
    lcd_draw_rect(0, 0, 319, 31, 0x1908, 1);
    lcd_draw_line(0, 31, 319, 31, 0x11ac);
    lcd_draw_string(120,7,"设备校准",&yahei18x18,0x24be,0x1908,2);
    lcd_draw_rect(0, 32, 319, 239, 0x18c6, 1);
    for (uint8_t i = 0;i < 6;i ++) {
        lcd_draw_round_rect(CalItems[i].x1,CalItems[i].y1,CalItems[i].x2,CalItems[i].y2,10,CardFtColor,1);
        lcd_draw_round_rect(CalItems[i].x1,CalItems[i].y1,CalItems[i].x2,CalItems[i].y2,10,CardOutlineColor,0);
        lcd_draw_string(CalItems[i].charx, CalItems[i].chary, CalItems[i].name, &yahei20x20,CardTextFtColor,
                        CardTextBgColor,CardTextIntervalSize);
    }
    lcd_draw_round_rect(CalItems[0].x1,CalItems[0].y1,CalItems[0].x2,CalItems[0].y2,10,CardOutlineSelectColor,0);
}

void cal_main_page_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT || msg.event == ENCODER_EVENT_PRESS_LEFT) {
        CAL_main_page_cursor_past = CAL_main_page_cursor_current;
        CAL_main_page_cursor_current = (CAL_main_page_cursor_current > Main_DPS)
                                       ? (CAL_main_page_cursor_current - 1)
                                       : Main_Resistance;
    }
    else if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT || msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        CAL_main_page_cursor_past = CAL_main_page_cursor_current;
        CAL_main_page_cursor_current = (CAL_main_page_cursor_current < Main_Resistance)
                                       ? (CAL_main_page_cursor_current + 1)
                                       : Main_DPS;
    }
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        CAL_previous_page = CAL_current_page;
        CAL_current_page = (CAL_AppPage_t)(CAL_main_page_cursor_current) + 1;
        switch (CAL_main_page_cursor_current) {
            case Main_DPS:          DPS_Calibrate_Enter(); break;
            case Main_DSO:          DSO_Calibrate_Enter(); break;
            case Main_AWG:          AWG_Calibrate_Enter(); break;
            case Main_Voltage:      DMM_V_Calibrate_Enter(); break;
            case Main_Current:      DMM_A_Calibrate_Enter(); break;
            case Main_Resistance:   DMM_R_Calibrate_Enter(); break;
        }
        StartBeezer(0);
        return;
    }
    else if (msg.key == KEY_ENCODER && msg.event == KEY_EVENT_LONG_PRESS) {
        AppListType APP = APP_LVGL;
        StartBeezer(0);
        osMessageQueuePut(AppSwitchQueueHandle, &APP, 0, 10);
        return;
    }
    else return;

    StartBeezer(0);
    lcd_draw_round_rect(CalItems[CAL_main_page_cursor_past].x1, CalItems[CAL_main_page_cursor_past].y1,
                        CalItems[CAL_main_page_cursor_past].x2, CalItems[CAL_main_page_cursor_past].y2, 10,
                        CardOutlineColor, 0);
    lcd_draw_round_rect(CalItems[CAL_main_page_cursor_current].x1, CalItems[CAL_main_page_cursor_current].y1,
                        CalItems[CAL_main_page_cursor_current].x2, CalItems[CAL_main_page_cursor_current].y2, 10,
                        CardOutlineSelectColor, 0);
}

/* ========================== 任务函数 ========================== */
uint8_t cal_time_count = 0;
KeyEventMsg_t CAL_Keymsg;

void Start_CalibrateTask(void *argument) {
    osThreadSuspend(CalibrateTaskHandle);

    for (;;) {
        cal_time_count++;
        if (osMessageQueueGet(KeyEventQueueHandle, &CAL_Keymsg, NULL, 0) == osOK) {
            page_handlers[CAL_current_page](CAL_Keymsg);
        }
        if (cal_time_count > 20) {
            cal_time_count = 0;
            if (CAL_current_page == PAGE_DPS_CAL) {
                DPS_Calibrate_Refresh();
            }
        }
        osDelay(10);
    }
}


/* ========================== 任务进入/退出函数 ========================== */
// 回到校准选择主页面，并重置状态
void Calibrate_ReturnToMain(void) {
    CAL_current_page = PAGE_MAIN;
    CAL_previous_page = PAGE_AWG_CAL;
    CAL_main_page_cursor_current = Main_DPS;
    CAL_main_page_cursor_past = Main_DPS;
    DrawCalMainBasicElement();
    Resume_IndevDetectTask();
}

// 挂起本任务
void Suspend_CalibrateTask(void) {
    Suspend_IndevDetectTask();                  // 挂起输入检查任务
    osThreadSuspend(CalibrateTaskHandle);       // 挂起本任务线程
}

// 恢复本任务
void Resume_CalibrateTask(void) {
    DrawCalMainBasicElement();                     // 绘制基础见面
    osDelay(150);                          // 等待界面绘制完成
    osThreadResume(CalibrateTaskHandle);        // 恢复本任务线程
    Resume_IndevDetectTask();                   // 恢复输入检查任务
}

