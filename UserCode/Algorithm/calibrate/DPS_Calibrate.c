#include <inttypes.h>
#include <stdio.h>
#include <tgmath.h>

#include "DPS_Calibrate.h"
#include "lcd_draw_api.h"
#include "UserDefineManage.h"
#include "SwitchManager.h"
#include "tim.h"
#include "PID.h"
#include "os_handles.h"
#include "UserTask.h"

// ============= 外部变量和函数声明 =============
extern volatile uint16_t g_adc_raw_buf[128];                            // ADC原始数据           DpsCoreTask.c
extern volatile float Voltage;                                          // 输出电压值            DpsCoreTask.c
extern volatile float Current_PID;                                      // 输出电流值            DpsCoreTask.c
extern volatile bool IsPowerOn;                                         // 电源是否打开          DpsCoreTask.c
extern void CB_DPS_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);       // ADC转换完成回调函数    DpsCoreTask.c
extern void DPS_ADC_Init(void);                                         // ADC初始化              DpsCoreTask.c

// ============= 内部全局变量声明 =============
static char DpsVoltageOriginalMsg[6] = {0};                         // 原点校准_电压原点值
static char DpsCurrentOriginalMsg[6] = {0};                         // 原点校准_电流原点值
static char DpsVoltageDisplayMsg[6] = {0};                          // 显示校准_电压系数
static char DpsCurrentDisplayMsg[6] = {0};                          // 显示校准_电流系数
static char Current_Voltage_str[7];                                 // 当前电压值
static char Current_Current_str[7];                                 // 当前电流值
static uint8_t DpsCal_Origin_VolPage_current = 0;
static uint8_t DpsCal_Origin_CurPage_current = 0;
static uint8_t DpsCal_Display_VolPage_current = 0;
static uint8_t DpsCal_Display_CurPage_current = 0;
static float DpsCal_OutputCal_Data[18] = {0};
static uint8_t mode = 0;

// ============= 校准页面枚举 =============
typedef enum DpsCalPage{
    DpsCal_Origin_Voltage_Main = 0,
    DpsCal_Origin_Voltage_Edit,
    DpsCal_Origin_Current_Main,
    DpsCal_Origin_Current_Edit,
    DpsCal_Display_Voltage_Main,
    DpsCal_Display_Voltage_Edit,
    DpsCal_Display_Current_Main,
    DpsCal_Display_Current_Edit,
    DpsCal_Output_Main,
    DpsCal_Page_Num
} DpsCalPage_e;

static DpsCalPage_e DpsCal_Page_Current = DpsCal_Origin_Voltage_Main;   // 当前的页面


// ============= 页面刷新 =============
static void DrawDpsCalibratePage(void);                             // 绘制初始界面

static void Refresh_DpsCal_Origin_Voltage(void);                    // 刷新实时电压 （0.000）
static void Refresh_DpsCal_Origin_Current(void);                    // 刷新实时电流 （0.000）
static void Refresh_DpsCal_Display_Voltage(void);                   // 刷新实时电压 （00.00）
static void DoNothing(void){}                                       // 空函数

// 各页面对应的刷新函数映射表
static void (*page_refresh[DpsCal_Page_Num])(void) = {  // 有挺多复用的
    [DpsCal_Origin_Voltage_Main] = Refresh_DpsCal_Origin_Voltage,
    [DpsCal_Origin_Voltage_Edit] = Refresh_DpsCal_Origin_Voltage,
    [DpsCal_Origin_Current_Main] = Refresh_DpsCal_Origin_Current,
    [DpsCal_Origin_Current_Edit] = Refresh_DpsCal_Origin_Current,
    [DpsCal_Display_Voltage_Main] = Refresh_DpsCal_Display_Voltage,
    [DpsCal_Display_Voltage_Edit] = Refresh_DpsCal_Display_Voltage,
    [DpsCal_Display_Current_Main] = Refresh_DpsCal_Origin_Current,
    [DpsCal_Display_Current_Edit] = Refresh_DpsCal_Origin_Current,
    [DpsCal_Output_Main] = DoNothing,       // 这里由于全局都在按键回调中，所以需要手动控制刷新
};

