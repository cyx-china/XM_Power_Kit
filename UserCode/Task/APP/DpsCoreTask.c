/**
******************************************************************************
  * @file           : DpsCoreTask.c
  * @brief          : 数字电源（DPS）系统核心任务，包括数据采集、UI显示控制、按键事件处理，
  *                   ,电源参数管理,以及PID算法控制
  * @date           : 2026/2/21
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  *
  * This DPS core task module is independently developed by the author.
  * It is released under the CC-BY-NC-SA 4.0 open source license.
  * And the author's right of attribution is reserved.
  ******************************************************************************
  */

/* ========================== 系统库头文件包含 ========================== */
#include <math.h>
#include "adc.h"
#include "tim.h"
#include "cmsis_os2.h"

/* ========================== 自定义库头文件包含 ========================== */
#include "graphics.h"
#include "lcd_draw_api.h"
#include "UserDefineManage.h"
#include "SwitchManager.h"
#include "UserTask.h"
#include "PID.h"
#include "format.h"
#include "DpsGraph.h"

/* ========================== 宏定义 ========================== */
/**
 * @brief 毫秒转小时系数
 * @note  用于能量计算（能量=功率×时间），1小时=3600000毫秒
 */
#define MS_TO_HOUR (1.0 / 3600000.0)     // 用 double：能量是小量累加，float 会丢精度

/* 曲线采样周期 DPS_GRAPH_SAMPLE_PERIOD_MS 定义在 DpsGraph.h（一列一点，266 点 ≈ 10 秒） */

/**
 * @brief 能量积分的默认步长（小时）
 * @note  开机阶段 ADC 尚未配置时使用；实际步长由 DPS_UpdateEnergyPeriod() 按 TIM8 配置刷新
 */
#define ENERGY_DT_HOUR_DEFAULT MS_TO_HOUR

/* ========================== 枚举类型定义 ========================== */
/**
 * @brief APP页面枚举
 * @note  定义DPS系统的所有功能页面
 */
typedef enum {
    PAGE_MAIN = 0,      // 主界面（默认页面）
    PAGE_VOLT_DIGIT,    // 电压数位编辑页面
    PAGE_CURR_DIGIT,    // 电流数位编辑页面
    PAGE_QUICK_MENU,    // 快速设置菜单页面
    PAGE_MAIN_GRAPH,    // 主界面曲线显示页面
    PAGE_NUM            // 页面总数
} DPS_AppPage_t;

/**
 * @brief 主界面光标枚举
 * @note  主界面可聚焦的操作项
 */
typedef enum {
    Main_Voltage = 0,   // 电压设置项
    Main_Current,       // 电流设置项
    Main_Setting,       // 快速设置项
    MAIN_NUM
} MainPageCursor_e;

/**
 * @brief 数位编辑位置枚举
 * @note  电压/电流编辑时的数位选择（百分位/十分位/个位/十位）
 */
typedef enum {
    hundredths = 0,     // 百分位（0.01V/A）
    tenths,             // 十分位（0.1V/A）
    units,              // 个位（1V/A）
    tens                // 十位（10V/A）
} DigitPosition_e;

/**
 * @brief 快速设置选项枚举
 * @note  快速设置菜单的5个预设项索引
 */
typedef enum {
    option_1 = 0,       // 快速设置第1项
    option_2,           // 快速设置第2项
    option_3,           // 快速设置第3项
    option_4,           // 快速设置第4项
    option_5            // 快速设置第5项
} FastSetOption_e;

/* ========================== 结构体定义 ========================== */
/**
 * @brief 焦点框绘制参数结构体
 * @note  用于表驱动管理不同焦点框的绘制属性
 */
typedef struct {
    uint16_t x;         // 焦点框X坐标
    uint16_t y;         // 焦点框Y坐标
    uint16_t w;         // 焦点框宽度
    uint16_t h;         // 焦点框高度
    uint16_t radius;    // 焦点框圆角半径
    uint16_t color;     // 焦点框颜色
    uint8_t filled;     // 填充模式：1-实心（电压/电流焦点框），0-空心边框（快速设置焦点框）
} FocusRect_t;

/**
 * @brief 进度条绘制上下文
 * @note  主界面和曲线页各有一套进度条，几何/颜色/量程都在这里描述，
 *        由 DrawBar() 统一绘制，避免为每个进度条写一份重复代码
 */
typedef struct {
    uint16_t x0;                // 轨道左端X坐标
    uint16_t y0;                // 轨道顶部Y坐标
    uint16_t x1;                // 轨道右端X坐标
    uint16_t y1;                // 轨道底部Y坐标
    uint16_t color;             // 填充色
    uint16_t bg;                // 轨道底色
    float max;                  // 满量程值
    uint8_t last_px;            // 上一次填充像素数（防抖刷新用）
} BarCtx_t;

/**
 * @brief 数位编辑上下文结构体
 * @note  统一管理电压/电流数位编辑的相关参数，主界面与曲线页各一套，
 *        因此坐标/下划线X表/进度条都以成员变量给出，不再写死在处理函数里
 */
typedef struct {
    volatile float *pTarget;    // 目标值指针（Voltage/Current）
    char *pSetMsg;              // 设定值显示字符串缓存指针
    uint16_t xSet;              // 设定值显示X坐标
    uint16_t yValue;            // 设定值显示Y坐标
    uint16_t color;             // 编辑态颜色
    uint16_t yUnderlineBase;    // 下划线Y坐标（数位选择指示）
    const uint16_t *xDigit;     // 4个数位下划线的X坐标表
    BarCtx_t *bar;              // 对应进度条上下文
    uint8_t isVoltage;          // 1-电压，0-电流
    uint8_t isGraph;            // 1-从曲线页进入，0-从主界面进入
    int8_t spacing;             // 设定值字间距（曲线页字窄，用 1；主界面用 2）
} DigitEditContext_t;

/**
 * @brief 快速设置项结构体
 * @note  快速设置菜单项的表驱动参数
 */
typedef struct {
    float volt;                 // 预设电压值
    float curr;                 // 预设电流值
    char *buf;                  // 显示字符串缓存
    uint16_t y;                 // 显示Y坐标
} QuickSetItem_t;

/* ========================== 全局变量 ========================== */
DPS_AppPage_t current_page = PAGE_MAIN;     // 当前激活页面
DPS_AppPage_t previous_page = PAGE_MAIN;    // 上一个页面（用于返回）

volatile bool PowerMode = false;        // 功率模式：false-CV(恒压)，true-CC(恒流)
bool PowerMode_last = false;            // 上一次功率模式（用于对比刷新）

char InputMsg[12];                      // 输入电压/电流显示缓存（格式：XX.XXV XX.XXA）
char TempMsg[6];                        // 温度显示缓存（格式：XX.XC）
char VoltageMsg[7];                     // 输出电压显示缓存
char CurrentMsg[7];                     // 输出电流显示缓存
char PowerMsg[7];                       // 输出功率显示缓存
char VoltageSetMsg[7];                  // 目标电压显示缓存
char Current_SetMsg[7];                 // 目标电流显示缓存
char EnergyMsg[10];                     // 能量显示缓存（格式：XXXX.XXXWh）
char TimeMsg[9];                        // 运行时间显示缓存（格式：XXX:XX:XX）
char FanMsg[5];                         // 风扇占空比显示缓存（格式：XXX%）

char set1[12], set2[12], set3[12], set4[12], set5[12];  // 快速设置5个项的显示缓存

bool Flush_OutputData = true;                // 输出数据刷新使能：true-刷新，false-暂停
volatile bool IsPowerOn = false;             // 电源使能：true-开启，false-关闭

volatile uint16_t dps_adc_raw_buf[128] = {0};  // ADC原始采样缓冲区（128个值，偶数列-电压，奇数列-电流）
volatile float Voltage = 0;                  // 实时输出电压（单位：V）
volatile float Voltage_PID = 0;              // 实时输出电压_PID（单位：V）
volatile float Current = 0;                  // 实时输出电流_显示（单位：A）
volatile float Current_PID = 0;              // 实时输出电流_PID（单位：A）  此变量也用于校准
volatile float Power = 0;                    // 实时输出功率（单位：W）  在ADC中断中更新
volatile double energy = 0;                  // 累计输出能量（单位：Wh） 在ADC中断中累加
                                             // 必须用 double：单次增量只有 ~1e-6 Wh（5W 负载），
                                             // float 在数值涨到几 Wh 之后的最小步进就大于增量，
                                             // 加法会被舍入掉，表现为"能量不再增长"。

// 单次 ADC 回调（64 对采样）对应的时长（单位：小时）
double energy_dt_hour = ENERGY_DT_HOUR_DEFAULT;

/**
 * @brief  读取累计能量（供任务上下文使用）
 * @note   double 在 32 位 MCU 上不是原子类型，中断里更新时会分两次写 32 位字，
 *         任务直接读可能读到"高低半字不同步"的撕裂值，因此这里短暂关中断再读。
 */
double DPS_GetEnergy(void) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    double value = energy;
    if (primask == 0U) {
        __enable_irq();
    }
    return value;
}

uint32_t running_time = 0;                   // 电源运行时长（单位：秒）

uint8_t widgets_update_time_count = 0;          // 界面控件更新计时（每10ms+1，20次=200ms刷新）
uint8_t Time_update_time_count = 0;             // 时间更新计时（每10ms+1，100次=1s刷新）
uint8_t Fan_update_time_count = 0;              // 风扇占空比更新计时（每10ms+1，100次=1s刷新）

KeyEventMsg_t DPS_Keymsg;               // 按键事件消息缓存

MainPageCursor_e main_page_cursor_current = Main_Voltage;   // 主界面当前光标位置
MainPageCursor_e main_page_cursor_past = Main_Voltage;      // 主界面上一次光标位置

DigitPosition_e VoltagePosition_Current = hundredths;       // 电压当前编辑数位
DigitPosition_e VoltagePosition_Past = hundredths;          // 电压上一次编辑数位

DigitPosition_e CurrentPosition_Current = hundredths;       // 电流当前编辑数位
DigitPosition_e CurrentPosition_Past = hundredths;          // 电流上一次编辑数位

FastSetOption_e current_option = option_1;                  // 快速设置当前选中项
FastSetOption_e previous_option = option_1;                 // 快速设置上一次选中项

