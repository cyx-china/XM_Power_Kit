#include <inttypes.h>
#include <stdio.h>

#include "lcd_draw_api.h"
#include "UserTask.h"
#include "os_handles.h"
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
void Refresh_DpsCal_Origin_Voltage(void);
void Refresh_DpsCal_Origin_Current(void);
// 数控电源校准子页面响应函数枚举
typedef enum DpsCalPage{
    DpsCal_Origin_Voltage_Main = 0,         // 原点校准_电压部分_主界面
    DpsCal_Origin_Voltage_Edit,             // 原点校准_电压部分_编辑界面
    DpsCal_Origin_Current_Main,             // 原点校准_电流部分_主界面
    DpsCal_Origin_Current_Edit,             // 原点校准_电流部分_编辑界面
    DpsCal_Page_Num                         // 页面总数
}DpsCalPage_e;

DpsCalPage_e DpsCal_Page_Current = DpsCal_Origin_Voltage_Main;              // 当前的页面响应函数

static void (*page_refresh[DpsCal_Page_Num])(void) = {
    [DpsCal_Origin_Voltage_Main] = Refresh_DpsCal_Origin_Voltage,
    [DpsCal_Origin_Voltage_Edit] = Refresh_DpsCal_Origin_Voltage,
    [DpsCal_Origin_Current_Main] = Refresh_DpsCal_Origin_Current,
    [DpsCal_Origin_Current_Edit] = Refresh_DpsCal_Origin_Current,

};





