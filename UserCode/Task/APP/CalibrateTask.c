#include <inttypes.h>
#include <stdio.h>
#include <tgmath.h>

#include "lcd_draw_api.h"
#include "UserTask.h"
#include "os_handles.h"
#include "PID.h"
#include "SwitchManager.h"
#include "tim.h"
#include "UserDefineManage.h"

void CB_DPS_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);
void DPS_ADC_DeInit(void);
void DPS_ADC_Init(void);

static void DrawMainBasicElement(void);
void cal_main_page_handler(KeyEventMsg_t msg);
void dps_cal_handler(KeyEventMsg_t msg);
void dso_cal_handler(KeyEventMsg_t msg);
void awg_cal_handler(KeyEventMsg_t msg);
void dmm_v_cal_handler(KeyEventMsg_t msg);
void dmm_a_cal_handler(KeyEventMsg_t msg);
void dmm_r_cal_handler(KeyEventMsg_t msg);

void DrawDpsCalibratePage(void);

extern volatile uint16_t g_adc_raw_buf[128];
extern volatile float Voltage;                  // 实时输出电压（单位：V）
extern volatile float Current;                  // 实时输出电流（单位：A）
extern volatile bool IsPowerOn;

/**
 * @brief 校准页面枚举
 * @note  定义校准系统功能页面
 */
typedef enum {
    PAGE_MAIN = 0,      // 主界面（默认页面）
    PAGE_DPS_CAL,       // 数控电源校准
    PAGE_DSO_CAL,       // 示波器校准
    PAGE_AWG_CAL,       // 波形发生器校准
    PAGE_DMM_V_CAL,     // 万用表电压档校准
    PAGE_DMM_A_CAL,     // 万用表电流档校准
    PAGE_DMM_R_CAL,     // 万用表电阻档校准
    PAGE_NUM            // 页面总数
} AWG_AppPage_t;

AWG_AppPage_t CAL_current_page = PAGE_MAIN;     // 当前激活页面
AWG_AppPage_t CAL_previous_page = PAGE_MAIN;    // 上一个页面（用于返回）


/* ========================== 页面处理函数回调表 ========================== */
/**
 * @brief 页面处理函数表驱动
 * @note  索引对应AppPage_t枚举，快速映射页面与处理函数
 */
static void (*page_handlers[PAGE_NUM])(KeyEventMsg_t) = {
    [PAGE_MAIN] = cal_main_page_handler ,
    [PAGE_DPS_CAL] = dps_cal_handler,
    [PAGE_DSO_CAL] = dso_cal_handler,
    [PAGE_AWG_CAL] = awg_cal_handler,
    [PAGE_DMM_V_CAL] = dmm_v_cal_handler,
    [PAGE_DMM_A_CAL] = dmm_a_cal_handler,
    [PAGE_DMM_R_CAL] = dmm_r_cal_handler,
};

/**
 * @brief 主界面光标枚举
 * @note  主界面可聚焦的操作项
 */
typedef enum {
    Main_DPS = 0,   // 电源
    Main_DSO,       // 示波器
    Main_AWG,       // 信号源
    Main_Voltage,   // 万用表电压
    Main_Current,   // 万用表电流
    Main_Resistance,// 万用表电阻
} MainPageCursor_e;

MainPageCursor_e CAL_main_page_cursor_current = Main_DPS;   // 主界面当前光标位置
MainPageCursor_e CAL_main_page_cursor_past = Main_DPS;      // 主界面上一次光标位置

char Current_Voltage_str[7];
char Current_Current_str[7];

/* ========================== 页面刷新函数回调表 ========================== */
//数控电源部分
void Refresh_DpsCal_Origin_Voltage(void);
void Refresh_DpsCal_Origin_Current(void);
void Refresh_DpsCal_Display_Voltage(void);
// 数控电源校准子页面响应函数枚举
typedef enum DpsCalPage{
    DpsCal_Origin_Voltage_Main = 0,         // 原点校准_电压部分_主界面
    DpsCal_Origin_Voltage_Edit,             // 原点校准_电压部分_编辑界面
    DpsCal_Origin_Current_Main,             // 原点校准_电流部分_主界面
    DpsCal_Origin_Current_Edit,             // 原点校准_电流部分_编辑界面

    DpsCal_Display_Voltage_Main,            // 显示校准_电压部分_主界面
    DpsCal_Display_Voltage_Edit,            // 显示校准_电压部分_编辑界面
    DpsCal_Display_Current_Main,            // 显示校准_电流部分_主界面
    DpsCal_Display_Current_Edit,            // 显示校准_电流部分_编辑界面

    DpsCal_Output_Main,                     // 输出校准_主界面

    DpsCal_Page_Num                         // 页面总数
}DpsCalPage_e;

DpsCalPage_e DpsCal_Page_Current = DpsCal_Origin_Voltage_Main;              // 当前的页面响应函数

void DoNothing(void){return;}

