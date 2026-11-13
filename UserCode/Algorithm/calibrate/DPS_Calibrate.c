#include <inttypes.h>
#include "CalClamp.h"
#include "format.h"
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
extern volatile uint16_t dps_adc_raw_buf[128];                          // ADC原始数据           DpsCoreTask.c
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
static uint8_t DpsCal_CheckLinearCoeff(float data[10], float *A, float *B, float *r2);

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
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)dps_adc_raw_buf, 128);                                    // 启动ADC & DMA
    AdcTim_OFF();
    __HAL_TIM_SET_PRESCALER(&htim8, 149);   // 设定更新频率64Khz
    __HAL_TIM_SET_AUTORELOAD(&htim8, 14);
    AdcTim_ON();                                                            // 启动定时器（ADC触发源）
    
    PID_Init();                                 // 初始化PID任务
    PID_InvalidateDacCache();                   // 校准期间直接写过 DAC，恢复前让 PID 强制重写
    osThreadResume(PIDTaskHandle);              // 恢复PID任务
}

void DPS_Calibrate_Refresh(void) {
    page_refresh[DpsCal_Page_Current]();        // 页面定时刷新函数分发
}

void dps_cal_handler(KeyEventMsg_t msg) {       // 按键事件分发
    DpsCalPage_handlers[DpsCal_Page_Current](msg);
}

// ============= 内部函数实现 =============

// 线性拟合函数：DAC = A * Voltage + B
//
// 只在"数据确实可信"时才写参数，任何一项不满足都返回失败标志位，
// 调用处据此显示失败原因（如"失败，K,B,R"）并且不写入 UserParam
// （原实现直接除 denom：9 个点电压几乎相同时 denom≈0，A/B 会变成 inf/NaN
//   并被保存，之后电源就再也调不起来）。
//   标志位可以多位同时成立：
//     V = 有校准点的电压读数超出合理范围（没接线 / 没开电源 / 读数贴边）
//     D = 电压几乎不变，无法拟合
//     K = 斜率 A 的符号或量级不合理（DAC 与电压负相关，约 -100）
//     B = 截距 B 超出合理范围
//     R = 线性度 R² < 0.999（采样抖动过大）
#define DPS_CAL_FAIL_V   0x01U
#define DPS_CAL_FAIL_D   0x02U
#define DPS_CAL_FAIL_K   0x04U
#define DPS_CAL_FAIL_B   0x08U
#define DPS_CAL_FAIL_R   0x10U
#define DPS_CAL_FAIL_ARG 0x80U      // 入参非法（正常流程不会出现）

static uint8_t DpsCal_CheckLinearCoeff(float data[10], float *A, float *B, float *r2) {
    if (data == NULL || A == NULL || B == NULL) return DPS_CAL_FAIL_ARG;
    if (r2 != NULL) *r2 = 0.0f;

    const int n = 9;                                                        // 总共有9个数据点
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f;
    uint8_t flags = 0;

    for (int i = 0; i < n; i++) {
        float dac  = data[2*i];
        float volt = data[2*i+1];

        if (volt < 0.20f || volt > 38.0f) flags |= DPS_CAL_FAIL_V;          // 单点不可信

        sum_x  += volt;                                                     // 计算 电压 和
        sum_y  += dac;                                                      // 计算 DAC 和
        sum_xy += volt * dac;                                               // 计算 电压*DAC 和
        sum_x2 += volt * volt;                                              // 计算 电压 平方和
    }

    float denom = (float)n * sum_x2 - sum_x * sum_x;
    if (fabsf(denom) < 1.0e-3f) return flags | DPS_CAL_FAIL_D;              // 电压几乎不变，后面的量算不出来

    float a = ((float)n * sum_xy - sum_x * sum_y) / denom;
    float b = (sum_y * sum_x2 - sum_x * sum_xy) / denom;

    if (a > -20.0f || a < -400.0f) flags |= DPS_CAL_FAIL_K;                // 斜率必须是负的
    if (b < 1000.0f || b > 9000.0f) flags |= DPS_CAL_FAIL_B;               // 截距在 0~4095 附近

    // R² 线性度校验：残差与总平方差都必须用 DAC（被预测量）的单位，
    // 拿电压方差当分母会与残差量纲不一致，导致 R² 恒在 0.99 附近、永远判不达标
    float mean_dac = sum_y / (float)n;
    float ss_tot = 0.0f, ss_res = 0.0f;
    for (int i = 0; i < n; i++) {
        float volt = data[2*i+1];
        float dac  = data[2*i];
        float pred = a * volt + b;
        ss_tot += (dac - mean_dac) * (dac - mean_dac);
        ss_res += (dac - pred) * (dac - pred);
    }
    if (ss_tot <= 0.0f) {
        flags |= DPS_CAL_FAIL_D;
    } else {
        float r_squared = 1.0f - ss_res / ss_tot;                           // 线性度（1.0 表示完全落在拟合直线上）
        if (r2 != NULL) *r2 = r_squared;
        if (r_squared < 0.999f) flags |= DPS_CAL_FAIL_R;
    }

    if (flags != 0U) return flags;                                          // 不可信：不写 A/B

    *A = a;
    *B = b;
    return 0;
}