// ============= 按键事件处理回调 =============
static void DpsCal_Origin_Voltage_Main_handler(KeyEventMsg_t msg);
static void DpsCal_Origin_Voltage_Edit_handler(KeyEventMsg_t msg);
static void DpsCal_Origin_Current_Main_handler(KeyEventMsg_t msg);
static void DpsCal_Origin_Current_Edit_handler(KeyEventMsg_t msg);
static void DpsCal_Display_Voltage_Main_handler(KeyEventMsg_t msg);
static void DpsCal_Display_Voltage_Edit_handler(KeyEventMsg_t msg);
static void DpsCal_Display_Current_Main_handler(KeyEventMsg_t msg);
static void DpsCal_Display_Current_Edit_handler(KeyEventMsg_t msg);
static void DpsCal_Output_Main_handler(KeyEventMsg_t msg);
static int DpsCal_CalculateLinearCoeff(float data[10], float *A, float *B);

// 各页面对应的按键处理函数映射表
static void (*DpsCalPage_handlers[DpsCal_Page_Num])(KeyEventMsg_t) = {
    [DpsCal_Origin_Voltage_Main] = DpsCal_Origin_Voltage_Main_handler,
    [DpsCal_Origin_Voltage_Edit] = DpsCal_Origin_Voltage_Edit_handler,
    [DpsCal_Origin_Current_Main] = DpsCal_Origin_Current_Main_handler,
    [DpsCal_Origin_Current_Edit] = DpsCal_Origin_Current_Edit_handler,
    [DpsCal_Display_Voltage_Main] = DpsCal_Display_Voltage_Main_handler,
    [DpsCal_Display_Voltage_Edit] = DpsCal_Display_Voltage_Edit_handler,
    [DpsCal_Display_Current_Main] = DpsCal_Display_Current_Main_handler,
    [DpsCal_Display_Current_Edit] = DpsCal_Display_Current_Edit_handler,
    [DpsCal_Output_Main] = DpsCal_Output_Main_handler,
};

// ============= 线性拟合参数 =============
typedef struct {
    uint16_t dac_value;
    char * message;
}DpsCal_Param_t;

static const DpsCal_Param_t DpsCal_Voltage_Param[9] = {
    {3600,"校准点1，等待稳定中"},
    {3200,"校准点2，等待稳定中"},
    {2800,"校准点3，等待稳定中"},
    {2400,"校准点4，等待稳定中"},
    {2000,"校准点5，等待稳定中"},
    {1600,"校准点6，等待稳定中"},
    {1200,"校准点7，等待稳定中"},
    {800,"校准点8，等待稳定中"},
    {400,"校准点9，等待稳定中"},
};

// ============= 公共函数实现 =============

// 数控电源校准 入口函数
void DPS_Calibrate_Enter(void) {
    DrawDpsCalibratePage();     // 绘制主界面
    
    DpsPower_OFF();             // 关闭电源输出
    DpsRelease_ON();            // 开启泄放电阻
    DPS_ADC_Init();             // 初始化ADC配置
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_COMPLETE_CB_ID, CB_DPS_ADC_ConvCpltCallback);   // 注册DMA回调
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_adc_raw_buf, 128);                                    // 启动ADC & DMA
    DpsTim_ON();                                // 开启时钟触发源
    
    PID_Init();                                 // 初始化PID任务
    osThreadResume(PIDTaskHandle);              // 恢复PID任务
}

void DPS_Calibrate_Refresh(void) {
    page_refresh[DpsCal_Page_Current]();        // 页面定时刷新函数分发
}

void dps_cal_handler(KeyEventMsg_t msg) {       // 按键事件分发
    DpsCalPage_handlers[DpsCal_Page_Current](msg);
}