const float dps_pow10_table[] = {1.0f, 10.0f, 100.0f, 1000.0f}; // 数位权重表（对应百分位到十位）

/* ========================== 静态全局变量 ========================== */
// 四套进度条：主界面（宽 130）/曲线页（宽 108，起点避开设定值末位）
static BarCtx_t volt_bar_main  = {180, 40, 309,  47, 0xeb0c, 0x6a49, MAX_OUTPUT_VOLTAGE, 0};
static BarCtx_t curr_bar_main  = {180, 93, 309, 100, 0x5cbd, 0x3ace, MAX_OUTPUT_CURRENT, 0};
static BarCtx_t volt_bar_graph = {208, 43, 315,  50, 0xeb0c, 0x6a49, MAX_OUTPUT_VOLTAGE, 0};
static BarCtx_t curr_bar_graph = {208, 82, 315,  89, 0x5cbd, 0x3ace, MAX_OUTPUT_CURRENT, 0};

// 曲线页实测值缓存（与主界面的大字号缓存分开，避免互相干扰）
static char graph_VoltageMsg[7] = {0};
static char graph_CurrentMsg[7] = {0};
static char graph_last_VoltageMsg[7] = {0};
static char graph_last_CurrentMsg[7] = {0};
static uint8_t graph_chip_on = 0xFF;        // 曲线页状态栏电源开关显示缓存
static uint8_t graph_chip_mode = 0xFF;      // 曲线页状态栏 CV/CC 显示缓存
static uint8_t graph_frame_pending = 0;     // 有新样本，等待重绘曲线
static uint32_t graph_sample_tick = 0;      // 上一次采样时的系统节拍（毫秒）

static MainPageCursor_e graph_cursor_current = Main_Voltage;   // 曲线页当前光标（仅电压/电流）
static MainPageCursor_e graph_cursor_past = Main_Voltage;      // 曲线页上一次光标

static char last_InputMsg[12] = {0};        // 上一次输入电压/电流显示值（用于对比刷新）
static char last_TempMsg[6] = {0};          // 上一次温度显示值（用于对比刷新）
static char last_VoltageMsg[7] = {0};       // 上一次输出电压显示值（用于对比刷新）
static char last_CurrentMsg[7] = {0};       // 上一次输出电流显示值（用于对比刷新）
static char last_PowerMsg[7] = {0};         // 上一次输出功率显示值（用于对比刷新）

/* ========================== 表驱动常量定义 ========================== */
/**
 * @brief 进度条绘制函数声明
 * @note  要在上下文常量之前声明
 */
static void DrawBar(BarCtx_t *bar, float value);    // 通用进度条绘制
static void DrawDpsGraphPage(void);                 // 绘制曲线页整页
static void DrawGraphPowerChip(void);               // 绘制状态栏电源开关/CV-CC
static void UpdateStatusBarInfo(void);              // 刷新状态栏输入/温度（主界面与曲线页共用）

/**
 * @brief 主界面焦点框表驱动
 * @note  索引对应MainPageCursor_e枚举：0-Voltage，1-Current，2-Setting
 */
static const FocusRect_t main_focus_rects[3] = {
    {180, 55, 6, 26, 3, 0x87d3, 1},     // Voltage: 实心填充焦点框
    {180, 107, 6, 26, 3, 0x87d3, 1},    // Current: 实心填充焦点框
    {178, 140, 130, 34, 10, 0x87d3, 0}  // Setting: 空心边框焦点框
};

/**
 * @brief 数位下划线X坐标表
 * @note  顺序：百分位、十分位、个位、十位（与DigitPosition_e对应）
 */
static const uint16_t digit_x_main[4]  = {279, 257, 219, 197};  // 主界面（设定值X=197）
static const uint16_t digit_x_graph[4] = {183, 162, 127, 106};  // 曲线页（设定值X=106，字间距1）

/**
 * @brief 曲线页焦点框表驱动
 * @note  索引对应MainPageCursor_e，曲线页只有电压/电流两项
 */
static const FocusRect_t graph_focus_rects[2] = {
    {100, 37, 6, 29, 3, 0x87d3, 1},     // Voltage: 实心填充焦点框
    {100, 76, 6, 29, 3, 0x87d3, 1}      // Current: 实心填充焦点框
};

/**
 * @brief 电压编辑上下文（主界面）
 */
static const DigitEditContext_t volt_ctx = {
    .pTarget        = &Target_Voltage,
    .pSetMsg        = VoltageSetMsg,
    .xSet           = 197,
    .yValue         = 54,
    .color          = 0xeb0c,
    .yUnderlineBase = 84,
    .xDigit         = digit_x_main,
    .bar            = &volt_bar_main,
    .isVoltage      = 1,
    .isGraph        = 0,
    .spacing        = 2
};

/**
 * @brief 电流编辑上下文（主界面）
 */
static const DigitEditContext_t curr_ctx = {
    .pTarget        = &Target_Current,
    .pSetMsg        = Current_SetMsg,
    .xSet           = 197,
    .yValue         = 106,
    .color          = 0x5cbd,
    .yUnderlineBase = 136,
    .xDigit         = digit_x_main,
    .bar            = &curr_bar_main,
    .isVoltage      = 0,
    .isGraph        = 0,
    .spacing        = 2
};

/**
 * @brief 电压编辑上下文（曲线页）
 */
static const DigitEditContext_t graph_volt_ctx = {
    .pTarget        = &Target_Voltage,
    .pSetMsg        = VoltageSetMsg,
    .xSet           = 106,
    .yValue         = 37,
    .color          = 0xeb0c,
    .yUnderlineBase = 67,
    .xDigit         = digit_x_graph,
    .bar            = &volt_bar_graph,
    .isVoltage      = 1,
    .isGraph        = 1,
    .spacing        = 1
};

/**
 * @brief 电流编辑上下文（曲线页）
 */
static const DigitEditContext_t graph_curr_ctx = {
    .pTarget        = &Target_Current,
    .pSetMsg        = Current_SetMsg,
    .xSet           = 106,
    .yValue         = 76,
    .color          = 0x5cbd,
    .yUnderlineBase = 106,
    .xDigit         = digit_x_graph,
    .bar            = &curr_bar_graph,
    .isVoltage      = 0,
    .isGraph        = 1,
    .spacing        = 1
};

/* ========================== 函数声明 ========================== */
// 绘制相关函数
static void DrawDpsMainBasicElement(void); // 绘制界面基础元素
static void DrawQuickSetPage(void); // 绘制快速设置页面

// 数据更新函数
static void UpdateLableValues(void); // 更新界面数值显示

// 页面处理函数
void dps_main_page_handler(KeyEventMsg_t msg);  // 主界面按键处理
void volt_digit_handler(KeyEventMsg_t msg);     // 电压数位编辑按键处理
void curr_digit_handler(KeyEventMsg_t msg);     // 电流数位编辑按键处理
void quick_menu_handler(KeyEventMsg_t msg);     // 快速设置菜单按键处理
void dps_graph_handler(KeyEventMsg_t msg);      // 曲线显示界面按键处理

// 页面刷新函数
void dps_main_page_refresh_handler(void);       // 主界面刷新函数
void dsp_graph_page_refresh_handler(void);      // 曲线界面刷新函数
void dps_digit_page_refresh_handler(void);      // 数位编辑页刷新（按来源页面分派）

// 格式化工具函数
void formatFloatMessage(float voltage, float current, char *message);
void formatTempMessage(float temp, char *message);
void formatOutputMessage(float value, char *message);
static void formatGraphValue(float value, char unit, char *message);   // 曲线页实测值（数值+单位）

// ADC相关函数
void CB_DPS_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);  // ADC转换完成回调
void DPS_ADC_Init(void);                                    // ADC初始化
void DPS_ADC_DeInit(void);                                  // ADC反初始化

// 任务相关函数
void Start_DpsCoreTask(void *argument);                     // DPS核心任务入口
void Start_PIDTask(void *argument);                         // PID运算子任务入口

// 通用辅助函数
static void digit_page_handler(KeyEventMsg_t msg, const DigitEditContext_t *ctx);   // 数位编辑通用处理
static void apply_quick_set_by_index(uint8_t index);                                // 应用指定索引的快速设置参数

/* ========================== 页面处理函数回调表 ========================== */
/**
 * @brief 页面处理函数表驱动
 * @note  索引对应AppPage_t枚举，快速映射页面与处理函数
 */
static void (*page_handlers[PAGE_NUM])(KeyEventMsg_t) = {
    [PAGE_MAIN] = dps_main_page_handler,
    [PAGE_VOLT_DIGIT] = volt_digit_handler,
    [PAGE_CURR_DIGIT] = curr_digit_handler,
    [PAGE_QUICK_MENU] = quick_menu_handler,
    [PAGE_MAIN_GRAPH] = dps_graph_handler
};

/* ========================== 页面刷新函数回调表 ========================== */
/**
 * @brief 页面刷新函数表驱动
 * @note  索引对应AppPage_t枚举，快速映射页面与刷新函数
 */
static void (*page_refreshers[PAGE_NUM])(void) = {
    [PAGE_MAIN] = dps_main_page_refresh_handler,
    [PAGE_VOLT_DIGIT] = dps_digit_page_refresh_handler,
    [PAGE_CURR_DIGIT] = dps_digit_page_refresh_handler,
    [PAGE_QUICK_MENU] = dps_main_page_refresh_handler,
    [PAGE_MAIN_GRAPH] = dsp_graph_page_refresh_handler
};

/* ========================== 格式化工具函数实现 ========================== */
/**
 * @brief  格式化电压+电流为显示字符串（格式：XX.XXV XX.XXA）
 * @param  voltage: 电压值（V）
 * @param  current: 电流值（A）
 * @param  message: 输出字符串缓冲区
 * @retval 无
 */
void formatFloatMessage(float voltage, float current, char *message) {
    char *p = message;
    p += fmt_fixed_digits(voltage, 3, p);
    *p++ = 'V';
    *p++ = ' ';
    p += fmt_fixed_digits(current, 3, p);
    *p++ = 'A';
    *p = '\0';
}

/**
 * @brief  格式化温度为显示字符串（格式：XX.XC）
 * @param  temp: 温度值（℃）
 * @param  message: 输出字符串缓冲区
 * @retval 无
 */