/* ========================== 任务函数 ========================== */
uint8_t time_count = 0;
KeyEventMsg_t CAL_Keymsg;                       // 按键事件消息缓存
void Start_CalibrateTask(void *argument) {
    osThreadSuspend(CalibrateTaskHandle);       // 任务自挂启

    for (;;) {
        time_count++;
        // 接收按键事件
        if (osMessageQueueGet(KeyEventQueueHandle, &CAL_Keymsg, NULL, 0) == osOK)  // 替换为CAL_Keymsg
            page_handlers[CAL_current_page](CAL_Keymsg);  // 替换为CAL_current_page和CAL_Keymsg

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

/**
 * @brief 选项信息结构体
 */
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

char DpsVoltageOriginalMsg[6] = {0};
char DpsCurrentOriginalMsg[6] = {0};
void DpsCal_Origin_Voltage_Main_handler(KeyEventMsg_t msg);
void DpsCal_Origin_Voltage_Edit_handler(KeyEventMsg_t msg);
void DpsCal_Origin_Current_Main_handler(KeyEventMsg_t msg);
void DpsCal_Origin_Current_Edit_handler(KeyEventMsg_t msg);






static void (*DpsCalPage_handlers[DpsCal_Page_Num])(KeyEventMsg_t) = {      // 响应函数映射表
    [DpsCal_Origin_Voltage_Main] = DpsCal_Origin_Voltage_Main_handler ,
    [DpsCal_Origin_Voltage_Edit] = DpsCal_Origin_Voltage_Edit_handler,
    [DpsCal_Origin_Current_Main] = DpsCal_Origin_Current_Main_handler ,
    [DpsCal_Origin_Current_Edit] = DpsCal_Origin_Current_Edit_handler,

};

/*---------- 原点校准部分 ----------*/
// 原点校准基础界面绘制
void DrawDpsCalibratePage(void) {
    //绘制顶部状态栏
    lcd_draw_rect(0, 0, 319, 31, 0x1908, 1);
    lcd_draw_line(0, 31, 319 - 1, 31, 0x11ac);
    lcd_draw_string(93, 6, "数控电源校准", &yahei20x20, 0x24be, 0x1908, 3);

    // 绘制主背景
    lcd_draw_rect(0, 32, 319, 239, 0x18c6, 1);

    // 绘制步骤条
    lcd_draw_round_rect(18,47,108,73,8,0x048A,1); // 原点校准背景
    lcd_draw_string(28, 52, "原点校准", &yahei16x16, 0xFFFF, 0x048A, 2);
    lcd_draw_round_rect(115,47,205,73,8,0x632C,1); // 指数下降背景
    lcd_draw_string(125, 52, "显示校准", &yahei16x16, 0xDEFB, 0x632C, 2);
    lcd_draw_round_rect(212,47,302,73,8,0x632C,1); // 正弦波背景
    lcd_draw_string(222, 52, "输出校准", &yahei16x16, 0xDEFB, 0x632C, 2);

    // 绘制数据显示区
    lcd_draw_round_rect(15,82,305,146,10,0x0000,1); // 底色
    lcd_draw_round_rect(15,82,305,146,10,0x632C,0); // 1px边框
    lcd_draw_string(58,90,"+0.000", &DIN_Medium32x48,0xeb0c,0x0000,2);  // 初始数值
    lcd_draw_string(256,118,"V", &KaiTi16x20,0xeb0c,0x0000,0);  // 单位

    // 操作提示容器
    lcd_draw_round_rect(15,158,305,188,8,0x29a7,1); // 底色
    lcd_draw_round_rect(15,158,305,188,8,0x632C,0); // 边框
    lcd_draw_string(25,165,"请移除负载，使数值接近0。", &yahei16x16,0xdf3c,0x29a7,1);  // 提示信息

    // 用户输入区
    lcd_draw_round_rect(15,200,212,231,8,0x07E0,0); // 默认选上
    lcd_draw_string(25,208,"电压原点:", &yahei16x16,0xFFFF,0x18c6,1);

    snprintf(DpsVoltageOriginalMsg, sizeof(DpsVoltageOriginalMsg), "%+03" PRId16, UserParam.DPS_Voltage_Original);
    lcd_draw_string(142,207,DpsVoltageOriginalMsg, &yahei18x18,0xFFFF,0x18c6,2);

    // 下一步按钮
    lcd_draw_round_rect(220,200,305,231,8,0x632C,1);
    lcd_draw_string(235,207,"下一步", &yahei18x18,0xFFFF,0x632C,1);

}

// 电源校准_原点校准_电压部分_主界面响应逻辑
uint8_t DpsCal_Origin_VolPage_current = 0;                         // 0表示选中编辑框；1表示选中“下一步按钮”
void DpsCal_Origin_Voltage_Main_handler(KeyEventMsg_t msg) {
    // 编码器右转，下键按下
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT) {
        DpsCal_Origin_VolPage_current = (DpsCal_Origin_VolPage_current < 1) ? (DpsCal_Origin_VolPage_current + 1) : 0;
    }
    // 编码器左转，上键按下
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT) {
        DpsCal_Origin_VolPage_current = (DpsCal_Origin_VolPage_current > 0) ? (DpsCal_Origin_VolPage_current - 1) : 1;
    }
    // 单击
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (DpsCal_Origin_VolPage_current == 0) {                       // 如果当前选择着 原点编辑框
            DpsCal_Page_Current = DpsCal_Origin_Voltage_Edit;           // 切换到 原点编辑响应逻辑
            lcd_draw_line(162,225,191,225,0x87d3);     // 绘制选择示意条
        }
        else if (DpsCal_Origin_VolPage_current == 1) {// 如果当前选择着 下一步按钮
            DpsCal_Page_Current = DpsCal_Origin_Current_Main;           // 切换到 电流主界面

            lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);     // 更新注释条
            lcd_draw_round_rect(220,200,305,231,8,0x632C,0);

            lcd_draw_string(256,118,"A", &KaiTi16x20,0x5cbd,0x0000,0);  // 更新单位

            // 更新电流原点值
            lcd_draw_string(25,208,"电流原点:", &yahei16x16,0xFFFF,0x18c6,1);
            snprintf(DpsCurrentOriginalMsg, sizeof(DpsCurrentOriginalMsg), "%+03" PRId16, UserParam.DPS_Current_Original);
            lcd_draw_string(142,207,DpsCurrentOriginalMsg, &yahei18x18,0xFFFF,0x18c6,2);
            StartBeezer(0);     // 蜂鸣器
            return;
        }
    }
    else {return;}
    // 响应逻辑
    StartBeezer(0);     // 蜂鸣器
    // 更新选择框
    if (DpsCal_Origin_VolPage_current == 0) {
        lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
        lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
    }
    else {
        lcd_draw_round_rect(15,200,212,231,8,0x632C,0);
        lcd_draw_round_rect(220,200,305,231,8,0x07E0,0);
    }

}

// 电源校准_原点校准_电压部分_电压编辑响应逻辑
void DpsCal_Origin_Voltage_Edit_handler(KeyEventMsg_t msg) {
    // 右转编码器/右键按下
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT) {
        UserParam.DPS_Voltage_Original += 1;                    // 电压原点 加一
    }
    // 编码器左转/左键按下
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DPS_Voltage_Original -= 1;                    // 电压原点 减一
    }
    // 编码器按下/确认键按下
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        DpsCal_Page_Current = DpsCal_Origin_Voltage_Main;       // 切回主界面
        lcd_draw_line(162,225,191,225,0x18c6); // 清除选示意条
    }
    else {return;}
    // 响应逻辑
    StartBeezer(0);     // 蜂鸣器
    // 更新数值显示
    snprintf(DpsVoltageOriginalMsg, sizeof(DpsVoltageOriginalMsg), "%+03" PRId16, UserParam.DPS_Voltage_Original);
    lcd_draw_string(142,207,DpsVoltageOriginalMsg, &yahei18x18,0xFFFF,0x18c6,2);
}