// ============= 内部函数实现 =============

// 线性拟合函数
static int DpsCal_CalculateLinearCoeff(float data[10], float *A, float *B) {
    // DAC = A * Voltage + B , 我们线性拟合出未知量 A 和 B
    if (data == NULL || A == NULL || B == NULL) return -1;
    const int n = 9;                                                        // 总共有9个数据点
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f;
    for (int i = 0; i < n; i++) {
        float dac = data[2*i];
        float volt = data[2*i+1];
        sum_x += volt;                                                      // 计算 电压 和
        sum_y += dac;                                                       // 计算 DAC 和
        sum_xy += volt * dac;                                               // 计算 电压*DAC 和
        sum_x2 += volt * volt;                                              // 计算 电压 平方和
    }
    float denom = (float)n * sum_x2 - sum_x * sum_x;
    *A = ((float)n * sum_xy - sum_x * sum_y) / denom;
    *B = (sum_y * sum_x2 - sum_x * sum_xy) / denom;
    return 0;
}

// ============= 子页面 handler 实现 =============

// 电压原点校准 主页面
static void DpsCal_Origin_Voltage_Main_handler(KeyEventMsg_t msg) {
    // 如果 下键按下 & 编码器右转
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT) {
        DpsCal_Origin_VolPage_current = (DpsCal_Origin_VolPage_current < 1) ? (DpsCal_Origin_VolPage_current + 1) : 0;
    }
    // 如果 上键按下 & 编码器左转
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT) {
        DpsCal_Origin_VolPage_current = (DpsCal_Origin_VolPage_current > 0) ? (DpsCal_Origin_VolPage_current - 1) : 1;
    }
    // 如果 确认键按下 & 编码器按下
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (DpsCal_Origin_VolPage_current == 0) {                                   // 如果选中的是 参数编辑 框
            DpsCal_Page_Current = DpsCal_Origin_Voltage_Edit;                       // 将当前页面移交给 电压原点编辑 页面
            lcd_draw_line(162,225,191,225,0x87d3);                // 绘制编辑页面的下划线示意条
        }
        else if (DpsCal_Origin_VolPage_current == 1) {                                      // 如果选中的是 下一步 按钮
            DpsCal_Page_Current = DpsCal_Origin_Current_Main;                               // 将当前页面移交给 电流原点校准 主页面
            // 更新页面元素
            lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
            lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
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
    // 更新选择焦点框
    if (DpsCal_Origin_VolPage_current == 0) {
        lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
        lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
    } else {
        lcd_draw_round_rect(15,200,212,231,8,0x632C,0);
        lcd_draw_round_rect(220,200,305,231,8,0x07E0,0);
    }
}

// 电压原点校准 编辑页面
static void DpsCal_Origin_Voltage_Edit_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT) {
        UserParam.DPS_Voltage_Original += 1;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DPS_Voltage_Original -= 1;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        DpsCal_Page_Current = DpsCal_Origin_Voltage_Main;
        lcd_draw_line(162,225,191,225,0x18c6);
    }
    else {return;}
    StartBeezer(0);
    snprintf(DpsVoltageOriginalMsg, sizeof(DpsVoltageOriginalMsg), "%+03" PRId16, UserParam.DPS_Voltage_Original);
    lcd_draw_string(142,207,DpsVoltageOriginalMsg, &yahei18x18,0xFFFF,0x18c6,2);
}