static void (*page_refresh[DpsCal_Page_Num])(void) = {
    [DpsCal_Origin_Voltage_Main] = Refresh_DpsCal_Origin_Voltage,
    [DpsCal_Origin_Voltage_Edit] = Refresh_DpsCal_Origin_Voltage,
    [DpsCal_Origin_Current_Main] = Refresh_DpsCal_Origin_Current,
    [DpsCal_Origin_Current_Edit] = Refresh_DpsCal_Origin_Current,

    [DpsCal_Display_Voltage_Main] = Refresh_DpsCal_Display_Voltage,
    [DpsCal_Display_Voltage_Edit] = Refresh_DpsCal_Display_Voltage,
    [DpsCal_Display_Current_Main] = Refresh_DpsCal_Origin_Current,
    [DpsCal_Display_Current_Edit] = Refresh_DpsCal_Origin_Current,

    [DpsCal_Output_Main] = DoNothing,

};


/* ========================== UI常量定义 ========================== */
#define SELECT_COLOR          0x07E0      // 编辑框选中色
#define NORMAL_COLOR          0x632C      // 普通框颜色
#define PROMPT_BG             0x29a7
#define PROMPT_BORDER         0x632C
#define PROMPT_TEXT_NORMAL    0xdf3c
#define PROMPT_TEXT_ERROR     0xF800
#define STEP_ACTIVE_BG        0x048A
#define STEP_INACTIVE_BG      0x632C
#define ORIGIN_STEP_TEXT      0xFFFF
#define DISPLAY_OUTPUT_TEXT   0xDEFB

// 编辑框 & 下一步按钮坐标（所有DPS子页面完全共用）
#define EDIT_RECT_X1  15
#define EDIT_RECT_Y1  200
#define EDIT_RECT_X2  212
#define EDIT_RECT_Y2  231
#define NEXT_RECT_X1  220
#define NEXT_RECT_Y1  200
#define NEXT_RECT_X2  305
#define NEXT_RECT_Y2  231


/* ========================== 公共辅助函数 ========================== */
// 更新提示条（所有页面共用同一坐标/风格）
static void DpsCal_UpdatePrompt(const char* text, uint16_t text_color) {
    lcd_draw_round_rect(15,158,305,188,8,PROMPT_BG,1);
    lcd_draw_round_rect(15,158,305,188,8,PROMPT_BORDER,0);
    lcd_draw_string(25,165,text, &yahei16x16,text_color,PROMPT_BG,1);
}

// 更新顶部步骤条（0=原点激活，1=显示激活，2=输出激活）
static void DpsCal_UpdateStepBar(uint8_t active_step) {
    uint16_t origin_bg  = (active_step == 0) ? STEP_ACTIVE_BG : STEP_INACTIVE_BG;
    uint16_t display_bg = (active_step == 1) ? STEP_ACTIVE_BG : STEP_INACTIVE_BG;
    uint16_t output_bg  = (active_step == 2) ? STEP_ACTIVE_BG : STEP_INACTIVE_BG;

    lcd_draw_round_rect(18,47,108,73,8,origin_bg,1);
    lcd_draw_string(28, 52, "原点校准", &yahei16x16, ORIGIN_STEP_TEXT, origin_bg, 2);

    lcd_draw_round_rect(115,47,205,73,8,display_bg,1);
    lcd_draw_string(125, 52, "显示校准", &yahei16x16, DISPLAY_OUTPUT_TEXT, display_bg, 2);

    lcd_draw_round_rect(212,47,302,73,8,output_bg,1);
    lcd_draw_string(222, 52, "输出校准", &yahei16x16, DISPLAY_OUTPUT_TEXT, output_bg, 2);
}

// 通用光标切换
static void DpsCal_UpdateCursor(uint8_t *cursor, KeyEventMsg_t msg) {
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT) {
        *cursor = (*cursor < 1) ? (*cursor + 1) : 0;
    } else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT) {
        *cursor = (*cursor > 0) ? (*cursor - 1) : 1;
    }
}

// 通用选择框绘制（编辑框 vs 下一步按钮）
static void DpsCal_DrawSelectionBox(uint8_t cursor) {
    if (cursor == 0) {
        lcd_draw_round_rect(EDIT_RECT_X1,EDIT_RECT_Y1,EDIT_RECT_X2,EDIT_RECT_Y2,8,SELECT_COLOR,0);
        lcd_draw_round_rect(NEXT_RECT_X1,NEXT_RECT_Y1,NEXT_RECT_X2,NEXT_RECT_Y2,8,NORMAL_COLOR,0);
    } else {
        lcd_draw_round_rect(EDIT_RECT_X1,EDIT_RECT_Y1,EDIT_RECT_X2,EDIT_RECT_Y2,8,NORMAL_COLOR,0);
        lcd_draw_round_rect(NEXT_RECT_X1,NEXT_RECT_Y1,NEXT_RECT_X2,NEXT_RECT_Y2,8,SELECT_COLOR,0);
    }
}

// 编辑模式下划线（is_display=true 表示显示校准页面）
static void DpsCal_DrawEditUnderline(bool is_display) {
    uint16_t x1 = is_display ? 110 : 162;
    uint16_t x2 = is_display ? 186 : 191;
    lcd_draw_line(x1,225,x2,225,0x87d3);
}
static void DpsCal_ClearEditUnderline(bool is_display) {
    uint16_t x1 = is_display ? 110 : 162;
    uint16_t x2 = is_display ? 186 : 191;
    lcd_draw_line(x1,225,x2,225,0x18c6);
}