void formatTempMessage(float temp, char *message) {
    char *p = message;
    p += fmt_fixed_digits(temp, 3, p);
    *p++ = 'C';
    *p = '\0';
}

/**
 * @brief  格式化输出值为显示字符串（通用型，4位总长度）
 * @param  value: 待格式化数值
 * @param  message: 输出字符串缓冲区
 * @retval 无
 */
void formatOutputMessage(float value, char *message) {
    char *p = message;
    p += fmt_fixed_digits(value, 4, p);
    *p = '\0';
}

/**
 * @brief  格式化曲线页的实测值（格式：XX.XXV，与主界面同样的有效位数）
 * @param  value: 待格式化数值
 * @param  unit: 单位字符（V/A）
 * @param  message: 输出字符串缓冲区（至少 7 字节）
 * @retval 无
 */
static void formatGraphValue(float value, char unit, char *message) {
    char *p = message;
    p += fmt_fixed_digits(value, 4, p);
    *p++ = unit;
    *p = '\0';
}

/* ========================== ADC相关函数实现 ========================== */
/**
 * @brief  按 TIM8 的实际配置刷新能量积分步长
 * @note   ADC 回调每处理 64 对采样触发一次，采样间隔 = (PSC+1)*(ARR+1)/TIM8_CLK，
 *        原先固定用 1ms 的 MS_TO_HOUR 会把能量算高（实际 ≈857µs 一次）。
 *        TIM8 挂在 APB2 上，APB2 预分频不为 1 时定时器时钟为 PCLK2 的 2 倍。
 * @retval 无
 */
static void DPS_UpdateEnergyPeriod(void) {
    // 直接读寄存器：PSC/ARR 在运行期被 __HAL_TIM_SET_xxx 改写，Init 结构体里的值并不可靠
    uint32_t psc = (uint32_t)htim8.Instance->PSC + 1U;
    uint32_t arr = (uint32_t)htim8.Instance->ARR + 1U;

    uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
    uint32_t ppre2 = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    uint32_t tim_clk = (ppre2 < 4U) ? pclk2 : (pclk2 * 2U);   // 0xx: 不分频，1xx: HCLK/n -> ×2

    double trigger_period_s = (double)psc * (double)arr / (double)tim_clk;   // 单次触发间隔
    energy_dt_hour = trigger_period_s * 64.0 / 3600.0;                       // 一次回调 = 64 对采样
}

/**
 * @brief  ADC转换完成回调函数
 * @param  hadc: ADC句柄指针
 * @retval 无
 * @note   处理ADC1的电压/电流采样数据，计算实时电压、电流、功率
 */
void CB_DPS_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        AdcTim_OFF();                                                            // 关闭定时器（ADC触发源）
        uint32_t sum_ch1 = 0, sum_ch2 = 0;

        // 累加64次ADC采样值（偶数列-通道1(电压)，奇数列-通道2(电流)）
        for (uint16_t i = 0; i < 64; i++) {
            sum_ch1 += dps_adc_raw_buf[2 * i];
            sum_ch2 += dps_adc_raw_buf[2 * i + 1];
        }

        // 计算均值并叠加校准偏移
        int32_t temp1 = (int32_t)(sum_ch1 >> 6) + UserParam.DPS_Voltage_Original;
        int32_t temp2 = (int32_t)(sum_ch2 >> 6) + UserParam.DPS_Current_Original;

        // 超出 ADC 量程一律按满量程处理（原实现回 0，过压时反而会被当成 0V，控制环因此失去保护）
        if (temp1 < 0)    temp1 = 0;
        if (temp1 > 4095) temp1 = 4095;
        if (temp2 < 0)    temp2 = 0;
        if (temp2 > 4095) temp2 = 4095;

        // 转换为实际电压值（3.3V参考，12位ADC，乘以校准系数）
        float raw_voltage = (float) temp1 * 3.3f / 4096.0f * UserParam.DPS_Voltage_Factor;

        // 转换为实际电流值（扣除电压耦合偏移和固定偏移）
        float raw_current = (float) temp2 * 3.3f / 4096.0f * UserParam.DPS_Current_Factor;

        // ==========================================
        //              软件 IIR 低通滤波
        // ==========================================
        // 滤波系数 (0.0 ~ 1.0)，越小滤波越强，反应越慢
        const float filter_display_alpha = 0.02f;       // 显示用小系数，保证平滑
        const float filter_PID_alpha = 0.2f;           // PID用大系数，保证快速响应

        // 静态变量保存上一次的滤波结果
        static float filtered_voltage = 0.0f;           // 滤波后的电压值_显示
        static float filtered_current = 0.0f;           // 滤波后的电流值_显示
        static float filtered_current_pid = 0.0f;       // 滤波后的电流值_PID
        static float filtered_voltage_pid = 0.0f;       // 滤波后的电压值_PID

        // 一阶 IIR 滤波公式：Output = alpha * Input + (1 - alpha) * Output_Last
        filtered_voltage = filter_display_alpha * raw_voltage + (1.0f - filter_display_alpha) * filtered_voltage;
        filtered_current = filter_display_alpha * raw_current + (1.0f - filter_display_alpha) * filtered_current;
        filtered_current_pid = filter_PID_alpha * raw_current + (1.0f - filter_PID_alpha) * filtered_current_pid;
        filtered_voltage_pid = filter_PID_alpha * raw_voltage + (1.0f - filter_PID_alpha) * filtered_voltage_pid;

        // 更新全局变量
        Voltage = filtered_voltage;
        filtered_current < 0.01f ? (Current = 0.0f) : (Current = filtered_current); // 如果电流小于0.01A，就归零，避免静止时跑量
        Current_PID = filtered_current_pid;
        Voltage_PID = filtered_voltage_pid;

        // 计算实时功率
        Power = Voltage * Current;
    }

    // 电源开启时累计能量（功率×时间，步长按 TIM8 实际配置换算成小时）
    // 增量用 double 计算：float 下 5W 负载的增量约 1.2e-6 Wh，累到几 Wh 就会被舍入掉
    if (IsPowerOn) energy += (double)Power * energy_dt_hour;

    AdcTim_ON();                                                             // 开启定时器（ADC触发源）
}

/**
 * @brief  复位控制环并把 DAC 立即写到位
 * @note  必须在打开输出级（DpsPower_ON）之前调用：
 *        控制环状态（尤其恒流模式下的 DAC）是上一次运行遗留的，直接沿用会在开机瞬间输出异常电压。
 */
static void Dps_ResetOutputAndWriteDac(void) {
    PID_ResetOutput();
    // I2C3 上只有 DC 这颗 DAC，但 PID 任务会并发写同一条总线，必须用互斥锁串行化
    if (osMutexAcquire(IIC3_MutexHandle, osWaitForever) == osOK) {
        MCP4725_WriteFast(&hmcp4725_DC, pid_controller.current_dac, MCP4725_MODE_NORMAL);
        osMutexRelease(IIC3_MutexHandle);
    }
}

/* ========================== 数位编辑通用处理函数实现 ========================== */
/**
 * @brief  电压/电流数位编辑通用处理函数
 * @param  msg: 按键事件消息
 * @param  ctx: 数位编辑上下文指针（volt_ctx/curr_ctx）
 * @retval 无
 * @note   统一处理电压/电流的数位选择、数值调整、返回主界面等逻辑
 */
static void digit_page_handler(KeyEventMsg_t msg, const DigitEditContext_t *ctx) {
    // 映射当前/上一次编辑数位（区分电压/电流），量程上下限也按通道区分
    DigitPosition_e *pCurrent = ctx->isVoltage ? &VoltagePosition_Current : &CurrentPosition_Current;
    DigitPosition_e *pPast = ctx->isVoltage ? &VoltagePosition_Past : &CurrentPosition_Past;
    const float limit_max = ctx->isVoltage ? MAX_OUTPUT_VOLTAGE : MAX_OUTPUT_CURRENT;
    const float limit_min = ctx->isVoltage ? MIN_OUTPUT_VOLTAGE : MIN_OUTPUT_CURRENT;

    // 上移/左按：切换到高位数位
    if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_PRESS_LEFT) {
        switch (*pCurrent) {
            case hundredths: *pPast = hundredths;
                *pCurrent = tenths;
                break;
            case tenths: *pPast = tenths;
                *pCurrent = units;
                break;
            case units: *pPast = units;
                *pCurrent = tens;
                break;
            case tens: *pPast = tens;
                *pCurrent = hundredths;
                break;
        }
    }
    // 下移/右按：切换到低位数位
    else if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        switch (*pCurrent) {
            case hundredths: *pPast = hundredths;
                *pCurrent = tens;
                break;
            case tenths: *pPast = tenths;
                *pCurrent = hundredths;
                break;
            case units: *pPast = units;
                *pCurrent = tenths;
                break;
            case tens: *pPast = tens;
                *pCurrent = units;
                break;
        }
    }
    // 右旋转编码器：增加对应数位的数值
    else if (msg.event == ENCODER_EVENT_RIGHT) {
        *ctx->pTarget = fminf(*ctx->pTarget + 0.01f * dps_pow10_table[*pCurrent], limit_max);
        fmt_fixed_00_00(*ctx->pTarget, limit_max, ctx->pSetMsg);
        lcd_draw_string(ctx->xSet, ctx->yValue, ctx->pSetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, ctx->spacing);
        if (ctx->isVoltage) SetTargetVoltage(*ctx->pTarget);
        else SetTargetCurrent(*ctx->pTarget);
        DrawBar(ctx->bar, *ctx->pTarget);
        StartBeezer(0);
        return;
    }
    // 左旋转编码器：减少对应数位的数值
    else if (msg.event == ENCODER_EVENT_LEFT) {
        *ctx->pTarget = fmaxf(*ctx->pTarget - 0.01f * dps_pow10_table[*pCurrent], limit_min);
        fmt_fixed_00_00(*ctx->pTarget, limit_max, ctx->pSetMsg);
        lcd_draw_string(ctx->xSet, ctx->yValue, ctx->pSetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, ctx->spacing);
        if (ctx->isVoltage) SetTargetVoltage(*ctx->pTarget);
        else SetTargetCurrent(*ctx->pTarget);
        DrawBar(ctx->bar, *ctx->pTarget);
        StartBeezer(0);
        return;
    }
    // 短按SET/编码器：返回进入编辑前的页面（主界面或曲线页）
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        current_page = ctx->isGraph ? PAGE_MAIN_GRAPH : PAGE_MAIN;
        previous_page = (ctx->isVoltage ? PAGE_VOLT_DIGIT : PAGE_CURR_DIGIT);
        *pCurrent = *pPast = hundredths;

        // 清除数位下划线：按上下文给出的X表算出左右边界
        uint16_t xs = ctx->xDigit[0];
        uint16_t xe = (uint16_t) (ctx->xDigit[0] + 16);
        for (uint8_t i = 1; i < 4; i++) {
            if (ctx->xDigit[i] < xs) xs = ctx->xDigit[i];
            if ((uint16_t) (ctx->xDigit[i] + 16) > xe) xe = (uint16_t) (ctx->xDigit[i] + 16);
        }
        lcd_draw_rect((uint16_t) (xs - 2), ctx->yUnderlineBase, (uint16_t) (xe + 2),
                      (uint16_t) (ctx->yUnderlineBase + 5), 0x29a7, 1);
        StartBeezer(0);
        return;
    }
    // 无匹配按键事件：直接返回
    else return;

    // 按键有效时蜂鸣提示
    StartBeezer(0);

    // 数位选择下划线更新（切换选中数位的指示）
    const uint16_t *x_pos = ctx->xDigit;    // 百分位/十分位/个位/十位的X坐标
    lcd_draw_round_rect(x_pos[*pPast], ctx->yUnderlineBase, x_pos[*pPast] + 16, ctx->yUnderlineBase + 3, 2, 0x29a7, 1);
    lcd_draw_round_rect(x_pos[*pCurrent], ctx->yUnderlineBase, x_pos[*pCurrent] + 16, ctx->yUnderlineBase + 3, 2,
                        ctx->color, 1);
}