// 电流原点校准 主页面
static void DpsCal_Origin_Current_Main_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT) {
        DpsCal_Origin_CurPage_current = (DpsCal_Origin_CurPage_current < 1) ? (DpsCal_Origin_CurPage_current + 1) : 0;
    }
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT) {
        DpsCal_Origin_CurPage_current = (DpsCal_Origin_CurPage_current > 0) ? (DpsCal_Origin_CurPage_current - 1) : 1;
    }
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (DpsCal_Origin_CurPage_current == 0) {
            DpsCal_Page_Current = DpsCal_Origin_Current_Edit;
            lcd_draw_line(162,225,191,225,0x87d3);
        }
        else if (DpsCal_Origin_CurPage_current == 1) {                                          // 如果选中的是下一步按钮
            DpsCal_Page_Current = DpsCal_Display_Voltage_Main;                                  // 将当前页面移交给 电压显示校准 主页面
            
            // 更新提示框文字
            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"连接万用表，使数值等于万用表值", &yahei16x16,0xdf3c,0x29a7,1);
            // 更新编辑框提示文字
            lcd_draw_round_rect(15,200,212,231,8,0x18c6,1);
            lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
            lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
            lcd_draw_string(25,208,"电压系数:", &yahei16x16,0xFFFF,0x18c6,1);
            // 更新编辑框数据
            snprintf(DpsVoltageDisplayMsg, sizeof(DpsVoltageDisplayMsg), "%05.2f", UserParam.DPS_Voltage_Factor);
            lcd_draw_string(110,207,DpsVoltageDisplayMsg, &yahei18x18,0xFFFF,0x18c6,0);
            // 更新数据显示单位
            lcd_draw_string(256,118,"V", &KaiTi16x20,0xeb0c,0x0000,0);
            // 更新进度框
            lcd_draw_round_rect(18,47,108,73,8,0x632C,1);
            lcd_draw_string(28, 52, "原点校准", &yahei16x16, 0xFFFF, 0x632C, 2);
            lcd_draw_round_rect(115,47,205,73,8,0x048A,1);
            lcd_draw_string(125, 52, "显示校准", &yahei16x16, 0xDEFB, 0x048A, 2);
            
            // 电源参数配置
            SetTargetVoltage(15.00f);       // 将输出电压设置成 15V     （此时没有校准输出，所以是不准的，13 ~ 17V 之间）
            IsPowerOn = true;               // 开启PID运算             （没用到PID运算，但是硬件控制在PID循环里）
            DpsPower_ON();                  // 开启PID输出
            osDelay(100);              // 等待100ms稳定  
            DpsRelease_OFF();               // 关闭泄放电阻             （可以防止过冲）
            DpsGnd_ON();                    // 连接 香蕉头负极 到 功率地
            DpsDc_ON();                     // 连接 香蕉头正极 到 电源正极
            
            StartBeezer(0);
            return;
        }
    }
    else {return;}
    StartBeezer(0);
    if (DpsCal_Origin_CurPage_current == 0) {
        lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
        lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
    } else {
        lcd_draw_round_rect(15,200,212,231,8,0x632C,0);
        lcd_draw_round_rect(220,200,305,231,8,0x07E0,0);
    }
}

// 电流原点校准 编辑界面
static void DpsCal_Origin_Current_Edit_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT) {
        UserParam.DPS_Current_Original += 1;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DPS_Current_Original -= 1;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        DpsCal_Page_Current = DpsCal_Origin_Current_Main;
        lcd_draw_line(162,225,191,225,0x18c6);
    }
    else {return;}
    StartBeezer(0);
    snprintf(DpsCurrentOriginalMsg, sizeof(DpsCurrentOriginalMsg), "%+03" PRId16, UserParam.DPS_Current_Original);
    lcd_draw_string(142,207,DpsCurrentOriginalMsg, &yahei18x18,0xFFFF,0x18c6,2);
}