// 输出校准阶段枚举
typedef enum {
    OUTPUT_CAL_START = 0,
    OUTPUT_CAL_SAVE
} OutputCalPhase_e;

static OutputCalPhase_e output_cal_phase = OUTPUT_CAL_START;


/* ========================== 状态变量 ========================== */
uint8_t DpsCal_OriginVolCursor = 0;     // 原点电压主界面光标
uint8_t DpsCal_OriginCurCursor = 0;     // 原点电流主界面光标
uint8_t DpsCal_DisplayVolCursor = 0;    // 显示电压主界面光标
uint8_t DpsCal_DisplayCurCursor = 0;    // 显示电流主界面光标

char DpsVoltageOriginalMsg[6] = {0};
char DpsCurrentOriginalMsg[6] = {0};
char DpsVoltageDisplayMsg[6] = {0};
char DpsCurrentDisplayMsg[6] = {0};


/* ========================== 任务函数 ========================== */
uint8_t time_count = 0;
KeyEventMsg_t CAL_Keymsg;                       // 按键事件消息缓存

void Start_CalibrateTask(void *argument) {
    osThreadSuspend(CalibrateTaskHandle);       // 任务自挂启

    for (;;) {
        time_count++;
        // 接收按键事件
        if (osMessageQueueGet(KeyEventQueueHandle, &CAL_Keymsg, NULL, 0) == osOK) {
            page_handlers[CAL_current_page](CAL_Keymsg);
        }

        // 下面是刷新任务,200ms刷新一次
        if (time_count > 20) {
            time_count = 0;

            if (CAL_current_page == PAGE_DPS_CAL) {
                page_refresh[DpsCal_Page_Current]();
            }
        }

        osDelay(10);
    }
}

/* ========================== 校准主界面 ========================== */

typedef struct {
    uint16_t x1;uint16_t y1;            // 卡片左上角坐标
    uint16_t x2;uint16_t y2;            // 卡片右下角坐标
    uint16_t charx;uint16_t chary;      // 卡片文字左上角坐标
    char* name;
} CalItem_t;

// 卡片参数表
static const CalItem_t CalItems[6] = {
    {10,40,154,96,36,59,"电源校准"},
    {165,40,309,96,179,59,"示波器校准"},
    {10,107,154,163,24,126,"信号源校准"},
    {165,107,309,163,179,126,"万用表电压"},
    {10,174,154,230,24,193,"万用表电流"},
    {165,174,309,230,179,193,"万用表电阻"}
};

#define CardFtColor             0x1908      // 卡片底色
#define CardOutlineColor        0x21aa      // 卡片描边色
#define CardOutlineSelectColor  0xe22c      // 卡片选中描边色
#define CardTextFtColor         0xef7d      // 卡片文字颜色
#define CardTextBgColor         0x1908      // 卡片文字背景色
#define CardTextIntervalSize    4           // 卡片间隔大小

// 绘制基础界面
static void DrawMainBasicElement(void) {
    // 顶栏
    lcd_draw_rect(0, 0, 319, 31, 0x1908, 1);                        // 底色
    lcd_draw_line(0, 31, 319, 31, 0x11ac);                                 // 分割线
    lcd_draw_string(120,7,"设备校准",&yahei18x18,0x24be,0x1908,2);
    // 卡片栏
    lcd_draw_rect(0, 32, 319, 239, 0x18c6, 1);                      // 底色
    // 绘制6个卡片
    for (uint8_t i = 0;i < 6;i ++) {
        lcd_draw_round_rect(CalItems[i].x1,CalItems[i].y1,CalItems[i].x2,CalItems[i].y2,10,CardFtColor,1);
        lcd_draw_round_rect(CalItems[i].x1,CalItems[i].y1,CalItems[i].x2,CalItems[i].y2,10,CardOutlineColor,0);
        lcd_draw_string(CalItems[i].charx, CalItems[i].chary, CalItems[i].name, &yahei20x20,CardTextFtColor,
                        CardTextBgColor,CardTextIntervalSize);
    }
    // 默认选中第一个卡片
    lcd_draw_round_rect(CalItems[0].x1,CalItems[0].y1,CalItems[0].x2,CalItems[0].y2,10,CardOutlineSelectColor,0);

}