/**
 * @brief  电压数位编辑按键处理（封装通用函数）
 * @param  msg: 按键事件消息
 * @retval 无
 */
void volt_digit_handler(KeyEventMsg_t msg) {
    // 从曲线页进入编辑时用曲线页的上下文（坐标/进度条不同）
    digit_page_handler(msg, (previous_page == PAGE_MAIN_GRAPH) ? &graph_volt_ctx : &volt_ctx);
}

/**
 * @brief  电流数位编辑按键处理（封装通用函数）
 * @param  msg: 按键事件消息
 * @retval 无
 */
void curr_digit_handler(KeyEventMsg_t msg) {
    digit_page_handler(msg, (previous_page == PAGE_MAIN_GRAPH) ? &graph_curr_ctx : &curr_ctx);
}

/* ========================== 快速设置辅助函数实现 ========================== */
/**
 * @brief  应用指定索引的快速设置参数
 * @param  index: 快速设置项索引（0~4对应option_1~option_5）
 * @retval 无
 * @note   根据索引设置目标电压和电流（读取UserParam中的预设值）
 */
static void apply_quick_set_by_index(uint8_t index) {
    switch (index) {
        case 0: SetTargetVoltage(UserParam.DPS_Fs_Voltage_1);
            SetTargetCurrent(UserParam.DPS_Fs_Current_1);
            break;
        case 1: SetTargetVoltage(UserParam.DPS_Fs_Voltage_2);
            SetTargetCurrent(UserParam.DPS_Fs_Current_2);
            break;
        case 2: SetTargetVoltage(UserParam.DPS_Fs_Voltage_3);
            SetTargetCurrent(UserParam.DPS_Fs_Current_3);
            break;
        case 3: SetTargetVoltage(UserParam.DPS_Fs_Voltage_4);
            SetTargetCurrent(UserParam.DPS_Fs_Current_4);
            break;
        case 4: SetTargetVoltage(UserParam.DPS_Fs_Voltage_5);
            SetTargetCurrent(UserParam.DPS_Fs_Current_5);
            break;
    }
}

/* ========================== 核心任务函数实现 ========================== */
/**
 * @brief  DPS核心任务入口函数
 * @param  argument: 任务参数（未使用）
 * @retval 无
 * @note   主循环处理按键事件、定时更新界面数值、运行时间和风扇状态
 */
void Start_DpsCoreTask(void *argument) {
    // 初始挂起任务（等待Resume激活）
    osThreadSuspend(DpsCoreTaskHandle);

    for (;;) {
        // 界面更新计时（每10ms+1）
        widgets_update_time_count++;
        // 时间更新计时（仅电源开启时）
        if (IsPowerOn) Time_update_time_count++;
        // 风扇状态更新计时
        Fan_update_time_count ++;

        // 曲线采样：按系统节拍约 38ms 取一点（266 点 ≈ 10 秒，一列一点）
        // 用节拍而不是循环计数：界面刷新耗时会拖慢循环，否则 10 秒窗口会变成十几秒
        uint32_t graph_now_tick = osKernelGetTickCount();
        if ((uint32_t) (graph_now_tick - graph_sample_tick) >= DPS_GRAPH_SAMPLE_PERIOD_MS) {
            graph_sample_tick = graph_now_tick;
            DpsGraph_Sample(Voltage, Current, Power);
            graph_frame_pending = 1;
        }

        // 读取按键事件并分发到当前页面的处理函数
        if (osMessageQueueGet(KeyEventQueueHandle, &DPS_Keymsg, NULL, 0) == osOK)
            page_handlers[current_page](DPS_Keymsg);

        // 界面刷新函数分发
        page_refreshers[current_page]();

        // 任务延时（10ms）
        osDelay(10);
    }
}

/* ========================== 绘制函数实现 ========================== */
/**
 * @brief  通用进度条绘制（主界面/曲线页共用）
 * @param  bar: 进度条上下文（几何、颜色、量程）
 * @param  value: 当前值
 * @retval 无
 * @note   防抖刷新：仅当填充像素数变化时才重绘。
 *         重画整个轨道后先把 bar->last_px 清 0，可强制重绘一次填充。
 */
static void DrawBar(BarCtx_t *bar, float value) {
    uint16_t span = (uint16_t) (bar->x1 - bar->x0);          // 轨道可用像素宽度
    uint8_t now = (uint8_t) ((float) span * (value / bar->max));

    // 边界值保护
    if (now > span) now = (uint8_t) span;
    if (now < 8) now = 8;

    // 防抖：像素数未变化则不重绘
    if (now == bar->last_px) return;

    // 像素数减少：先清空整个轨道再绘制新值（增加时直接覆盖即可）
    if (now < bar->last_px)
        lcd_draw_round_rect(bar->x0, bar->y0, bar->x1, bar->y1, 4, bar->bg, 1);

    lcd_draw_round_rect(bar->x0, bar->y0, (uint16_t) (bar->x0 + now), bar->y1, 4, bar->color, 1);
    bar->last_px = now;
}

/**
 * @brief  绘制界面基础元素（框架、图标、初始数值）
 * @retval 无
 * @note   初始化界面时调用，绘制静态框架和默认数值
 */
static void DrawDpsMainBasicElement(void) {
    // 绘制顶部/中部/底部背景框
    lcd_draw_rect(0, 0, 319, 31, 0x2946, 1);
    lcd_draw_rect(0, 32, 319, 207, 0x29a7, 1);
    lcd_draw_rect(0, 208, 319, 239, 0x2946, 1);

    // 绘制顶部信息框（输入/温度/风扇）
    lcd_draw_round_rect(5, 3, 139, 26, 5, 0x31e8, 1);
    lcd_draw_round_rect(146, 3, 230, 26, 5, 0x31e8, 1);
    lcd_draw_round_rect(237, 3, 315, 26, 5, 0x31e8, 1);

    // 绘制顶部图标
    lcd_draw_image(7, 5, &img_IN_16x20);
    lcd_draw_image(150, 5, &img_FIRE_16x20);
    lcd_draw_image(241, 7, &img_FAN_16x16);

    // 绘制顶部初始数值
    lcd_draw_string(27, 5, "0.00V 0.00A", &KaiTi16x20, 0xef7d, 0x31e8, -5);
    lcd_draw_string(171, 5, "00.0C", &KaiTi16x20, 0xef7d, 0x31e8, -4);
    lcd_draw_string(262, 5, "000%", &KaiTi16x20, 0xef7d, 0x31e8, -4);

    // 绘制输出电压/电流/功率初始数值
    lcd_draw_string(3, 38, "0.000", &DIN_Medium32x50, 0xeb0c, 0x29a7, 0);
    lcd_draw_string(3, 95, "0.000", &DIN_Medium32x50, 0x5cbd, 0x29a7, 0);
    lcd_draw_string(3, 152, "0.000", &DIN_Medium32x50, 0xdb3f, 0x29a7, 0);

    // 绘制单位（V/A/W）
    lcd_draw_string(150, 64, "V", &JetBrainsMono16x22, 0xeb0c, 0x29a7, 0);
    lcd_draw_string(150, 122, "A", &JetBrainsMono16x22, 0x5cbd, 0x29a7, 0);
    lcd_draw_string(150, 178, "W", &JetBrainsMono16x22, 0xdb3f, 0x29a7, 0);

    // 绘制电压进度条（先清 0 强制重绘填充，避免轨道重画后填充被防抖跳过）
    lcd_draw_round_rect(180, 40, 309, 47, 4, 0x6a49, 1);
    volt_bar_main.last_px = 0;
    DrawBar(&volt_bar_main, Target_Voltage);
    fmt_fixed_00_00(Target_Voltage, MAX_OUTPUT_VOLTAGE, VoltageSetMsg);
    lcd_draw_string(197, 54, VoltageSetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, 2);

    // 绘制电流进度条
    lcd_draw_round_rect(180, 93, 309, 100, 4, 0x3ace, 1);
    curr_bar_main.last_px = 0;
    DrawBar(&curr_bar_main, Target_Current);
    fmt_fixed_00_00(Target_Current, MAX_OUTPUT_VOLTAGE, Current_SetMsg);
    lcd_draw_string(197, 106, Current_SetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, 2);

    // 绘制电源开关/功率模式/快速设置按钮
    const uint16_t pwr_bg = IsPowerOn ? 0x9fec : 0x632c;
    const uint16_t pwr_ft = IsPowerOn ? 0x632c : 0xFFFF;
    const uint16_t mode_bg = PowerMode ? 0x869f : 0xec10;
    const uint16_t mode_ft = PowerMode ? 0x632c : 0xef7d;

    lcd_draw_round_rect(175, 177, 239, 200, 8, pwr_bg, 1);
    lcd_draw_round_rect(247, 177, 311, 200, 8, mode_bg, 1);
    lcd_draw_round_rect(182, 144, 304, 169, 8, 0x4408, 1);

    // 绘制按钮文字（开关状态与功率模式按当前值绘制，从其它页面返回时不会显示旧状态）
    lcd_draw_string(205, 148, "快速设置", &YPLXT18x18, 0xFFFF, 0x4408, 2);
    lcd_draw_string(IsPowerOn ? 193 : 186, 180, IsPowerOn ? "ON" : "OFF",
                    &JetBrainsMono14x18, pwr_ft, pwr_bg, 2);
    lcd_draw_string(264, 180, PowerMode ? "CC" : "CV", &JetBrainsMono14x18, mode_ft, mode_bg, 5);
    PowerMode_last = PowerMode;

    // 绘制当前光标焦点框
    const FocusRect_t *fr = &main_focus_rects[main_page_cursor_current];
    lcd_draw_round_rect(fr->x, fr->y, (uint16_t) (fr->x + fr->w), (uint16_t) (fr->y + fr->h),
                        fr->radius, fr->color, fr->filled);

    // 绘制底部时间/能量背景框
    lcd_draw_round_rect(5, 213, 156, 236, 5, 0x31e8, 1);
    lcd_draw_round_rect(164, 213, 315, 236, 5, 0x31e8, 1);

    // 绘制底部图标
    lcd_draw_image(9, 216, &img_time_18x18);
    lcd_draw_image(168, 215, &img_energy_16x20);

    // 绘制底部数值（时间/能量按当前值绘制）
    fmt_hms(running_time, TimeMsg);
    lcd_draw_string(33, 215, TimeMsg, &KaiTi16x20, 0xFFFF, 0x31e8, -3);
    fmt_energy_wh(DPS_GetEnergy(), EnergyMsg);
    lcd_draw_string(188, 215, EnergyMsg, &KaiTi16x20, 0xFFFF, 0x31e8, -3);

    // 界面已回到初始值/当前值，让"变化才刷新"的影子串失效，保证下一帧必定刷新
    last_InputMsg[0] = '\0';
    last_TempMsg[0] = '\0';
    last_VoltageMsg[0] = '\0';
    last_CurrentMsg[0] = '\0';
    last_PowerMsg[0] = '\0';
}