// 电压显示校准 主界面
static void DpsCal_Display_Voltage_Main_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT) {
        DpsCal_Display_VolPage_current = (DpsCal_Display_VolPage_current < 1) ? (DpsCal_Display_VolPage_current + 1) : 0;
    }
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT) {
        DpsCal_Display_VolPage_current = (DpsCal_Display_VolPage_current > 0) ? (DpsCal_Display_VolPage_current - 1) : 1;
    }
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (DpsCal_Display_VolPage_current == 0) {
            DpsCal_Page_Current = DpsCal_Display_Voltage_Edit;
            lcd_draw_line(110,225,186,225,0x87d3);
        }
        else if (DpsCal_Display_VolPage_current == 1) {
            DpsCal_Page_Current = DpsCal_Display_Current_Main;
            
            lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
            lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
            lcd_draw_string(256,118,"A", &KaiTi16x20,0x5cbd,0x0000,0);
            
            lcd_draw_string(25,208,"电流系数:", &yahei16x16,0xFFFF,0x18c6,1);
            snprintf(DpsCurrentDisplayMsg, sizeof(DpsCurrentDisplayMsg), "%05.2f", UserParam.DPS_Current_Factor);
            lcd_draw_string(110,207,DpsCurrentDisplayMsg, &yahei18x18,0xFFFF,0x18c6,0);
            
            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"输出3.5V，请接负载。可不调", &yahei16x16,0xdf3c,0x29a7,1);
            
            SetTargetVoltage(3.5f);        // 将输出电压设置成 3.5V         (此时未校准，实际 3V ~ 4V之间)
            
            StartBeezer(0);
            return;
        }
    }
    else {return;}
    StartBeezer(0);
    if (DpsCal_Display_VolPage_current == 0) {
        lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
        lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
    } else {
        lcd_draw_round_rect(15,200,212,231,8,0x632C,0);
        lcd_draw_round_rect(220,200,305,231,8,0x07E0,0);
    }
}

// 电压显示校准 编辑界面
static void DpsCal_Display_Voltage_Edit_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT) {
        UserParam.DPS_Voltage_Factor += 0.01f;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DPS_Voltage_Factor -= 0.01f;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        DpsCal_Page_Current = DpsCal_Display_Voltage_Main;
        lcd_draw_line(110,225,186,225,0x18c6);
    }
    else {return;}
    StartBeezer(0);
    snprintf(DpsVoltageDisplayMsg, sizeof(DpsVoltageDisplayMsg), "%05.2f", UserParam.DPS_Voltage_Factor);
    lcd_draw_string(110,207,DpsVoltageDisplayMsg, &yahei18x18,0xFFFF,0x18c6,0);
}

// 电流显示校准 主界面
static void DpsCal_Display_Current_Main_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT) {
        DpsCal_Display_CurPage_current = (DpsCal_Display_CurPage_current < 1) ? (DpsCal_Display_CurPage_current + 1) : 0;
    }
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT) {
        DpsCal_Display_CurPage_current = (DpsCal_Display_CurPage_current > 0) ? (DpsCal_Display_CurPage_current - 1) : 1;
    }
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (DpsCal_Display_CurPage_current == 0) {
            DpsCal_Page_Current = DpsCal_Display_Current_Edit;
            lcd_draw_line(110,225,186,225,0x87d3);
            StartBeezer(0);
            return;
        }
        else if (DpsCal_Display_CurPage_current == 1) {
            DpsCal_Page_Current = DpsCal_Output_Main;
            
            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"请移除负载，按下按钮后自动校准", &yahei16x16,0xdf3c,0x29a7,1);
            
            lcd_draw_round_rect(15,200,212,231,8,0x18c6,1);
            lcd_draw_string(256,118,"V", &KaiTi16x20,0xeb0c,0x0000,0);
            
            Refresh_DpsCal_Origin_Voltage();
            
            lcd_draw_round_rect(115,47,205,73,8,0x632C,1);
            lcd_draw_string(125, 52, "显示校准", &yahei16x16, 0xDEFB, 0x632C, 2);
            lcd_draw_round_rect(212,47,302,73,8,0x048A,1);
            lcd_draw_string(222, 52, "输出校准", &yahei16x16, 0xDEFB, 0x048A, 2);
            
            IsPowerOn = false;              // 关闭 PID 
            DpsPower_OFF();                 // 关闭 DPS 电源
            DpsRelease_ON();                // 开启泄放电阻
            DpsDc_OFF();                    // 关闭 香蕉头正极 连接
            DpsGnd_OFF();                   // 关闭 香蕉头负极 连接
            SetTargetVoltage(2.0f);         // 设置初始电压 2V        （用不到，保险起见，以低电压起步）
            
            StartBeezer(0);
            return;
        }
    }
    StartBeezer(0);
    if (DpsCal_Display_CurPage_current == 0) {
        lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
        lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
    } else {
        lcd_draw_round_rect(15,200,212,231,8,0x632C,0);
        lcd_draw_round_rect(220,200,305,231,8,0x07E0,0);
    }
}