void cal_main_page_handler(KeyEventMsg_t msg) {
    // 向上/左切换（对应原逻辑：DPS→Resistance→Current→Voltage→AWG→DSO→DPS）
    if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT || msg.event == ENCODER_EVENT_PRESS_LEFT) {
        CAL_main_page_cursor_past = CAL_main_page_cursor_current;
        CAL_main_page_cursor_current = (CAL_main_page_cursor_current > Main_DPS)
                                       ? (CAL_main_page_cursor_current - 1)
                                       : Main_Resistance;
    }
    // 向下/右切换（对应原逻辑：DPS→DSO→AWG→Voltage→Current→Resistance→DPS）
    else if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT || msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        CAL_main_page_cursor_past = CAL_main_page_cursor_current;
        CAL_main_page_cursor_current = (CAL_main_page_cursor_current < Main_Resistance)
                                       ? (CAL_main_page_cursor_current + 1)
                                       : Main_DPS;
    }
    // 短按SET/编码器：进入选中项的编辑页面
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        CAL_previous_page = CAL_current_page;
        CAL_current_page = (AWG_AppPage_t)(CAL_main_page_cursor_current) + 1;
        switch (CAL_main_page_cursor_current) {
            case Main_DPS:   // 电源校准
                DrawDpsCalibratePage();     // 绘制电源校准页面
                DpsPower_OFF();             // 关闭电源输出
                DpsRelease_ON();            // 开启泄放电阻
                DPS_ADC_Init();             // 初始化ADC
                HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_COMPLETE_CB_ID, CB_DPS_ADC_ConvCpltCallback);   // 注册回调函数
                HAL_ADC_Start_DMA(&hadc1, (uint32_t *) g_adc_raw_buf, 128);     // 启动ADC + DMA
                DpsTim_ON();                                                           // 启动定时器（ADC触发源）

                PID_Init();                                                             // 初始化PID控制器
                osThreadResume(PIDTaskHandle);              // 恢复PID运算任务

                break;
        }


        StartBeezer(0);
        return;
    }
    // 长按编码器：切换到LVGL APP
    else if (msg.key == KEY_ENCODER && msg.event == KEY_EVENT_LONG_PRESS) {
        AppListType APP = APP_LVGL;
        StartBeezer(0);
        osMessageQueuePut(AppSwitchQueueHandle, &APP, 0, 10);
        return;
    }
    // 无匹配按键事件：直接返回
    else return;

    // 更新显示
    StartBeezer(0);
    // 更新显示
    // 清空上一个选项框
    lcd_draw_round_rect(CalItems[CAL_main_page_cursor_past].x1, CalItems[CAL_main_page_cursor_past].y1,
                        CalItems[CAL_main_page_cursor_past].x2, CalItems[CAL_main_page_cursor_past].y2, 10,
                        CardOutlineColor, 0);

    // 绘制当前选项框
    lcd_draw_round_rect(CalItems[CAL_main_page_cursor_current].x1, CalItems[CAL_main_page_cursor_current].y1,
                        CalItems[CAL_main_page_cursor_current].x2, CalItems[CAL_main_page_cursor_current].y2, 10,
                        CardOutlineSelectColor, 0);

}



/* ========================== 电源校准界面 ========================== */

void DpsCal_Origin_Voltage_Main_handler(KeyEventMsg_t msg);
void DpsCal_Origin_Voltage_Edit_handler(KeyEventMsg_t msg);
void DpsCal_Origin_Current_Main_handler(KeyEventMsg_t msg);
void DpsCal_Origin_Current_Edit_handler(KeyEventMsg_t msg);

void DpsCal_Display_Voltage_Main_handler(KeyEventMsg_t msg);
void DpsCal_Display_Voltage_Edit_handler(KeyEventMsg_t msg);
void DpsCal_Display_Current_Main_handler(KeyEventMsg_t msg);
void DpsCal_Display_Current_Edit_handler(KeyEventMsg_t msg);
void DpsCal_Output_Main_handler(KeyEventMsg_t msg);

static void (*DpsCalPage_handlers[DpsCal_Page_Num])(KeyEventMsg_t) = {      // 响应函数映射表
    [DpsCal_Origin_Voltage_Main] = DpsCal_Origin_Voltage_Main_handler ,
    [DpsCal_Origin_Voltage_Edit] = DpsCal_Origin_Voltage_Edit_handler,
    [DpsCal_Origin_Current_Main] = DpsCal_Origin_Current_Main_handler ,
    [DpsCal_Origin_Current_Edit] = DpsCal_Origin_Current_Edit_handler,

    [DpsCal_Display_Voltage_Main] = DpsCal_Display_Voltage_Main_handler ,
    [DpsCal_Display_Voltage_Edit] = DpsCal_Display_Voltage_Edit_handler,
    [DpsCal_Display_Current_Main] = DpsCal_Display_Current_Main_handler ,
    [DpsCal_Display_Current_Edit] = DpsCal_Display_Current_Edit_handler,

    [DpsCal_Output_Main] = DpsCal_Output_Main_handler ,

};

int DpsCal_CalculateLinearCoeff(float data[10], float *A, float *B);

/*---------- 原点校准部分 ----------*/
// 原点校准基础界面绘制
void DrawDpsCalibratePage(void) {
    //绘制顶部状态栏
    lcd_draw_rect(0, 0, 319, 31, 0x1908, 1);
    lcd_draw_line(0, 31, 319 - 1, 31, 0x11ac);
    lcd_draw_string(93, 6, "数控电源校准", &yahei20x20, 0x24be, 0x1908, 3);

    // 绘制主背景
    lcd_draw_rect(0, 32, 319, 239, 0x18c6, 1);

    // 绘制步骤条（使用辅助函数）
    DpsCal_UpdateStepBar(0);

    // 绘制数据显示区
    lcd_draw_round_rect(15,82,305,146,10,0x0000,1); // 底色
    lcd_draw_round_rect(15,82,305,146,10,0x632C,0); // 1px边框
    lcd_draw_string(58,90,"+0.000", &DIN_Medium32x48,0xeb0c,0x0000,2);  // 初始数值
    lcd_draw_string(256,118,"V", &KaiTi16x20,0xeb0c,0x0000,0);  // 单位

    // 操作提示容器（使用辅助函数）
    DpsCal_UpdatePrompt("请移除负载，使数值接近0。", PROMPT_TEXT_NORMAL);

    // 用户输入区
    lcd_draw_round_rect(EDIT_RECT_X1,EDIT_RECT_Y1,EDIT_RECT_X2,EDIT_RECT_Y2,8,SELECT_COLOR,0); // 默认选上
    lcd_draw_string(25,208,"电压原点:", &yahei16x16,0xFFFF,0x18c6,1);

    snprintf(DpsVoltageOriginalMsg, sizeof(DpsVoltageOriginalMsg), "%+03" PRId16, UserParam.DPS_Voltage_Original);
    lcd_draw_string(142,207,DpsVoltageOriginalMsg, &yahei18x18,0xFFFF,0x18c6,2);

    // 下一步按钮
    lcd_draw_round_rect(NEXT_RECT_X1,NEXT_RECT_Y1,NEXT_RECT_X2,NEXT_RECT_Y2,8,NORMAL_COLOR,1);
    lcd_draw_string(235,207,"下一步", &yahei18x18,0xFFFF,0x632C,1);

}