/**
 * @brief  绘制快速设置页面
 * @retval 无
 * @note   绘制5个快速设置项，读取UserParam中的预设值并显示
 */
static void DrawQuickSetPage(void) {
    // 绘制快速设置页面背景框
    lcd_draw_rect(0, 32, 149, 207, 0x29a7, 1);
    lcd_draw_round_rect(0, 32, 149, 207, 10, 0xFFFF, 0);

    // 快速设置项表驱动
    static const QuickSetItem_t items[5] = {
        {0, 0, set1, 41},
        {0, 0, set2, 75},
        {0, 0, set3, 109},
        {0, 0, set4, 143},
        {0, 0, set5, 177}
    };

    // 绘制5个快速设置项
    for (uint8_t i = 0; i < 5; i++) {
        // 读取对应索引的预设电压/电流
        float v = (i == 0)
                      ? UserParam.DPS_Fs_Voltage_1
                      : (i == 1)
                            ? UserParam.DPS_Fs_Voltage_2
                            : (i == 2)
                                  ? UserParam.DPS_Fs_Voltage_3
                                  : (i == 3)
                                        ? UserParam.DPS_Fs_Voltage_4
                                        : UserParam.DPS_Fs_Voltage_5;
        float c = (i == 0)
                      ? UserParam.DPS_Fs_Current_1
                      : (i == 1)
                            ? UserParam.DPS_Fs_Current_2
                            : (i == 2)
                                  ? UserParam.DPS_Fs_Current_3
                                  : (i == 3)
                                        ? UserParam.DPS_Fs_Current_4
                                        : UserParam.DPS_Fs_Current_5;

        // 计算项的Y坐标
        uint16_t y_top = 38 + i * 34;
        // 选中项高亮（第1项默认选中）
        uint16_t color = (i == 0) ? 0x75ff : 0x632c;

        // 格式化显示字符串
        formatFloatMessage(v, c, items[i].buf);
        // 绘制项背景框
        lcd_draw_round_rect(5, y_top, 144, y_top + 25, 8, color, 1);
        // 绘制项文字
        lcd_draw_string(16, items[i].y, items[i].buf, &yahei16x20, 0xef7d, color, -4);
    }
}

/* ========================== 数值更新函数实现 ========================== */
/**
 * @brief  更新状态栏信息（输入电压/电流 + 温度）
 * @retval 无
 * @note   主界面与曲线页的状态栏几何完全一致，因此这段刷新逻辑共用
 */
static void UpdateStatusBarInfo(void) {
    // 更新输入电压/电流显示（仅数值变化时）
    formatFloatMessage(Input_Voltage, Input_Current, InputMsg);
    if (strcmp(InputMsg, last_InputMsg) != 0) {
        lcd_draw_string(27, 5, InputMsg, &KaiTi16x20, 0xef7d, 0x31e8, -5);
        strcpy(last_InputMsg, InputMsg);
    }

    // 更新温度显示
    formatTempMessage(DCDC_Temperature, TempMsg);
    if (strcmp(TempMsg, last_TempMsg) != 0) {
        uint16_t col = (DCDC_Temperature <= 60.0f) ? 0xef7d : 0xf980;
        lcd_draw_string(171, 5, TempMsg, &KaiTi16x20, col, 0x31e8, -4);
        strcpy(last_TempMsg, TempMsg);
    }
}

/**
 * @brief  更新主界面数值显示
 * @retval 无
 * @note   仅当数值变化时才重绘，避免频繁刷新LCD
 */
static void UpdateLableValues(void) {
    // 状态栏（输入电压/电流 + 温度）
    UpdateStatusBarInfo();

    // 更新输出数据显示（仅刷新使能时）
    if (Flush_OutputData) {
        formatOutputMessage(Current, CurrentMsg);
        formatOutputMessage(Voltage, VoltageMsg);
        formatOutputMessage(Power, PowerMsg);

        // 更新输出电流显示（仅数值变化时）
        if (strcmp(CurrentMsg, last_CurrentMsg) != 0) {
            lcd_draw_string(3, 95, CurrentMsg, &DIN_Medium32x50, 0x5cbd, 0x29a7, 0);
            strcpy(last_CurrentMsg, CurrentMsg);
        }

        // 更新输出电压显示（仅数值变化时）
        if (strcmp(VoltageMsg, last_VoltageMsg) != 0) {
            lcd_draw_string(3, 38, VoltageMsg, &DIN_Medium32x50, 0xeb0c, 0x29a7, 0);
            strcpy(last_VoltageMsg, VoltageMsg);
        }

        // 更新输出功率显示（仅数值变化时）
        if (strcmp(PowerMsg, last_PowerMsg) != 0) {
            lcd_draw_string(3, 152, PowerMsg, &DIN_Medium32x50, 0xdb3f, 0x29a7, 0);
            strcpy(last_PowerMsg, PowerMsg);
        }
    }

    // 更新能量显示（仅电源开启时）
    if (IsPowerOn) {
        fmt_energy_wh(DPS_GetEnergy(), EnergyMsg);
        lcd_draw_string(188, 215, EnergyMsg, &KaiTi16x20, 0xFFFF, 0x31e8, -3);
    }

    // 更新功率模式显示（仅模式变化时）
    if (PowerMode != PowerMode_last) {
        uint16_t bg = PowerMode ? 0x869f : 0xec10;
        uint16_t ft = PowerMode ? 0x632c : 0xef7d;
        lcd_draw_round_rect(247, 177, 311, 200, 8, bg, 1);
        lcd_draw_string(264, 180, PowerMode ? "CC" : "CV", &JetBrainsMono14x18, ft, bg, 5);
        PowerMode_last = PowerMode;
    }
}

/* ========================== 页面处理函数实现 ========================== */
/**
 * @brief  主界面按键处理函数
 * @param  msg: 按键事件消息
 * @retval 无
 * @note   处理主界面的光标移动、进入编辑、电源开关、切换APP等逻辑
 */