// 把失败标志位拼成提示串，例如 "失败，K,B,R"
static void DpsCal_FormatFailMsg(uint8_t flags, float r2, char *buf, size_t size) {
    static const struct { uint8_t bit; char letter; } items[] = {
        {DPS_CAL_FAIL_V, 'V'},
        {DPS_CAL_FAIL_D, 'D'},
        {DPS_CAL_FAIL_K, 'K'},
        {DPS_CAL_FAIL_B, 'B'},
    };

    int used = snprintf(buf, size, "失败，");
    if (used < 0 || (size_t) used >= size) return;

    bool first = true;
    for (size_t i = 0; i < sizeof items / sizeof items[0]; i++) {
        if ((flags & items[i].bit) == 0U) continue;
        int w = snprintf(buf + used, size - (size_t) used, "%s%c", first ? "" : ",", items[i].letter);
        if (w < 0 || (size_t) (used + w) >= size) return;
        used += w;
        first = false;
    }

    // R² 后面直接带上实测值（三位小数），方便判断到底差多少
    if ((flags & DPS_CAL_FAIL_R) != 0U) {
        int32_t milli = (int32_t) (r2 * 1000.0f + 0.5f);                    // 千分位取整，不用 %f
        if (milli < 0) milli = 0;
        if (milli > 9999) milli = 9999;
        int w = snprintf(buf + used, size - (size_t) used, "%sR:%d.%03d",
                         first ? "" : ",", (int) (milli / 1000), (int) (milli % 1000));
        if (w < 0 || (size_t) (used + w) >= size) return;
        used += w;
        first = false;
    }

    if (first) (void) snprintf(buf + used, size - (size_t) used, "未知");
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
        UserParam.DPS_Voltage_Original = cal_clamp_i16((int32_t) UserParam.DPS_Voltage_Original + 1, CAL_OFFSET_MIN, CAL_OFFSET_MAX);
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DPS_Voltage_Original = cal_clamp_i16((int32_t) UserParam.DPS_Voltage_Original - 1, CAL_OFFSET_MIN, CAL_OFFSET_MAX);
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
            lcd_draw_string(25,165,"连接万用表，使万用表值等于数值", &yahei16x16,0xdf3c,0x29a7,1);
            // 更新编辑框提示文字
            lcd_draw_round_rect(15,200,212,231,8,0x18c6,1);
            lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
            lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
            lcd_draw_string(25,208,"电压系数:", &yahei16x16,0xFFFF,0x18c6,1);
            // 更新编辑框数据
            fmt_fixed(DpsVoltageDisplayMsg, sizeof(DpsVoltageDisplayMsg), UserParam.DPS_Voltage_Factor, 2, 5, FMT_ZERO_PAD);
            lcd_draw_string(110,207,DpsVoltageDisplayMsg, &yahei18x18,0xFFFF,0x18c6,0);
            // 更新数据显示单位
            lcd_draw_string(256,118,"V", &KaiTi16x20,0xeb0c,0x0000,0);
            // 更新进度框
            lcd_draw_round_rect(18,47,108,73,8,0x632C,1);
            lcd_draw_string(28, 52, "原点校准", &yahei16x16, 0xFFFF, 0x632C, 2);
            lcd_draw_round_rect(115,47,205,73,8,0x048A,1);
            lcd_draw_string(125, 52, "显示校准", &yahei16x16, 0xDEFB, 0x048A, 2);
            
            // 电源参数配置（PID 任务同时在跑，I2C3 总线必须加锁）
            if (osMutexAcquire(IIC3_MutexHandle, osWaitForever) == osOK) {
                MCP4725_WriteFast(&hmcp4725_DC, 2800, MCP4725_MODE_NORMAL);
                osMutexRelease(IIC3_MutexHandle);
            }
            IsPowerOn = true;               // 开启PID运算
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
        UserParam.DPS_Current_Original = cal_clamp_i16((int32_t) UserParam.DPS_Current_Original + 1, CAL_OFFSET_MIN, CAL_OFFSET_MAX);
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DPS_Current_Original = cal_clamp_i16((int32_t) UserParam.DPS_Current_Original - 1, CAL_OFFSET_MIN, CAL_OFFSET_MAX);
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
            fmt_fixed(DpsCurrentDisplayMsg, sizeof(DpsCurrentDisplayMsg), UserParam.DPS_Current_Factor, 2, 5, FMT_ZERO_PAD);
            lcd_draw_string(110,207,DpsCurrentDisplayMsg, &yahei18x18,0xFFFF,0x18c6,0);
            
            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"输出1.5V，请接负载。可不调", &yahei16x16,0xdf3c,0x29a7,1);
            
            SetTargetVoltage(1.5f);        // 将输出电压设置成 1.5V
            
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
        UserParam.DPS_Voltage_Factor = cal_clamp_f(UserParam.DPS_Voltage_Factor + 0.01f, CAL_FACTOR_MIN, CAL_FACTOR_MAX);
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DPS_Voltage_Factor = cal_clamp_f(UserParam.DPS_Voltage_Factor - 0.01f, CAL_FACTOR_MIN, CAL_FACTOR_MAX);
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        DpsCal_Page_Current = DpsCal_Display_Voltage_Main;
        lcd_draw_line(110,225,186,225,0x18c6);
    }
    else {return;}
    StartBeezer(0);
    fmt_fixed(DpsVoltageDisplayMsg, sizeof(DpsVoltageDisplayMsg), UserParam.DPS_Voltage_Factor, 2, 5, FMT_ZERO_PAD);
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
        UserParam.DPS_Current_Factor = cal_clamp_f(UserParam.DPS_Current_Factor + 0.01f, CAL_FACTOR_MIN, CAL_FACTOR_MAX);
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DPS_Current_Factor = cal_clamp_f(UserParam.DPS_Current_Factor - 0.01f, CAL_FACTOR_MIN, CAL_FACTOR_MAX);
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        DpsCal_Page_Current = DpsCal_Display_Current_Main;
        lcd_draw_line(110,225,186,225,0x18c6);
        StartBeezer(0);
        return;
    }
    else {return;}
    StartBeezer(0);
    fmt_fixed(DpsCurrentDisplayMsg, sizeof(DpsCurrentDisplayMsg), UserParam.DPS_Current_Factor, 2, 5, FMT_ZERO_PAD);
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
                // 设置 DAC 输出值（PID 任务同时在跑，I2C3 总线必须加锁）
                if (osMutexAcquire(IIC3_MutexHandle, osWaitForever) == osOK) {
                    MCP4725_WriteFast(&hmcp4725_DC, DpsCal_Voltage_Param[cal_idx].dac_value, MCP4725_MODE_NORMAL);
                    osMutexRelease(IIC3_MutexHandle);
                }
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

            float DPS_Voltage_DAC_Coefficient = 0.0f, DPS_Voltage_DAC_Constant = 0.0f;
            float fit_r2 = 0.0f;
            uint8_t fit_flags = DpsCal_CheckLinearCoeff(DpsCal_OutputCal_Data, &DPS_Voltage_DAC_Coefficient,
                                                        &DPS_Voltage_DAC_Constant, &fit_r2);
            osDelay(2000);

            if (fit_flags == 0U) {
                lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
                lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
                lcd_draw_string(25,165,"校准成功，参数已保存！", &yahei16x16,0xdf3c,0x29a7,1);
                UserParam.DPS_Voltage_DAC_Coefficient = DPS_Voltage_DAC_Coefficient;
                UserParam.DPS_Voltage_DAC_Constant = DPS_Voltage_DAC_Constant;
            } else {
                char fail_msg[32];
                DpsCal_FormatFailMsg(fit_flags, fit_r2, fail_msg, sizeof fail_msg);
                lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
                lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
                lcd_draw_string(25,165, fail_msg, &yahei16x16,0xF800,0x29a7,1);
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
    fmt_fixed(Current_Voltage_str, sizeof(Current_Voltage_str), Voltage, 3, 6, FMT_SIGN | FMT_ZERO_PAD);
    lcd_draw_string(58,90,Current_Voltage_str, &DIN_Medium32x48,0xeb0c,0x0000,2);
}

// 更新电流显示 (+0.000)
static void Refresh_DpsCal_Origin_Current(void) {
    fmt_fixed(Current_Current_str, sizeof(Current_Current_str), Current_PID, 3, 6, FMT_SIGN | FMT_ZERO_PAD);
    lcd_draw_string(58,90,Current_Current_str, &DIN_Medium32x48,0x5cbd,0x0000,2);
}

// 更新电压显示 (00.00)
static void Refresh_DpsCal_Display_Voltage(void) {
    fmt_fixed(Current_Voltage_str, sizeof(Current_Voltage_str), Voltage, 2, 6, FMT_SIGN | FMT_ZERO_PAD);
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