void DpsCal_Origin_Voltage_Main_handler(KeyEventMsg_t msg) {
    // 导航切换（使用公共函数）
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT ||
        msg.key == KEY_UP   || msg.event == ENCODER_EVENT_LEFT) {
        DpsCal_UpdateCursor(&DpsCal_OriginVolCursor, msg);
        DpsCal_DrawSelectionBox(DpsCal_OriginVolCursor);
        StartBeezer(0);
        return;   // 原逻辑中导航会走到外面绘制，但此处提前返回保持与原蜂鸣器/绘制时机一致
    }
    // 单击
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (DpsCal_OriginVolCursor == 0) {                       // 选中编辑框
            DpsCal_Page_Current = DpsCal_Origin_Voltage_Edit;
            DpsCal_DrawEditUnderline(false);                     // 原点类型
            // 注意：此处不鸣笛（与原代码一致），外面会执行一次
        }
        else if (DpsCal_OriginVolCursor == 1) {                  // 下一步 → 电流原点
            DpsCal_Page_Current = DpsCal_Origin_Current_Main;

            DpsCal_UpdatePrompt("连接万用表，使数值等于万用表值", PROMPT_TEXT_NORMAL);
            DpsCal_UpdateStepBar(1);   // 显示校准激活

            lcd_draw_round_rect(EDIT_RECT_X1,EDIT_RECT_Y1,EDIT_RECT_X2,EDIT_RECT_Y2,8,0x18c6,1);
            lcd_draw_round_rect(EDIT_RECT_X1,EDIT_RECT_Y1,EDIT_RECT_X2,EDIT_RECT_Y2,8,SELECT_COLOR,0);
            lcd_draw_round_rect(NEXT_RECT_X1,NEXT_RECT_Y1,NEXT_RECT_X2,NEXT_RECT_Y2,8,NORMAL_COLOR,0);

            lcd_draw_string(256,118,"A", &KaiTi16x20,0x5cbd,0x0000,0);

            lcd_draw_string(25,208,"电流原点:", &yahei16x16,0xFFFF,0x18c6,1);
            snprintf(DpsCurrentOriginalMsg, sizeof(DpsCurrentOriginalMsg), "%+03" PRId16, UserParam.DPS_Current_Original);
            lcd_draw_string(142,207,DpsCurrentOriginalMsg, &yahei18x18,0xFFFF,0x18c6,2);

            StartBeezer(0);
            return;
        }
    }
    else {return;}

    StartBeezer(0);
    DpsCal_DrawSelectionBox(DpsCal_OriginVolCursor);   // 统一绘制（原代码逻辑保留）
}

// 电源校准_原点校准_电压部分_电压编辑响应逻辑
void DpsCal_Origin_Voltage_Edit_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT) {
        UserParam.DPS_Voltage_Original += 1;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DPS_Voltage_Original -= 1;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        DpsCal_Page_Current = DpsCal_Origin_Voltage_Main;
        DpsCal_ClearEditUnderline(false);
    }
    else {return;}

    StartBeezer(0);
    snprintf(DpsVoltageOriginalMsg, sizeof(DpsVoltageOriginalMsg), "%+03" PRId16, UserParam.DPS_Voltage_Original);
    lcd_draw_string(142,207,DpsVoltageOriginalMsg, &yahei18x18,0xFFFF,0x18c6,2);
}