void dps_main_page_handler(KeyEventMsg_t msg) {
    // 上移/左按：光标向上切换（Voltage→Setting→Current→Voltage）
    if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT || msg.event == ENCODER_EVENT_PRESS_LEFT) {
        main_page_cursor_past = main_page_cursor_current;
        main_page_cursor_current = ((int)main_page_cursor_current == 0)
                                    ? (MAIN_NUM - 1)
                                    : (MainPageCursor_e)((int)main_page_cursor_current - 1);
    }
    // 下移/右按：光标向下切换（Voltage→Current→Setting→Voltage）
    else if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT || msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        main_page_cursor_past = main_page_cursor_current;
        main_page_cursor_current = (main_page_cursor_current + 1 >= MAIN_NUM)
                            ? Main_Voltage
                            : main_page_cursor_current + 1;
    }
    // 短按SET/编码器：进入选中项的编辑页面
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (main_page_cursor_current == Main_Voltage) {
            // 进入电压数位编辑页面
            current_page = PAGE_VOLT_DIGIT;
            previous_page = PAGE_MAIN;
            lcd_draw_round_rect(279, 84, 295, 87, 2, 0xeb0c, 1);
        } else if (main_page_cursor_current == Main_Current) {
            // 进入电流数位编辑页面
            current_page = PAGE_CURR_DIGIT;
            previous_page = PAGE_MAIN;
            lcd_draw_round_rect(279, 136, 295, 139, 2, 0x5cbd, 1);
        } else if (main_page_cursor_current == Main_Setting) {
            // 进入快速设置页面
            current_page = PAGE_QUICK_MENU;
            previous_page = PAGE_MAIN;
            Flush_OutputData = false;
            DrawQuickSetPage();
        }
    }
    // 长按SET：电源开关
    else if (msg.key == KEY_SET && msg.event == KEY_EVENT_LONG_PRESS) {
        IsPowerOn = !IsPowerOn;
        if (IsPowerOn) {
            // 电源开启：更新UI + 硬件控制
            lcd_draw_round_rect(175, 177, 239, 200, 8, 0x9fec, 1);
            lcd_draw_string(193, 180, "ON", &JetBrainsMono14x18, 0x632c, 0x9fec, 2);
            Dps_ResetOutputAndWriteDac();   // 先把 DAC 写回目标电压位置，再打开输出级
            DpsPower_ON();
            osDelay(10);
            DpsRelease_OFF();
            DpsGnd_ON();
            DpsDc_ON();
        } else {
            // 电源关闭：更新UI + 硬件控制
            lcd_draw_round_rect(175, 177, 239, 200, 8, 0x632c, 1);
            lcd_draw_string(186, 180, "OFF", &JetBrainsMono14x18, 0xFFFF, 0x632c, 2);
            DpsPower_OFF();
            DpsRelease_ON();
            osDelay(80);
            DpsDc_OFF();
            DpsGnd_OFF();
        }
    }
    // 长按编码器：切换到LVGL APP
    else if (msg.key == KEY_ENCODER && msg.event == KEY_EVENT_LONG_PRESS) {
        IsPowerOn = false;

        AppListType APP = APP_LVGL;
        StartBeezer(0);
        osMessageQueuePut(AppSwitchQueueHandle, &APP, 0, 10);
        return;
    }
    // 单击设置键，切换到曲线显示界面
    else if (msg.key == KEY_MODE && msg.event == KEY_EVENT_CLICK) {
        current_page = PAGE_MAIN_GRAPH;
        previous_page = PAGE_MAIN;
        graph_cursor_current = graph_cursor_past = Main_Voltage;   // 曲线页只在上两项间移动
        DrawDpsGraphPage();                                        // 曲线页需要自己画整套界面
        StartBeezer(0);
        return;
    }




    // 无匹配按键事件：直接返回
    else return;

    // 按键有效时蜂鸣提示
    StartBeezer(0);

    // 更新焦点框显示（表驱动方式）
    const FocusRect_t *old_r = &main_focus_rects[main_page_cursor_past];
    const FocusRect_t *new_r = &main_focus_rects[main_page_cursor_current];

    // 清除上一个焦点框
    lcd_draw_round_rect(old_r->x, old_r->y, old_r->x + old_r->w, old_r->y + old_r->h,
                        old_r->radius, 0x29a7, old_r->filled);
    // 绘制新焦点框
    lcd_draw_round_rect(new_r->x, new_r->y, new_r->x + new_r->w, new_r->y + new_r->h,
                        new_r->radius, new_r->color, new_r->filled);
}

/**
 * @brief  快速设置菜单按键处理函数
 * @param  msg: 按键事件消息
 * @retval 无
 * @note   处理快速设置菜单的选项切换、应用、返回等逻辑
 */
void quick_menu_handler(KeyEventMsg_t msg) {
    // 上移/左按：选项向上切换（option_1→option_5→option_4→...）
    if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT || msg.event == ENCODER_EVENT_PRESS_LEFT) {
        previous_option = current_option;
        current_option = (current_option == option_1) ? option_5 : (FastSetOption_e) (current_option - 1);
    }
    // 下移/右按：选项向下切换（option_1→option_2→...→option_5→option_1）
    else if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT || msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        previous_option = current_option;
        current_option = (current_option == option_5) ? option_1 : (FastSetOption_e) (current_option + 1);
    }
    // 短按SET/编码器：应用选中的快速设置项并返回主界面
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        apply_quick_set_by_index((uint8_t) current_option);
        StartBeezer(0);
        current_option = previous_option = option_1;
        current_page = PAGE_MAIN;
        previous_page = PAGE_QUICK_MENU;

        // 更新电压设置显示和进度条
        fmt_fixed_00_00(Target_Voltage, MAX_OUTPUT_VOLTAGE, VoltageSetMsg);
        lcd_draw_string(197, 54, VoltageSetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, 2);
        DrawBar(&volt_bar_main, Target_Voltage);

        // 更新电流设置显示和进度条
        fmt_fixed_00_00(Target_Current, MAX_OUTPUT_VOLTAGE, Current_SetMsg);
        lcd_draw_string(197, 106, Current_SetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, 2);
        DrawBar(&curr_bar_main, Target_Current);

        // 恢复主界面输出数据刷新
        lcd_draw_rect(0, 32, 149, 207, 0x29a7, 1);
        lcd_draw_string(3, 95, CurrentMsg, &DIN_Medium32x50, 0x5cbd, 0x29a7, 0);
        lcd_draw_string(3, 38, VoltageMsg, &DIN_Medium32x50, 0xeb0c, 0x29a7, 0);
        lcd_draw_string(3, 152, PowerMsg, &DIN_Medium32x50, 0xdb3f, 0x29a7, 0);
        Flush_OutputData = true;
        return;
    }
    // 长按SET/编码器：直接返回主界面（不应用设置）
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_LONG_PRESS) {
        StartBeezer(0);
        current_option = previous_option = option_1;
        current_page = PAGE_MAIN;
        previous_page = PAGE_QUICK_MENU;

        // 恢复电压/电流设置显示
        fmt_fixed_00_00(Target_Voltage, MAX_OUTPUT_VOLTAGE, VoltageSetMsg);
        lcd_draw_string(197, 54, VoltageSetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, 2);
        fmt_fixed_00_00(Target_Current, MAX_OUTPUT_VOLTAGE, Current_SetMsg);
        lcd_draw_string(197, 106, Current_SetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, 2);

        // 恢复主界面输出数据刷新
        lcd_draw_rect(0, 32, 149, 207, 0x29a7, 1);
        lcd_draw_string(3, 95, CurrentMsg, &DIN_Medium32x50, 0x5cbd, 0x29a7, 0);
        lcd_draw_string(3, 38, VoltageMsg, &DIN_Medium32x50, 0xeb0c, 0x29a7, 0);
        lcd_draw_string(3, 152, PowerMsg, &DIN_Medium32x50, 0xdb3f, 0x29a7, 0);
        Flush_OutputData = true;
        return;
    }
    // 无匹配按键事件：直接返回
    else return;

    // 按键有效时蜂鸣提示
    StartBeezer(0);

    // 更新快速设置选项的选中状态显示
    static const QuickSetItem_t items[5] = {
        {0, 0, set1, 41}, {0, 0, set2, 75}, {0, 0, set3, 109}, {0, 0, set4, 143}, {0, 0, set5, 177}
    };

    // 刷新当前选中项和上一项的显示
    for (uint8_t sel = 0; sel < 2; sel++) {
        uint8_t i = (sel == 0) ? (uint8_t) current_option : (uint8_t) previous_option;
        uint16_t color = (sel == 0) ? 0x75ff : 0x632c;
        uint16_t y_top = 38 + i * 34;

        // 读取对应索引的预设电压/电流
        float v = (i == 0)
                      ? UserParam.DPS_Fs_Voltage_1
                      : (i == 1)
                            ? UserParam.DPS_Fs_Voltage_2
                            : (i == 2)
                                  ? UserParam.DPS_Fs_Voltage_3
                                  : (i == 3)
                                        ? UserParam.DPS_Fs_Voltage_4
                                        : UserParam.DPS_Fs_Voltage_5;
        float c = (i == 0)
                      ? UserParam.DPS_Fs_Current_1
                      : (i == 1)
                            ? UserParam.DPS_Fs_Current_2
                            : (i == 2)
                                  ? UserParam.DPS_Fs_Current_3
                                  : (i == 3)
                                        ? UserParam.DPS_Fs_Current_4
                                        : UserParam.DPS_Fs_Current_5;

        // 格式化显示字符串并更新UI
        formatFloatMessage(v, c, items[i].buf);
        lcd_draw_round_rect(5, y_top, 144, y_top + 25, 8, color, 1);
        lcd_draw_string(16, items[i].y, items[i].buf, &yahei16x20, 0xef7d, color, -4);
    }
}

// ========================================
// 初始化与反初始化、挂起与恢复函数
// 功能概述：
// 1. ADC初始化/反初始化：配置ADC1双通道采集（电压/电流），配合DMA环形模式实现高速数据采集
// 2. 任务挂起/恢复：安全停止/重启DPS核心任务，包含电源、外设、线程的有序操作
// 3. PID任务：实时计算电流PID并更新DAC输出，实现恒流控制
// ========================================

/**
 * @brief  DPS系统ADC初始化函数
 * @note   配置ADC1采集通道9/8（对应GPIOB0/GPIOB1），用于电压/电流模拟量采集
 *         - 触发源：定时器8 TRGO事件上升沿
 *         - 采集模式：扫描模式+DMA环形模式，双通道连续采集
 *         - 分辨率：12位，采样时间144周期（保证采集精度）
 * @retval 无
 */