// 电压显示校准 编辑界面
static void DpsCal_Display_Current_Edit_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT) {
        UserParam.DPS_Current_Factor += 0.01f;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DPS_Current_Factor -= 0.01f;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        DpsCal_Page_Current = DpsCal_Display_Current_Main;
        lcd_draw_line(110,225,186,225,0x18c6);
        StartBeezer(0);
        return;
    }
    else {return;}
    StartBeezer(0);
    snprintf(DpsCurrentDisplayMsg, sizeof(DpsCurrentDisplayMsg), "%05.2f", UserParam.DPS_Current_Factor);
    lcd_draw_string(110,207,DpsCurrentDisplayMsg, &yahei18x18,0xFFFF,0x18c6,0);
}

// 输出校准 主界面
static void DpsCal_Output_Main_handler(KeyEventMsg_t msg) {
    if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        Suspend_IndevDetectTask();
        StartBeezer(0);

        if (mode == 0) {
            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"开始自动校准，请耐心等待", &yahei16x16,0xdf3c,0x29a7,1);
            osDelay(2000);

            // --- 校准循环开始 ---
            for (uint8_t cal_idx = 0; cal_idx < 9; cal_idx++) {
                // 更新屏幕显示
                lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
                lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
                lcd_draw_string(25, 165, DpsCal_Voltage_Param[cal_idx].message, &yahei16x16, 0xdf3c, 0x29a7, 1);
                // 设置 DAC 输出值
                MCP4725_WriteFast(&hmcp4725_DC, DpsCal_Voltage_Param[cal_idx].dac_value, MCP4725_MODE_NORMAL);
                // 仅在第一个校准点开启电源
                if (cal_idx == 0) {
                    DpsPower_ON();
                    DpsRelease_OFF();
                }
                // 等待读数稳定
                for (uint8_t i = 0; i < 25; i++) {
                    Refresh_DpsCal_Origin_Voltage();
                    osDelay(200);
                }
                // 保存校准数据 (DAC设定值 & 实际采样值)
                DpsCal_OutputCal_Data[cal_idx * 2]     = (float)DpsCal_Voltage_Param[cal_idx].dac_value;
                DpsCal_OutputCal_Data[cal_idx * 2 + 1] = Voltage;
            }
            // --- 校准循环结束 ---

            osDelay(1000);
            Suspend_DpsCoreTask();
            lcd_draw_string(58,90,"+0.000", &DIN_Medium32x48,0xeb0c,0x0000,2);

            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"拟合计算中，请等待", &yahei16x16,0xdf3c,0x29a7,1);

            float DPS_Voltage_DAC_Coefficient, DPS_Voltage_DAC_Constant;
            int result = DpsCal_CalculateLinearCoeff(DpsCal_OutputCal_Data, &DPS_Voltage_DAC_Coefficient,
                                                     &DPS_Voltage_DAC_Constant);
            osDelay(2000);

            if (result == 0) {
                lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
                lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
                lcd_draw_string(25,165,"校准成功，参数已保存！", &yahei16x16,0xdf3c,0x29a7,1);
                UserParam.DPS_Voltage_DAC_Coefficient = DPS_Voltage_DAC_Coefficient;
                UserParam.DPS_Voltage_DAC_Constant = DPS_Voltage_DAC_Constant;
            } else {
                lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
                lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
                lcd_draw_string(25,165,"校准失败，请重试！", &yahei16x16,0xF800,0x29a7,1);
                Suspend_DpsCoreTask();
            }
            mode = 1;
            Resume_IndevDetectTask();
        }
        else if (mode == 1) {
            UserParam_SaveAllValues();
            Calibrate_ReturnToMain();   // 返回主界面
        }
    }
    else return;
}