void DpsCal_Origin_Current_Main_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT ||
        msg.key == KEY_UP   || msg.event == ENCODER_EVENT_LEFT) {
        DpsCal_UpdateCursor(&DpsCal_OriginCurCursor, msg);
        DpsCal_DrawSelectionBox(DpsCal_OriginCurCursor);
        StartBeezer(0);
        return;
    }
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (DpsCal_OriginCurCursor == 0) {
            DpsCal_Page_Current = DpsCal_Origin_Current_Edit;
            DpsCal_DrawEditUnderline(false);
        }
        else if (DpsCal_OriginCurCursor == 1) {
            DpsCal_Page_Current = DpsCal_Display_Voltage_Main;

            DpsCal_UpdatePrompt("连接万用表，使数值等于万用表值", PROMPT_TEXT_NORMAL);

            lcd_draw_round_rect(EDIT_RECT_X1,EDIT_RECT_Y1,EDIT_RECT_X2,EDIT_RECT_Y2,8,0x18c6,1);
            lcd_draw_round_rect(EDIT_RECT_X1,EDIT_RECT_Y1,EDIT_RECT_X2,EDIT_RECT_Y2,8,SELECT_COLOR,0);
            lcd_draw_round_rect(NEXT_RECT_X1,NEXT_RECT_Y1,NEXT_RECT_X2,NEXT_RECT_Y2,8,NORMAL_COLOR,0);

            lcd_draw_string(256,118,"V", &KaiTi16x20,0xeb0c,0x0000,0);

            lcd_draw_string(25,208,"电压系数:", &yahei16x16,0xFFFF,0x18c6,1);
            snprintf(DpsVoltageDisplayMsg, sizeof(DpsVoltageDisplayMsg), "%05.2f", UserParam.DPS_Voltage_Factor);
            lcd_draw_string(110,207,DpsVoltageDisplayMsg, &yahei18x18,0xFFFF,0x18c6,0);

            DpsCal_UpdateStepBar(1);

            SetTargetVoltage(15.00f);
            IsPowerOn = true;
            DpsPower_ON();
            osDelay(10);
            DpsRelease_OFF();
            DpsGnd_ON();
            DpsDc_ON();

            StartBeezer(0);
            return;
        }
    }
    else {return;}

    StartBeezer(0);
    DpsCal_DrawSelectionBox(DpsCal_OriginCurCursor);
}

void DpsCal_Origin_Current_Edit_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT) {
        UserParam.DPS_Current_Original += 1;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DPS_Current_Original -= 1;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        DpsCal_Page_Current = DpsCal_Origin_Current_Main;
        DpsCal_ClearEditUnderline(false);
    }
    else {return;}

    StartBeezer(0);
    snprintf(DpsCurrentOriginalMsg, sizeof(DpsCurrentOriginalMsg), "%+03" PRId16, UserParam.DPS_Current_Original);
    lcd_draw_string(142,207,DpsCurrentOriginalMsg, &yahei18x18,0xFFFF,0x18c6,2);
}


void DpsCal_Display_Voltage_Main_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT ||
        msg.key == KEY_UP   || msg.event == ENCODER_EVENT_LEFT) {
        DpsCal_UpdateCursor(&DpsCal_DisplayVolCursor, msg);
        DpsCal_DrawSelectionBox(DpsCal_DisplayVolCursor);
        StartBeezer(0);
        return;
    }
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (DpsCal_DisplayVolCursor == 0) {
            DpsCal_Page_Current = DpsCal_Display_Voltage_Edit;
            DpsCal_DrawEditUnderline(true);
        }
        else if (DpsCal_DisplayVolCursor == 1) {
            DpsCal_Page_Current = DpsCal_Display_Current_Main;

            DpsCal_UpdatePrompt("输出3.5V，请接负载。这个基本不用调", PROMPT_TEXT_NORMAL);

            lcd_draw_round_rect(EDIT_RECT_X1,EDIT_RECT_Y1,EDIT_RECT_X2,EDIT_RECT_Y2,8,SELECT_COLOR,0);
            lcd_draw_round_rect(NEXT_RECT_X1,NEXT_RECT_Y1,NEXT_RECT_X2,NEXT_RECT_Y2,8,NORMAL_COLOR,0);

            lcd_draw_string(256,118,"A", &KaiTi16x20,0x5cbd,0x0000,0);

            lcd_draw_string(25,208,"电流系数:", &yahei16x16,0xFFFF,0x18c6,1);
            snprintf(DpsCurrentDisplayMsg, sizeof(DpsCurrentDisplayMsg), "%05.2f", UserParam.DPS_Current_Factor);
            lcd_draw_string(110,207,DpsCurrentDisplayMsg, &yahei18x18,0xFFFF,0x18c6,0);

            SetTargetVoltage(3.5f);

            StartBeezer(0);
            return;
        }
    }
    else {return;}

    StartBeezer(0);
    DpsCal_DrawSelectionBox(DpsCal_DisplayVolCursor);
}

void DpsCal_Display_Voltage_Edit_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT) {
        UserParam.DPS_Voltage_Factor += 0.01f;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DPS_Voltage_Factor -= 0.01f;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        DpsCal_Page_Current = DpsCal_Display_Voltage_Main;
        DpsCal_ClearEditUnderline(true);
    }
    else {return;}

    StartBeezer(0);
    snprintf(DpsVoltageDisplayMsg, sizeof(DpsVoltageDisplayMsg), "%05.2f", UserParam.DPS_Voltage_Factor);
    lcd_draw_string(110,207,DpsVoltageDisplayMsg, &yahei18x18,0xFFFF,0x18c6,0);
}