void DPS_ADC_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};           // ADC通道配置结构体初始化
    GPIO_InitTypeDef GPIO_InitStruct = {0};         // GPIO配置结构体初始化

    // -------------------------- ADC1 基础配置 --------------------------
    hadc1.Instance = ADC1;                                              // 选择ADC1外设
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;               // ADC时钟分频：PCLK/2（保证ADC时钟不超过最大允许值）
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;                         // 12位分辨率（4096级，满足电压/电流采集精度要求）
    hadc1.Init.ScanConvMode = ENABLE;                                   // 使能扫描模式（支持多通道采集）
    hadc1.Init.ContinuousConvMode = DISABLE;                            // 关闭连续转换（由外部触发控制采集时机）
    hadc1.Init.DiscontinuousConvMode = DISABLE;                         // 关闭间断模式
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;  // 外部触发边沿：上升沿
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T8_TRGO;         // 触发源：定时器8 TRGO事件
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;                         // 数据右对齐（常规使用方式）
    hadc1.Init.NbrOfConversion = 2;                                     // 转换通道数：2个（电压+电流）
    hadc1.Init.DMAContinuousRequests = ENABLE;                          // 使能DMA连续请求（环形模式必需）
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;                         // 序列转换完成后触发EOC中断
    if (HAL_ADC_Init(&hadc1) != HAL_OK)                                 // ADC初始化，失败则进入错误处理
    {
        Error_Handler();
    }

    // -------------------------- ADC通道配置 --------------------------
    // 通道9（GPIOB0）：电压采集，优先级1
    sConfig.Channel = ADC_CHANNEL_9;                            // 选择通道9
    sConfig.Rank = 1;                                           // 优先级1
    sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;            // 采样时间144周期
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {    // 初始化gpio
        Error_Handler();
    }

    // 通道8（GPIOB1）：电流采集，优先级2
    sConfig.Channel = ADC_CHANNEL_8;                            // 选择通道8
    sConfig.Rank = 2;                                           // 优先级2
    sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;            // 采样时间144周期
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {    // 初始化gpio
        Error_Handler();
    }

    // -------------------------- GPIO 初始化 --------------------------
    __HAL_RCC_ADC1_CLK_ENABLE();                                // 使能ADC1时钟
    __HAL_RCC_GPIOB_CLK_ENABLE();                               // 使能GPIOB时钟（ADC通道对应引脚）
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;              // ADC通道8/9对应引脚
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;                    // 模拟输入模式
    GPIO_InitStruct.Pull = GPIO_NOPULL;                         // 无上下拉
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // -------------------------- DMA 配置 --------------------------
    hdma_adc1.Instance = DMA2_Stream4;                              // DMA2流4（ADC1默认DMA通道）
    hdma_adc1.Init.Channel = DMA_CHANNEL_0;                         // DMA通道0
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;                // 数据方向：外设到内存（ADC->RAM）
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;                    // 外设地址不递增（ADC数据寄存器固定）
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;                        // 内存地址递增（存储到缓冲区数组）
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;   // 外设数据对齐：半字（16位）
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;      // 内存数据对齐：半字（16位）
    hdma_adc1.Init.Mode = DMA_CIRCULAR;                             // 环形模式（循环采集，无需重复启动）
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;                    // 高优先级（保证ADC数据不丢失）
    hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;                 // 关闭FIFO模式
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) {
        Error_Handler();
    }

    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);   // 关联ADC1和DMA句柄，使ADC触发DMA传输
}

/**
 * @brief  DPS系统ADC反初始化函数
 * @note   安全停止ADC采集，释放ADC、GPIO、DMA相关资源
 * @retval 无
 */
void DPS_ADC_DeInit(void) {
    HAL_ADC_Stop(&hadc1);                               // 停止ADC转换
    HAL_ADC_Stop_DMA(&hadc1);                           // 停止ADC DMA传输
    // 等待DMA传输完成（超时100ms），避免数据传输中断导致异常
    HAL_DMA_PollForTransfer(&hdma_adc1, HAL_DMA_FULL_TRANSFER, 100);

    HAL_ADC_DeInit(&hadc1);                                     // ADC外设反初始化
    __HAL_RCC_ADC1_CLK_DISABLE();                               // 关闭ADC1时钟
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_0 | GPIO_PIN_1);    // 释放ADC对应GPIO引脚
    HAL_DMA_DeInit(&hdma_adc1);                                 // DMA反初始化
}

/**
 * @brief  挂起DPS核心任务
 * @note   安全停止DPS系统运行，执行顺序：
 *         1. 关闭电源输出相关外设
 *         2. 反初始化ADC（释放资源）
 *         3. 挂起相关线程（PID任务、输入检测任务、核心任务）
 * @retval 无
 */
void Suspend_DpsCoreTask(void) {
    // 关闭电源输出，确保硬件安全
    DpsPower_OFF();         // 关闭主电源
    DpsRelease_ON();        // 使能释放保护
    osDelay(100);      // 等待泄放
    DpsDc_OFF();            // 关闭DC输出
    DpsGnd_OFF();           // 关闭接地回路


    DPS_ADC_DeInit();       // 反初始化ADC，释放外设资源

    // 挂起相关任务，停止业务逻辑
    osThreadSuspend(PIDTaskHandle);         // 挂起PID运算任务
    Suspend_IndevDetectTask();              // 挂起输入设备检测任务
    osThreadSuspend(DpsCoreTaskHandle);     // 挂起DPS核心任务
}

/**
 * @brief  恢复DPS核心任务
 * @note   重启DPS系统，恢复初始化状态，执行顺序：
 *         1. 初始化硬件状态（电源默认关闭）
 *         2. 重置UI/状态变量
 *         3. 重新初始化ADC
 *         4. 绘制基础UI，启动ADC采集
 *         5. 初始化PID，恢复线程运行
 * @retval 无
 */
void Resume_DpsCoreTask(void) {
    // 初始化电源状态
    DpsPower_OFF();
    DpsRelease_ON();

    // 重置UI和状态变量，恢复初始界面
    Voltage = 0;                            // 实时输出电压（单位：V）
    Current = 0;                            // 实时输出电流_显示（单位：A）
    Current_PID = 0;                        // 实时输出电流_PID（单位：A）  此变量也用于校准
    Power = 0;                              // 实时输出功率（单位：W）
    energy = 0;                             // 累计输出能量（单位：Wh）
    running_time = 0;                       // 电源运行时长（单位：秒）
    widgets_update_time_count = 0;          // 界面控件更新计时（每10ms+1，20次=200ms刷新）
    Time_update_time_count = 0;             // 时间更新计时（每10ms+1，100次=1s刷新）
    Fan_update_time_count = 0;              // 风扇占空比更新计时（每10ms+1，100次=1s刷新）
    IsPowerOn = false;                      // 电源开启状态（默认关闭）

    current_page = previous_page = PAGE_MAIN;                           // 恢复主界面
    main_page_cursor_current = main_page_cursor_past = Main_Voltage;    // 光标默认选中电压
    VoltagePosition_Current = VoltagePosition_Past = hundredths;        // 电压编辑位默认百分位
    CurrentPosition_Current = CurrentPosition_Past = hundredths;        // 电流编辑位默认百分位
    Flush_OutputData = true;                                            // 使能输出数据刷新


    DpsGraph_Reset();           // 清空曲线采样缓冲
    graph_sample_tick = osKernelGetTickCount();
    graph_frame_pending = 1;

    DPS_ADC_Init();             // 重新初始化ADC
    DrawDpsMainBasicElement();     // 绘制基础UI界面
    osDelay(150);          // 延时确保UI绘制完成

    // 注册ADC转换完成回调函数，用于处理采集数据
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_COMPLETE_CB_ID, CB_DPS_ADC_ConvCpltCallback);
    // 启动ADC DMA采集（缓冲区长度128，环形模式）
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *) dps_adc_raw_buf, 128);

    AdcTim_OFF();
    __HAL_TIM_SET_PRESCALER(&htim8, 0);   // 设定更新频率64Khz
    __HAL_TIM_SET_AUTORELOAD(&htim8, 2249);
    DPS_UpdateEnergyPeriod();             // 按刚下发的 TIM8 配置刷新能量积分步长
    AdcTim_ON();                                                            // 启动定时器（ADC触发源）

    PID_Init();                                                             // 初始化PID控制器
    SetTargetVoltage(5.0f);                                                 // 设置默认目标电压5.0V

    // 恢复任务运行，重启业务逻辑
    osThreadResume(DpsCoreTaskHandle);          // 恢复DPS核心任务
    Resume_IndevDetectTask();                   // 恢复输入设备检测任务
    osThreadResume(PIDTaskHandle);              // 恢复PID运算任务
}

// ========================================
// PID运算子任务
// ========================================
/**
 * @brief  PID运算任务函数
 * @note   实时执行电流PID计算，输出控制值到DAC（MCP4725）
 *         - 任务周期：1ms（osDelay(1)）
 *         - 仅在电源开启（IsPowerOn）时执行PID运算
 * @param  argument: 任务入参（未使用）
 * @retval 无
 */
void Start_PIDTask(void *argument) {
    osThreadSuspend(PIDTaskHandle);     // 任务启动后先挂起，等待Resume触发

    // DAC 写缓存：MCP4725 会保持上次写入的输出，值没变就不必再走一次 I2C 事务。
    // 原先每 1ms 无条件写一次（400kHz 下 3 字节 ≈ 70µs，约占 7% CPU）。
    uint16_t last_dac_value = 0;
    bool     last_dac_valid = false;

    for (;;) {
        // 仅当电源开启时执行PID运算和DAC更新
        if (IsPowerOn == true) {
            // 计算电流PID输出值（目标电流-实际电流的闭环调节）
            uint16_t DAC_Value = PID_Calculate(Current_PID,Voltage_PID);
            // 仅在 DAC 值变化（或缓存被外部失效）时才真正写 I2C
            if ((last_dac_valid == false) || (DAC_Value != last_dac_value) || (pid_dac_force_write != 0)) {
                // 与校准/复位路径共用 I2C3 总线，加锁避免两个任务的事务互相打断
                HAL_StatusTypeDef st = HAL_BUSY;
                if (osMutexAcquire(IIC3_MutexHandle, osWaitForever) == osOK) {
                    st = MCP4725_WriteFast(&hmcp4725_DC, DAC_Value, MCP4725_MODE_NORMAL);
                    osMutexRelease(IIC3_MutexHandle);
                }
                if (st == HAL_OK) {
                    last_dac_value = DAC_Value;
                    last_dac_valid = true;
                    pid_dac_force_write = 0;
                }
            }
        } else {
            // 电源关闭后重新开启时，必须重新写一次
            last_dac_valid = false;
        }
        osDelay(1); // 任务周期1ms
    }
}

// ========================= 曲线显示页面 ======================== //

/**
 * @brief  绘制曲线页状态栏右侧的电源开关与功率模式
 * @note   只在状态变化时重绘；切页时由 DrawDpsGraphPage() 把缓存置为无效
 */
static void DrawGraphPowerChip(void) {
    uint8_t on = IsPowerOn ? 1 : 0;
    uint8_t mode = PowerMode ? 1 : 0;

    if (on == graph_chip_on && mode == graph_chip_mode) return;

    graph_chip_on = on;
    graph_chip_mode = mode;

    lcd_draw_round_rect(237, 3, 315, 26, 5, 0x31e8, 1);
    lcd_draw_string(240, 5, on ? "ON" : "OFF", &JetBrainsMono14x18,
                    on ? 0x9fec : 0x7bef, 0x31e8, -2);
    lcd_draw_string(286, 5, mode ? "CC" : "CV", &JetBrainsMono14x18,
                    mode ? 0xf980 : 0xef7d, 0x31e8, -2);
}

/**
 * @brief  刷新曲线页的实测电压/电流（含状态栏上的开关状态）
 * @retval 无
 * @note   仅当字符串变化时才重绘，避免 150ms 一次的无效刷新
 */