uint8_t DpsCal_Origin_CurPage_current = 0;                         // 0表示选中编辑框；1表示选中“下一步按钮”
// 电源校准_原点校准_电流部分_主界面响应逻辑
void DpsCal_Origin_Current_Main_handler(KeyEventMsg_t msg) {
    // 编码器右转，下键按下
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT) {
        DpsCal_Origin_CurPage_current = (DpsCal_Origin_CurPage_current < 1) ? (DpsCal_Origin_CurPage_current + 1) : 0;
    }
    // 编码器左转，上键按下
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT) {
        DpsCal_Origin_CurPage_current = (DpsCal_Origin_CurPage_current > 0) ? (DpsCal_Origin_CurPage_current - 1) : 1;
    }
    // 单击
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (DpsCal_Origin_CurPage_current == 0) {
            // 如果当前选择着 原点编辑框
            DpsCal_Page_Current = DpsCal_Origin_Current_Edit;           // 切换到 原点编辑响应逻辑
            lcd_draw_line(162,225,191,225,0x87d3);     // 绘制选择示意条
        }
        else if (DpsCal_Origin_CurPage_current == 1) {  // 如果当前选择着 下一步按钮

        }
    }
    else {return;}

    StartBeezer(0);     // 蜂鸣器
    // 更新选择框
    if (DpsCal_Origin_CurPage_current == 0) {
        lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
        lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
    }
    else {
        lcd_draw_round_rect(15,200,212,231,8,0x632C,0);
        lcd_draw_round_rect(220,200,305,231,8,0x07E0,0);
    }
}

// 电源校准_原点校准_电流部分_电流编辑响应逻辑
void DpsCal_Origin_Current_Edit_handler(KeyEventMsg_t msg) {
    // 右转编码器/右键按下
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT) {
        UserParam.DPS_Current_Original += 1;                    // 电流原点 加一
    }
    // 编码器左转/左键按下
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DPS_Current_Original -= 1;                    // 电流原点 减一
    }
    // 编码器按下/确认键按下
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        DpsCal_Page_Current = DpsCal_Origin_Current_Main;       // 切回主界面
        lcd_draw_line(162,225,191,225,0x18c6); // 清除选示意条
    }
    else {return;}

    // 响应逻辑
    StartBeezer(0);     // 蜂鸣器
    // 更新数值显示
    snprintf(DpsCurrentOriginalMsg, sizeof(DpsCurrentOriginalMsg), "%+03" PRId16, UserParam.DPS_Current_Original);
    lcd_draw_string(142,207,DpsCurrentOriginalMsg, &yahei18x18,0xFFFF,0x18c6,2);
}

// 数控电源校准总响应函数（子页面分发）
void dps_cal_handler(KeyEventMsg_t msg) {
    DpsCalPage_handlers[DpsCal_Page_Current](msg);
}

// 界面持续更新函数
void Refresh_DpsCal_Origin_Voltage(void) {      // 原点_电压部分_刷新函数
    snprintf(Current_Voltage_str, sizeof(Current_Voltage_str), "%+06.3f", Voltage);
    lcd_draw_string(58,90,Current_Voltage_str, &DIN_Medium32x48,0xeb0c,0x0000,2);  // 初始数值
}

void Refresh_DpsCal_Origin_Current(void) {
    snprintf(Current_Current_str, sizeof(Current_Current_str), "%+06.3f", Current);
    lcd_draw_string(58,90,Current_Current_str, &DIN_Medium32x48,0x5cbd,0x0000,2);  // 初始数值
}



/* ========================== 示波器校准界面 ========================== */

void dso_cal_handler(KeyEventMsg_t msg) {

}

void awg_cal_handler(KeyEventMsg_t msg) {

}

void dmm_v_cal_handler(KeyEventMsg_t msg) {

}

void dmm_a_cal_handler(KeyEventMsg_t msg) {

}

void dmm_r_cal_handler(KeyEventMsg_t msg) {

}

void Suspend_CalibrateTask(void) {
    Suspend_IndevDetectTask();              // 挂起输入设备检测任务
    osThreadSuspend(CalibrateTaskHandle);
}

void Resume_CalibrateTask(void) {
    DrawMainBasicElement();                      // 绘制基础UI界面
    osDelay(150);                       // 延时确保UI绘制完成
    osThreadResume(CalibrateTaskHandle);     // 恢复DPS核心任务
    Resume_IndevDetectTask();                // 恢复输入设备检测任务
}