void DpsCal_Display_Current_Main_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT ||
        msg.key == KEY_UP   || msg.event == ENCODER_EVENT_LEFT) {
        DpsCal_UpdateCursor(&DpsCal_DisplayCurCursor, msg);
        DpsCal_DrawSelectionBox(DpsCal_DisplayCurCursor);
        StartBeezer(0);
        return;
    }
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (DpsCal_DisplayCurCursor == 0) {
            DpsCal_Page_Current = DpsCal_Display_Current_Edit;
            DpsCal_DrawEditUnderline(true);
            StartBeezer(0);
            return;
        }
        else if (DpsCal_DisplayCurCursor == 1) {
            DpsCal_Page_Current = DpsCal_Output_Main;

            DpsCal_UpdatePrompt("请移除负载，按下按钮后自动校准", PROMPT_TEXT_NORMAL);

            lcd_draw_round_rect(EDIT_RECT_X1,EDIT_RECT_Y1,EDIT_RECT_X2,EDIT_RECT_Y2,8,0x18c6,1);

            lcd_draw_string(256,118,"V", &KaiTi16x20,0xeb0c,0x0000,0);
            Refresh_DpsCal_Origin_Voltage();

            DpsCal_UpdateStepBar(2);

            IsPowerOn = false;
            DpsPower_OFF();
            DpsRelease_ON();
            DpsDc_OFF();
            DpsGnd_OFF();

            SetTargetVoltage(2.0f);

            StartBeezer(0);
            return;
        }
    }

    StartBeezer(0);
    DpsCal_DrawSelectionBox(DpsCal_DisplayCurCursor);
}

void DpsCal_Display_Current_Edit_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT) {
        UserParam.DPS_Current_Factor += 0.01f;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DPS_Current_Factor -= 0.01f;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        DpsCal_Page_Current = DpsCal_Display_Current_Main;
        DpsCal_ClearEditUnderline(true);
        StartBeezer(0);
        return;
    }
    else {return;}

    StartBeezer(0);
    snprintf(DpsCurrentDisplayMsg, sizeof(DpsCurrentDisplayMsg), "%05.2f", UserParam.DPS_Current_Factor);
    lcd_draw_string(110,207,DpsCurrentDisplayMsg, &yahei18x18,0xFFFF,0x18c6,0);
}


float DpsCal_OutputCal_Data[18] = {0};

// 电源校准_输出校准_主界面响应逻辑
void DpsCal_Output_Main_handler(KeyEventMsg_t msg) {
    if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        Suspend_IndevDetectTask();
        StartBeezer(0);

        if (output_cal_phase == OUTPUT_CAL_START) {
            DpsCal_UpdatePrompt("开始自动校准，请耐心等待", PROMPT_TEXT_NORMAL);
            osDelay(2000);

            // 校准点1
            MCP4725_WriteFast(&hmcp4725_DC, 3600, MCP4725_MODE_NORMAL);
            DpsPower_ON();
            DpsRelease_OFF();
            DpsCal_UpdatePrompt("校准点1，等待稳定中", PROMPT_TEXT_NORMAL);
            osDelay(5000);
            DpsCal_OutputCal_Data[0] = 3600.0f;
            DpsCal_OutputCal_Data[1] = Voltage;
            Refresh_DpsCal_Origin_Voltage();

            // 校准点2
            MCP4725_WriteFast(&hmcp4725_DC, 3200, MCP4725_MODE_NORMAL);
            DpsCal_UpdatePrompt("校准点2，等待稳定中", PROMPT_TEXT_NORMAL);
            osDelay(5000);
            DpsCal_OutputCal_Data[2] = 3200.0f;
            DpsCal_OutputCal_Data[3] = Voltage;
            Refresh_DpsCal_Origin_Voltage();

            // 校准点3
            MCP4725_WriteFast(&hmcp4725_DC, 2800, MCP4725_MODE_NORMAL);
            DpsCal_UpdatePrompt("校准点3，等待稳定中", PROMPT_TEXT_NORMAL);
            osDelay(5000);
            DpsCal_OutputCal_Data[4] = 2800.0f;
            DpsCal_OutputCal_Data[5] = Voltage;
            Refresh_DpsCal_Origin_Voltage();

            // 校准点4
            MCP4725_WriteFast(&hmcp4725_DC, 2400, MCP4725_MODE_NORMAL);
            DpsCal_UpdatePrompt("校准点4，等待稳定中", PROMPT_TEXT_NORMAL);
            osDelay(5000);
            DpsCal_OutputCal_Data[6] = 2400.0f;
            DpsCal_OutputCal_Data[7] = Voltage;
            Refresh_DpsCal_Origin_Voltage();

            // 校准点5
            MCP4725_WriteFast(&hmcp4725_DC, 2000, MCP4725_MODE_NORMAL);
            DpsCal_UpdatePrompt("校准点5，等待稳定中", PROMPT_TEXT_NORMAL);
            osDelay(5000);
            DpsCal_OutputCal_Data[8] = 2000.0f;
            DpsCal_OutputCal_Data[9] = Voltage;

            // 校准点6
            MCP4725_WriteFast(&hmcp4725_DC, 1600, MCP4725_MODE_NORMAL);
            DpsCal_UpdatePrompt("校准点6，等待稳定中", PROMPT_TEXT_NORMAL);
            osDelay(5000);
            DpsCal_OutputCal_Data[10] = 1600.0f;
            DpsCal_OutputCal_Data[11] = Voltage;

            // 校准点7
            MCP4725_WriteFast(&hmcp4725_DC, 1200, MCP4725_MODE_NORMAL);
            DpsCal_UpdatePrompt("校准点7，等待稳定中", PROMPT_TEXT_NORMAL);
            osDelay(5000);
            DpsCal_OutputCal_Data[12] = 1200.0f;
            DpsCal_OutputCal_Data[13] = Voltage;

            // 校准点8
            MCP4725_WriteFast(&hmcp4725_DC, 800, MCP4725_MODE_NORMAL);
            DpsCal_UpdatePrompt("校准点8，等待稳定中", PROMPT_TEXT_NORMAL);
            osDelay(5000);
            DpsCal_OutputCal_Data[14] = 800.0f;
            DpsCal_OutputCal_Data[15] = Voltage;

            // 校准点9
            MCP4725_WriteFast(&hmcp4725_DC, 400, MCP4725_MODE_NORMAL);
            DpsCal_UpdatePrompt("校准点9，等待稳定中", PROMPT_TEXT_NORMAL);
            osDelay(5000);
            DpsCal_OutputCal_Data[16] = 400.0f;
            DpsCal_OutputCal_Data[17] = Voltage;


            Refresh_DpsCal_Origin_Voltage();

            osDelay(1000);
            Suspend_DpsCoreTask();
            lcd_draw_string(58,90,"+0.000", &DIN_Medium32x48,0xeb0c,0x0000,2);

            DpsCal_UpdatePrompt("拟合计算中，请等待", PROMPT_TEXT_NORMAL);
            float DPS_Voltage_DAC_Coefficient,DPS_Voltage_DAC_Constant;
            int result = DpsCal_CalculateLinearCoeff(DpsCal_OutputCal_Data, &DPS_Voltage_DAC_Coefficient,
                                                     &DPS_Voltage_DAC_Constant);

            osDelay(2000);
            if (result == 0) {
                DpsCal_UpdatePrompt("校准成功，参数已保存！", PROMPT_TEXT_NORMAL);
                UserParam.DPS_Voltage_DAC_Coefficient = DPS_Voltage_DAC_Coefficient;
                UserParam.DPS_Voltage_DAC_Constant = DPS_Voltage_DAC_Constant;
            }
            else {
                DpsCal_UpdatePrompt("校准失败，请重试！", PROMPT_TEXT_ERROR);
                Suspend_DpsCoreTask();
            }

            output_cal_phase = OUTPUT_CAL_SAVE;
            Resume_IndevDetectTask();
        }
        else if (output_cal_phase == OUTPUT_CAL_SAVE) {
            UserParam_SaveAllValues();

            CAL_current_page = PAGE_MAIN;
            CAL_previous_page = PAGE_AWG_CAL;
            CAL_main_page_cursor_current = Main_DPS;
            CAL_main_page_cursor_past = Main_DPS;

            DrawMainBasicElement();

            StartBeezer(0);
            Resume_IndevDetectTask();
        }
    }
    else return;
}