static void UpdateGraphOutputValues(void) {
    formatGraphValue(Voltage, 'V', graph_VoltageMsg);
    if (strcmp(graph_VoltageMsg, graph_last_VoltageMsg) != 0) {
        lcd_draw_string(2, 40, graph_VoltageMsg, &yahei16x20, 0xeb0c, 0x29a7, 0);
        strcpy(graph_last_VoltageMsg, graph_VoltageMsg);
    }

    formatGraphValue(Current, 'A', graph_CurrentMsg);
    if (strcmp(graph_CurrentMsg, graph_last_CurrentMsg) != 0) {
        lcd_draw_string(2, 79, graph_CurrentMsg, &yahei16x20, 0x5cbd, 0x29a7, 0);
        strcpy(graph_last_CurrentMsg, graph_CurrentMsg);
    }

    DrawGraphPowerChip();
}

/**
 * @brief  绘制曲线页整页（状态栏 + 电压电流调节区 + 曲线区底图）
 * @retval 无
 * @note   曲线本身由 dsp_graph_page_refresh_handler() 在下一拍重绘
 */
static void DrawDpsGraphPage(void) {
    // -------------------- 顶部状态栏（与主界面同一套几何） --------------------
    lcd_draw_rect(0, 0, 319, 31, 0x2946, 1);
    lcd_draw_round_rect(5, 3, 139, 26, 5, 0x31e8, 1);
    lcd_draw_round_rect(146, 3, 230, 26, 5, 0x31e8, 1);
    lcd_draw_round_rect(237, 3, 315, 26, 5, 0x31e8, 1);
    lcd_draw_image(7, 5, &img_IN_16x20);
    lcd_draw_image(150, 5, &img_FIRE_16x20);
    lcd_draw_string(27, 5, "0.00V 0.00A", &KaiTi16x20, 0xef7d, 0x31e8, -5);
    lcd_draw_string(171, 5, "00.0C", &KaiTi16x20, 0xef7d, 0x31e8, -4);
    last_InputMsg[0] = '\0';              // 已画占位值，影子串失效，保证马上就刷新
    last_TempMsg[0] = '\0';
    graph_chip_on = 0xFF;                  // 开关/CV-CC 缓存失效，强制重画
    graph_chip_mode = 0xFF;
    DrawGraphPowerChip();

    // -------------------- 电压/电流调节区 --------------------
    lcd_draw_rect(0, 32, 319, 109, 0x29a7, 1);

    // 实测值（16 号雅黑，和主界面单位行同一套字体）
    formatGraphValue(Voltage, 'V', graph_VoltageMsg);
    lcd_draw_string(2, 40, graph_VoltageMsg, &yahei16x20, 0xeb0c, 0x29a7, 0);
    strcpy(graph_last_VoltageMsg, graph_VoltageMsg);

    formatGraphValue(Current, 'A', graph_CurrentMsg);
    lcd_draw_string(2, 79, graph_CurrentMsg, &yahei16x20, 0x5cbd, 0x29a7, 0);
    strcpy(graph_last_CurrentMsg, graph_CurrentMsg);

    // 电压设定值 + 进度条
    fmt_fixed_00_00(Target_Voltage, MAX_OUTPUT_VOLTAGE, VoltageSetMsg);
    lcd_draw_string(106, 37, VoltageSetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, 1);
    lcd_draw_round_rect(volt_bar_graph.x0, volt_bar_graph.y0, volt_bar_graph.x1, volt_bar_graph.y1,
                        4, volt_bar_graph.bg, 1);
    volt_bar_graph.last_px = 0;
    DrawBar(&volt_bar_graph, Target_Voltage);

    // 电流设定值 + 进度条
    fmt_fixed_00_00(Target_Current, MAX_OUTPUT_CURRENT, Current_SetMsg);
    lcd_draw_string(106, 76, Current_SetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, 1);
    lcd_draw_round_rect(curr_bar_graph.x0, curr_bar_graph.y0, curr_bar_graph.x1, curr_bar_graph.y1,
                        4, curr_bar_graph.bg, 1);
    curr_bar_graph.last_px = 0;
    DrawBar(&curr_bar_graph, Target_Current);

    // 当前光标焦点框
    const FocusRect_t *fr = &graph_focus_rects[graph_cursor_current];
    lcd_draw_round_rect(fr->x, fr->y, (uint16_t) (fr->x + fr->w), (uint16_t) (fr->y + fr->h),
                        fr->radius, fr->color, fr->filled);

    // -------------------- 曲线区 --------------------
    DpsGraph_DrawChrome();
    graph_frame_pending = 1;
}

// 曲线显示界面按键处理
void dps_graph_handler(KeyEventMsg_t msg) {
    // 上移/左按：焦点切到电压
    if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT || msg.event == ENCODER_EVENT_PRESS_LEFT) {
        graph_cursor_past = graph_cursor_current;
        graph_cursor_current = Main_Voltage;
    }
    // 下移/右按：焦点切到电流
    else if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT || msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        graph_cursor_past = graph_cursor_current;
        graph_cursor_current = Main_Current;
    }
    // 短按SET/编码器：进入数位编辑（退出后仍回曲线页）
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (graph_cursor_current == Main_Current) {
            current_page = PAGE_CURR_DIGIT;
            previous_page = PAGE_MAIN_GRAPH;
            lcd_draw_round_rect(digit_x_graph[hundredths], 106,
                                (uint16_t) (digit_x_graph[hundredths] + 16), 109, 2, 0x5cbd, 1);
        } else {
            current_page = PAGE_VOLT_DIGIT;
            previous_page = PAGE_MAIN_GRAPH;
            lcd_draw_round_rect(digit_x_graph[hundredths], 67,
                                (uint16_t) (digit_x_graph[hundredths] + 16), 70, 2, 0xeb0c, 1);
        }
        StartBeezer(0);
        return;
    }
    // 长按SET：电源开关
    else if (msg.key == KEY_SET && msg.event == KEY_EVENT_LONG_PRESS) {
        IsPowerOn = !IsPowerOn;
        if (IsPowerOn) {
            Dps_ResetOutputAndWriteDac();   // 先把 DAC 写回目标电压位置，再打开输出级
            DpsPower_ON();
            osDelay(10);
            DpsRelease_OFF();
            DpsGnd_ON();
            DpsDc_ON();
        } else {
            DpsPower_OFF();
            DpsRelease_ON();
            osDelay(80);
            DpsDc_OFF();
            DpsGnd_OFF();
        }
        DrawGraphPowerChip();
        StartBeezer(0);
        return;
    }
    // 长按编码器：切换到LVGL APP
    else if (msg.key == KEY_ENCODER && msg.event == KEY_EVENT_LONG_PRESS) {
        IsPowerOn = false;

        AppListType APP = APP_LVGL;
        StartBeezer(0);
        osMessageQueuePut(AppSwitchQueueHandle, &APP, 0, 10);
        return;
    }
    // 单击MODE：回主界面
    else if (msg.key == KEY_MODE && msg.event == KEY_EVENT_CLICK) {
        current_page = PAGE_MAIN;
        previous_page = PAGE_MAIN_GRAPH;
        DrawDpsMainBasicElement();
        StartBeezer(0);
        return;
    }
    else {
        return;
    }

    // 光标移动有效：蜂鸣提示 + 重画焦点框（表驱动）
    StartBeezer(0);
    if (graph_cursor_past != graph_cursor_current) {
        const FocusRect_t *old_r = &graph_focus_rects[graph_cursor_past];
        const FocusRect_t *new_r = &graph_focus_rects[graph_cursor_current];
        lcd_draw_round_rect(old_r->x, old_r->y, (uint16_t) (old_r->x + old_r->w),
                            (uint16_t) (old_r->y + old_r->h), old_r->radius, 0x29a7, old_r->filled);
        lcd_draw_round_rect(new_r->x, new_r->y, (uint16_t) (new_r->x + new_r->w),
                            (uint16_t) (new_r->y + new_r->h), new_r->radius, new_r->color, new_r->filled);
    }
}

void dps_main_page_refresh_handler(void) {
    // 每150ms更新一次界面数值（15×10ms） // 8帧/s
    if (widgets_update_time_count >= 15) {
        widgets_update_time_count = 0;
        UpdateLableValues();
    }

    // 每1秒更新一次运行时间（100×10ms）
    if (Time_update_time_count >= 100) {
        Time_update_time_count = 0;
        running_time++;
        fmt_hms(running_time, TimeMsg);
        lcd_draw_string(33, 215, TimeMsg, &KaiTi16x20, 0xFFFF, 0x31e8, -3);
    }
    // 每1s更新一次风扇状态（100×10ms）
    if (Fan_update_time_count >= 100) {
        Fan_update_time_count = 0;
        fmt_percent_u8(Fan_Duty_Cycle, FanMsg);
        lcd_draw_string(262, 5, FanMsg, &KaiTi16x20, 0xef7d, 0x31e8, -4);
    }
}


/**
 * @brief  曲线页刷新
 * @note   150ms 刷新一次状态栏/实测值；曲线按采样节拍（25fps）重绘一整帧
 */
void dsp_graph_page_refresh_handler(void) {
    // 每150ms更新一次状态栏与实测值（15×10ms）
    if (widgets_update_time_count >= 15) {
        widgets_update_time_count = 0;
        UpdateStatusBarInfo();
        UpdateGraphOutputValues();
    }

    // 时间/风扇计时照常推进，切回主界面时不会一次跳变
    if (Time_update_time_count >= 100) {
        Time_update_time_count = 0;
        if (IsPowerOn) running_time++;
    }
    if (Fan_update_time_count >= 100) Fan_update_time_count = 0;

    // 有新样本就重绘一帧
    if (graph_frame_pending) {
        graph_frame_pending = 0;
        DpsGraph_DrawFrame(Target_Voltage, Target_Current, IsPowerOn ? 1 : 0);
    }
}

/**
 * @brief  数位编辑页刷新（按进入编辑前的页面分派）
 * @note   从曲线页进入编辑时，主界面的大字号刷新会把曲线页画面覆盖掉，必须分开处理
 */
void dps_digit_page_refresh_handler(void) {
    if (previous_page == PAGE_MAIN_GRAPH) {
        dsp_graph_page_refresh_handler();
    } else {
        dps_main_page_refresh_handler();
    }
}