// ============= 界面刷新函数 =============

// 更新电压显示 (+0.000)
static void Refresh_DpsCal_Origin_Voltage(void) {
    snprintf(Current_Voltage_str, sizeof(Current_Voltage_str), "%+06.3f", Voltage);
    lcd_draw_string(58,90,Current_Voltage_str, &DIN_Medium32x48,0xeb0c,0x0000,2);
}

// 更新电流显示 (+0.000)
static void Refresh_DpsCal_Origin_Current(void) {
    snprintf(Current_Current_str, sizeof(Current_Current_str), "%+06.3f", Current_PID);
    lcd_draw_string(58,90,Current_Current_str, &DIN_Medium32x48,0x5cbd,0x0000,2);
}

// 更新电压显示 (00.00)
static void Refresh_DpsCal_Display_Voltage(void) {
    snprintf(Current_Voltage_str, sizeof(Current_Voltage_str), "%+06.2f", Voltage);
    lcd_draw_string(58,90,Current_Voltage_str, &DIN_Medium32x48,0xeb0c,0x0000,2);
}

// 绘制电源校准初始页面
static void DrawDpsCalibratePage(void) {
    // ===== 底色绘制 ===== //
    lcd_draw_rect(0, 0, 319, 31, 0x1908, 1);
    lcd_draw_rect(0, 32, 319, 239, 0x18c6, 1);
    // ===== 顶栏绘制 ===== //
    lcd_draw_line(0, 31, 319-1, 31, 0x11ac);
    lcd_draw_string(93, 6, "数控电源校准", &yahei20x20, 0x24be, 0x1908, 3);
    // ===== 进度提示绘制 ===== //
    lcd_draw_round_rect(18,47,108,73,8,0x048A,1);
    lcd_draw_string(28, 52, "原点校准", &yahei16x16, 0xFFFF, 0x048A, 2);
    lcd_draw_round_rect(115,47,205,73,8,0x632C,1);
    lcd_draw_string(125, 52, "显示校准", &yahei16x16, 0xDEFB, 0x632C, 2);
    lcd_draw_round_rect(212,47,302,73,8,0x632C,1);
    lcd_draw_string(222, 52, "输出校准", &yahei16x16, 0xDEFB, 0x632C, 2);
    // ===== 电压显示绘制 ===== //
    lcd_draw_round_rect(15,82,305,146,10,0x0000,1);
    lcd_draw_round_rect(15,82,305,146,10,0x632C,0);
    lcd_draw_string(58,90,"+0.000", &DIN_Medium32x48,0xeb0c,0x0000,2);
    lcd_draw_string(256,118,"V", &KaiTi16x20,0xeb0c,0x0000,0);
    //  ===== 提示框绘制 ===== //
    lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
    lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
    lcd_draw_string(25,165,"请移除负载，使数值接近0。", &yahei16x16,0xdf3c,0x29a7,1);
    // ====== 调节区绘制 ===== //
    lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
    lcd_draw_string(25,208,"电压原点:", &yahei16x16,0xFFFF,0x18c6,1);
    snprintf(DpsVoltageOriginalMsg, sizeof(DpsVoltageOriginalMsg), "%+03" PRId16, UserParam.DPS_Voltage_Original);
    lcd_draw_string(142,207,DpsVoltageOriginalMsg, &yahei18x18,0xFFFF,0x18c6,2);
    // ===== 下一步按钮绘制 ===== //
    lcd_draw_round_rect(220,200,305,231,8,0x632C,1);
    lcd_draw_string(235,207,"下一步", &yahei18x18,0xFFFF,0x632C,1);
}