int DpsCal_CalculateLinearCoeff(float data[10], float *A, float *B){
    if (data == NULL || A == NULL || B == NULL) {
        return -1;
    }

    const int n = 9;
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f;

    for (int i = 0; i < n; i++) {
        float dac = data[2 * i];
        float volt = data[2 * i + 1];
        sum_x += volt;
        sum_y += dac;
        sum_xy += volt * dac;
        sum_x2 += volt * volt;
    }

    float denom = (float)n * sum_x2 - sum_x * sum_x;
    *A = ((float)n * sum_xy - sum_x * sum_y) / denom;
    *B = (sum_y * sum_x2 - sum_x * sum_xy) / denom;
    return 0;
}


// 数控电源校准总响应函数（子页面分发）
void dps_cal_handler(KeyEventMsg_t msg) {
    DpsCalPage_handlers[DpsCal_Page_Current](msg);
}

// 界面持续更新函数
void Refresh_DpsCal_Origin_Voltage(void) {
    snprintf(Current_Voltage_str, sizeof(Current_Voltage_str), "%+06.3f", Voltage);
    lcd_draw_string(58,90,Current_Voltage_str, &DIN_Medium32x48,0xeb0c,0x0000,2);
}

void Refresh_DpsCal_Origin_Current(void) {
    snprintf(Current_Current_str, sizeof(Current_Current_str), "%+06.3f", Current);
    lcd_draw_string(58,90,Current_Current_str, &DIN_Medium32x48,0x5cbd,0x0000,2);
}

void Refresh_DpsCal_Display_Voltage(void) {
    snprintf(Current_Voltage_str, sizeof(Current_Voltage_str), "%+06.2f", Voltage);
    lcd_draw_string(58,90,Current_Voltage_str, &DIN_Medium32x48,0xeb0c,0x0000,2);
}


/* ========================== 其他校准界面 ========================== */

void dso_cal_handler(KeyEventMsg_t msg) {}
void awg_cal_handler(KeyEventMsg_t msg) {}
void dmm_v_cal_handler(KeyEventMsg_t msg) {}
void dmm_a_cal_handler(KeyEventMsg_t msg) {}
void dmm_r_cal_handler(KeyEventMsg_t msg) {}

void Suspend_CalibrateTask(void) {
    Suspend_IndevDetectTask();
    osThreadSuspend(CalibrateTaskHandle);
}

void Resume_CalibrateTask(void) {
    DrawMainBasicElement();
    osDelay(150);
    osThreadResume(CalibrateTaskHandle);
    Resume_IndevDetectTask();
}