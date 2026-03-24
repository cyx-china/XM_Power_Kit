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

/* ========================== 宏定义 ========================== */
/**
 * @brief 毫秒转小时系数
 * @note  用于能量计算（能量=功率×时间），1小时=3600000毫秒
 */
#define MS_TO_HOUR (1.0f / 3600000.0f)

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
    PAGE_NUM            // 页面总数
} AWG_AppPage_t;

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
 * @brief 数位编辑上下文结构体
 * @note  统一管理电压/电流数位编辑的相关参数
 */
typedef struct {
    volatile float *pTarget;    // 目标值指针（Voltage/Current）
    char *pSetMsg;              // 显示字符串缓存指针
    uint16_t yValue;            // 数值显示Y坐标
    uint16_t yBar;              // 进度条Y坐标
    uint16_t color;             // 编辑态颜色
    uint8_t yUnderlineBase;     // 下划线Y坐标（数位选择指示）
    void (*drawBarFunc)(float); // 进度条绘制函数指针
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
AWG_AppPage_t current_page = PAGE_MAIN;     // 当前激活页面
AWG_AppPage_t previous_page = PAGE_MAIN;    // 上一个页面（用于返回）

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
float Power = 0;                             // 实时输出功率（单位：W）
float energy = 0;                            // 累计输出能量（单位：Wh）

uint32_t running_time = 0;                   // 电源运行时长（单位：秒）

uint8_t widgets_update_time_count = 0;          // 界面控件更新计时（每10ms+1，20次=200ms刷新）
uint8_t Time_update_time_count = 0;             // 时间更新计时（每10ms+1，100次=1s刷新）
uint8_t Fan_update_time_count = 0;              // 风扇占空比更新计时（每10ms+1，100次=1s刷新）

KeyEventMsg_t Keymsg;               // 按键事件消息缓存

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
static uint8_t last_voltage_pixels = 0;     // 上一次电压进度条像素数（用于防抖刷新）
static uint8_t last_current_pixels = 0;     // 上一次电流进度条像素数（用于防抖刷新）

static char last_InputMsg[12] = {0};        // 上一次输入电压/电流显示值（用于对比刷新）
static char last_TempMsg[6] = {0};          // 上一次温度显示值（用于对比刷新）
static char last_VoltageMsg[7] = {0};       // 上一次输出电压显示值（用于对比刷新）
static char last_CurrentMsg[7] = {0};       // 上一次输出电流显示值（用于对比刷新）
static char last_PowerMsg[7] = {0};         // 上一次输出功率显示值（用于对比刷新）

/* ========================== 表驱动常量定义 ========================== */
/**
 * @brief 声明进度条绘制函数
 * @note  这俩声明要提前
 */
static void DrawVoltageBar(float voltage);  // 电压进度条绘制函数
static void DrawCurrentBar(float current);  // 电流进度条绘制函数

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
 * @brief 电压编辑上下文常量
 */
static const DigitEditContext_t volt_ctx = {
    &Target_Voltage,    // 目标电压指针
    VoltageSetMsg,      // 电压设置显示缓存
    54,                 // 电压值显示Y坐标
    40,                 // 电压进度条Y坐标
    0xeb0c,             // 电压编辑态颜色
    84,                 // 电压下划线基准Y坐标
    DrawVoltageBar      // 电压进度条绘制函数
};

/**
 * @brief 电流编辑上下文常量
 */
static const DigitEditContext_t curr_ctx = {
    &Target_Current,    // 目标电流指针
    Current_SetMsg,     // 电流设置显示缓存
    106,                // 电流值显示Y坐标
    93,                 // 电流进度条Y坐标
    0x5cbd,             // 电流编辑态颜色
    136,                // 电流下划线基准Y坐标
    DrawCurrentBar      // 电流进度条绘制函数
};

/* ========================== 函数声明 ========================== */
// 绘制相关函数
static void DrawMainBasicElement(void); // 绘制界面基础元素
static void DrawQuickSetPage(void); // 绘制快速设置页面

// 数据更新函数
static void UpdateLableValues(void); // 更新界面数值显示

// 页面处理函数
void dps_main_page_handler(KeyEventMsg_t msg);  // 主界面按键处理
void volt_digit_handler(KeyEventMsg_t msg); // 电压数位编辑按键处理
void curr_digit_handler(KeyEventMsg_t msg); // 电流数位编辑按键处理
void quick_menu_handler(KeyEventMsg_t msg); // 快速设置菜单按键处理

// 格式化工具函数
int float_to_fixed_digits(float value, uint8_t target_digits, char *buf);

void formatFloatMessage(float voltage, float current, char *message);

void formatTempMessage(float temp, char *message);

void formatOutputMessage(float value, char *message);

int float_to_fixed_00_00(float value, char *buf);

int float_to_0000_000Wh(float value, char *buf);

int seconds_to_hms_format(uint32_t time, char *buf);

int uint8_to_000_percent(uint8_t value, char *buf);

// ADC相关函数
void CB_DPS_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);  // ADC转换完成回调
void DPS_ADC_Init(void);                                    // ADC初始化
void DPS_ADC_DeInit(void);                                  // ADC反初始化

// 任务相关函数
void Start_DpsCoreTask(void *argument);                     // DPS核心任务入口
void Start_PIDTask(void *argument);                         // PID运算子任务入口

// 通用辅助函数
static void append_padded_uint(char **p, uint32_t val, uint8_t width);              // 无符号数补0追加到字符串
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
};

/* ========================== 通用辅助函数实现 ========================== */
/**
 * @brief  无符号整数按指定宽度补0后追加到字符串
 * @param  p: 字符串指针的指针（指向待追加位置）
 * @param  val: 待处理的无符号整数
 * @param  width: 目标宽度（不足补0，超过保留原数）
 * @retval 无
 * @note   val为0时直接填充width个0，确保输出字符串长度符合宽度要求
 */
static void append_padded_uint(char **p, uint32_t val, uint8_t width) {
    char tmp[16];
    uint8_t n = 0;

    // 数值为0时直接填充指定宽度的0
    if (val == 0) {
        for (uint8_t i = 0; i < width; i++) *(*p)++ = '0';
        return;
    }

    // 拆分数字到临时缓冲区（逆序）
    while (val > 0) {
        tmp[n++] = (val % 10) + '0';
        val /= 10;
    }

    // 补0到指定宽度
    while (n < width) tmp[n++] = '0';

    // 逆序写回目标字符串
    while (n--) *(*p)++ = tmp[n];
}

/* ========================== 格式化工具函数实现 ========================== */
/**
 * @brief  将浮点数格式化为指定位数的字符串（自动调整小数位）
 * @param  value: 待格式化的浮点数
 * @param  target_digits: 目标总位数（2~5）
 * @param  buf: 输出字符串缓冲区
 * @retval 输出字符串长度
 * @note   自动根据数值大小调整小数位数，确保显示精度
 */
int float_to_fixed_digits(float value, uint8_t target_digits, char *buf) {
    // 边界值保护
    if (target_digits < 2) target_digits = 2;
    if (target_digits > 5) target_digits = 5;
    if (value < 0) value = -value;

    // 数值上限保护
    float maxv[] = {0, 0, 99.9f, 999.f, 9999.f, 99999.f};
    if (value > maxv[target_digits]) value = maxv[target_digits];

    char *p = buf;
    int dec_digits;

    // 根据数值大小确定小数位数
    if (value >= 100.f) dec_digits = (target_digits >= 4) ? (target_digits - 3) : 0;
    else if (value >= 10.f) dec_digits = target_digits - 2;
    else dec_digits = target_digits - 1;
    if (dec_digits < 1) dec_digits = 1;

    // 放大数值为整数处理
    float scale = 1.0f;
    for (int i = 0; i < dec_digits; i++) scale *= 10.0f;
    uint32_t ival = (uint32_t) (value * scale + 0.5f);
    uint32_t int_part = ival / (uint32_t) scale;
    uint32_t frac_part = ival % (uint32_t) scale;

    // 进位处理：数值超过阈值时减少小数位
    uint32_t carry_threshold = (value >= 100.f) ? 1000 : (value >= 10.f ? 100 : 10);
    if (int_part >= carry_threshold && dec_digits > 1) {
        dec_digits--;
        frac_part /= 10;
    }

    // 写入整数部分
    if (int_part == 0) *p++ = '0';
    else {
        char tmp[8];
        int n = 0;
        uint32_t t = int_part;
        do {
            tmp[n++] = (t % 10) + '0';
            t /= 10;
        } while (t > 0);
        while (n--) *p++ = tmp[n];
    }

    // 写入小数点
    *p++ = '.';

    // 写入小数部分
    char frac_buf[6];
    for (int i = 0; i < dec_digits; i++) {
        frac_buf[i] = (frac_part % 10) + '0';
        frac_part /= 10;
    }
    for (int i = dec_digits - 1; i >= 0; i--) *p++ = frac_buf[i];

    // 字符串结束符
    *p = '\0';
    return (int) (p - buf);
}

/**
 * @brief  格式化电压+电流为显示字符串（格式：XX.XXV XX.XXA）
 * @param  voltage: 电压值（V）
 * @param  current: 电流值（A）
 * @param  message: 输出字符串缓冲区
 * @retval 无
 */
void formatFloatMessage(float voltage, float current, char *message) {
    char *p = message;
    p += float_to_fixed_digits(voltage, 3, p);
    *p++ = 'V';
    *p++ = ' ';
    p += float_to_fixed_digits(current, 3, p);
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
    p += float_to_fixed_digits(temp, 3, p);
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
    p += float_to_fixed_digits(value, 4, p);
    *p = '\0';
}

/**
 * @brief  将浮点数格式化为XX.XX格式的字符串（固定2位小数）
 * @param  value: 待格式化数值（V/A）
 * @param  buf: 输出字符串缓冲区
 * @retval 输出字符串长度（固定5）
 * @note   数值范围限制：0.01 ~ MAX_OUTPUT_VOLTAGE
 */
int float_to_fixed_00_00(float value, char *buf) {
    // 边界值保护
    if (value < 0.01f) value = 0.01f;
    if (value > MAX_OUTPUT_VOLTAGE) value = MAX_OUTPUT_VOLTAGE;

    // 转换为百分位整数
    uint16_t ival = (uint16_t) (value * 100.0f + 0.5f);
    char *p = buf;

    // 写入整数部分（2位补0）
    append_padded_uint(&p, ival / 100, 2);
    *p++ = '.';

    // 写入小数部分（2位补0）
    append_padded_uint(&p, ival % 100, 2);
    *p = '\0';

    return 5;
}

/**
 * @brief  将能量值格式化为XXXX.XXXWh格式的字符串
 * @param  value: 能量值（Wh）
 * @param  buf: 输出字符串缓冲区
 * @retval 输出字符串长度（固定9）
 * @note   数值范围限制：0 ~ 9999.999
 */
int float_to_0000_000Wh(float value, char *buf) {
    // 边界值保护
    if (value < 0) value = -value;
    if (value > 9999.999f) value = 9999.999f;

    // 转换为毫瓦时整数
    uint32_t ival = (uint32_t) (value * 1000.0f + 0.5f);
    char *p = buf;

    // 写入整数部分（4位补0）
    append_padded_uint(&p, ival / 1000, 4);
    *p++ = '.';

    // 写入小数部分（3位补0）
    append_padded_uint(&p, ival % 1000, 3);
    *p++ = 'W';
    *p++ = 'h';
    *p = '\0';

    return 9;
}

/**
 * @brief  将秒数格式化为XXX:XX:XX格式的时间字符串
 * @param  time: 时长（秒）
 * @param  buf: 输出字符串缓冲区
 * @retval 成功返回8，失败返回-1
 * @note   小时数上限999，超过则固定为999:59:59
 */
int seconds_to_hms_format(uint32_t time, char *buf) {
    if (!buf) return -1;

    // 拆分时分秒
    uint32_t h = time / 3600, m = (time % 3600) / 60, s = time % 60;

    // 小时数上限保护
    if (h > 999) {
        h = 999;
        m = 59;
        s = 59;
    }

    char *p = buf;
    append_padded_uint(&p, h, 3);
    *p++ = ':';
    append_padded_uint(&p, m, 2);
    *p++ = ':';
    append_padded_uint(&p, s, 2);
    *p = '\0';

    return 8;
}

/**
 * @brief  将uint8_t数值格式化为XXX%格式的百分比字符串
 * @param  value: 百分比值（0~100）
 * @param  buf: 输出字符串缓冲区
 * @retval 成功返回4，失败返回-1
 */
int uint8_to_000_percent(uint8_t value, char *buf) {
    if (!buf) return -1;

    char *p = buf;
    append_padded_uint(&p, value, 3);
    *p++ = '%';
    *p = '\0';

    return 4;
}

/* ========================== ADC相关函数实现 ========================== */
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

        // 转换为实际电压值（3.3V参考，12位ADC，乘以校准系数）
        float raw_voltage = (float) (temp1 > 4096 ? 0 : temp1) * 3.3f / 4096.0f * UserParam.DPS_Voltage_Factor;

        // 转换为实际电流值（扣除电压耦合偏移和固定偏移）
        float raw_current = (float) (temp2 > 4096 ? 0 : temp2) * 3.3f / 4096.0f * UserParam.DPS_Current_Factor;

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

    // 电源开启时累计能量（功率×时间，时间单位：小时）
    if (IsPowerOn) energy += Power * MS_TO_HOUR;

    AdcTim_ON();                                                             // 开启定时器（ADC触发源）
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
    // 映射当前/上一次编辑数位（区分电压/电流）
    DigitPosition_e *pCurrent = (ctx == &volt_ctx) ? &VoltagePosition_Current : &CurrentPosition_Current;
    DigitPosition_e *pPast = (ctx == &volt_ctx) ? &VoltagePosition_Past : &CurrentPosition_Past;

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
        *ctx->pTarget = fminf(*ctx->pTarget + 0.01f * dps_pow10_table[*pCurrent],
                              (ctx == &volt_ctx ? MAX_OUTPUT_VOLTAGE : MAX_OUTPUT_CURRENT));
        float_to_fixed_00_00(*ctx->pTarget, ctx->pSetMsg);
        lcd_draw_string(197, ctx->yValue, ctx->pSetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, 2);
        if (ctx == &volt_ctx) SetTargetVoltage(*ctx->pTarget);
        else SetTargetCurrent(*ctx->pTarget);
        ctx->drawBarFunc(*ctx->pTarget);
        StartBeezer(0);
        return;
    }
    // 左旋转编码器：减少对应数位的数值
    else if (msg.event == ENCODER_EVENT_LEFT) {
        *ctx->pTarget = fmaxf(*ctx->pTarget - 0.01f * dps_pow10_table[*pCurrent],
                              (ctx == &volt_ctx ? MIN_OUTPUT_VOLTAGE : MIN_OUTPUT_CURRENT));
        float_to_fixed_00_00(*ctx->pTarget, ctx->pSetMsg);
        lcd_draw_string(197, ctx->yValue, ctx->pSetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, 2);
        if (ctx == &volt_ctx) SetTargetVoltage(*ctx->pTarget);
        else SetTargetCurrent(*ctx->pTarget);
        ctx->drawBarFunc(*ctx->pTarget);
        StartBeezer(0);
        return;
    }
    // 短按SET/编码器：返回主界面
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        current_page = PAGE_MAIN;
        previous_page = (ctx == &volt_ctx ? PAGE_VOLT_DIGIT : PAGE_CURR_DIGIT);
        *pCurrent = *pPast = hundredths;
        lcd_draw_rect(195, ctx->yUnderlineBase, 297, ctx->yUnderlineBase + 5, 0x29a7, 1);
        StartBeezer(0);
        return;
    }
    // 无匹配按键事件：直接返回
    else return;

    // 按键有效时蜂鸣提示
    StartBeezer(0);

    // 数位选择下划线更新（切换选中数位的指示）
    uint16_t x_pos[4] = {279, 257, 219, 197}; // 百分位/十分位/个位/十位的X坐标
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
    digit_page_handler(msg, &volt_ctx);
}

/**
 * @brief  电流数位编辑按键处理（封装通用函数）
 * @param  msg: 按键事件消息
 * @retval 无
 */
void curr_digit_handler(KeyEventMsg_t msg) {
    digit_page_handler(msg, &curr_ctx);
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

        // 读取按键事件并分发到当前页面的处理函数
        if (osMessageQueueGet(KeyEventQueueHandle, &Keymsg, NULL, 0) == osOK)
            page_handlers[current_page](Keymsg);

        // 每100ms更新一次界面数值（15×10ms）
        if (widgets_update_time_count >= 15) {
            widgets_update_time_count = 0;
            UpdateLableValues();
        }

        // 每1秒更新一次运行时间（100×10ms）
        if (Time_update_time_count >= 100) {
            Time_update_time_count = 0;
            running_time++;
            seconds_to_hms_format(running_time, TimeMsg);
            lcd_draw_string(33, 215, TimeMsg, &KaiTi16x20, 0xFFFF, 0x31e8, -3);
        }
        // 每s更新一次风扇状态（100×10ms）
        if (Fan_update_time_count >= 100) {
            Fan_update_time_count = 0;
            uint8_to_000_percent(Fan_Duty_Cycle, FanMsg);
            lcd_draw_string(262, 5, FanMsg, &KaiTi16x20, 0xef7d, 0x31e8, -4);
        }

        // 任务延时（10ms）
        osDelay(10);
    }
}

/* ========================== 绘制函数实现 ========================== */
/**
 * @brief  绘制电压进度条
 * @param  voltage: 目标电压值（V）
 * @retval 无
 * @note   防抖刷新：仅当像素数变化时才重绘，避免频繁刷新
 */
static void DrawVoltageBar(float voltage) {
    // 计算进度条像素数（最大128像素，对应MAX_OUTPUT_VOLTAGE）
    uint8_t now = (uint8_t) (128 * (voltage / MAX_OUTPUT_VOLTAGE));

    // 边界值保护
    if (now > 128) now = 128;
    if (now < 8) now = 8;

    // 防抖：像素数未变化则不重绘
    if (now == last_voltage_pixels) return;

    // 像素数减少：先清空整个进度条再绘制新值
    if (now < last_voltage_pixels) {
        lcd_draw_round_rect(180, 40, 309, 47, 4, 0x6a49, 1);
        lcd_draw_round_rect(180, 40, 180 + now, 47, 4, 0xeb0c, 1);
    }
    // 像素数增加：直接绘制新增部分
    else {
        lcd_draw_round_rect(180, 40, 180 + now, 47, 4, 0xeb0c, 1);
    }

    // 更新上一次像素数
    last_voltage_pixels = now;
}

/**
 * @brief  绘制电流进度条
 * @param  current: 目标电流值（A）
 * @retval 无
 * @note   防抖刷新：仅当像素数变化时才重绘，避免频繁刷新
 */
static void DrawCurrentBar(float current) {
    // 计算进度条像素数（最大128像素，对应MAX_OUTPUT_CURRENT）
    uint8_t now = (uint8_t) (128 * (current / MAX_OUTPUT_CURRENT));

    // 边界值保护
    if (now > 128) now = 128;
    if (now < 8) now = 8;

    // 防抖：像素数未变化则不重绘
    if (now == last_current_pixels) return;

    // 像素数减少：先清空整个进度条再绘制新值
    if (now < last_current_pixels) {
        lcd_draw_round_rect(180, 93, 309, 100, 4, 0x3ace, 1);
        lcd_draw_round_rect(180, 93, 180 + now, 100, 4, 0x5cbd, 1);
    }
    // 像素数增加：直接重绘
    else {
        lcd_draw_round_rect(180, 93, 180 + now, 100, 4, 0x5cbd, 1);
    }

    // 更新上一次像素数
    last_current_pixels = now;
}

/**
 * @brief  绘制界面基础元素（框架、图标、初始数值）
 * @retval 无
 * @note   初始化界面时调用，绘制静态框架和默认数值
 */
static void DrawMainBasicElement(void) {
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

    // 绘制电压进度条背景和初始值
    lcd_draw_round_rect(180, 40, 309, 47, 4, 0x6a49, 1);
    DrawVoltageBar(5.0f);
    lcd_draw_string(197, 54, "05.00", &ScreenMatrix20x28, 0xFFFF, 0x29a7, 2);

    // 绘制电流进度条背景和初始值
    lcd_draw_round_rect(180, 93, 309, 100, 4, 0x3ace, 1);
    DrawCurrentBar(2.0f);
    lcd_draw_string(197, 106, "02.00", &ScreenMatrix20x28, 0xFFFF, 0x29a7, 2);

    // 绘制初始焦点框（电压项）
    lcd_draw_round_rect(180, 55, 185, 80, 3, 0x87d3, 1);

    // 绘制电源开关/功率模式/快速设置按钮
    lcd_draw_round_rect(175, 177, 239, 200, 8, 0x632c, 1);
    lcd_draw_round_rect(247, 177, 311, 200, 8, 0xec10, 1);
    lcd_draw_round_rect(182, 144, 304, 169, 8, 0x7644, 1);

    // 绘制按钮文字
    lcd_draw_string(205, 148, "快速设置", &YPLXT18x18, 0xFFFF, 0x7644, 2);
    lcd_draw_string(186, 180, "OFF", &JetBrainsMono14x18, 0xFFFF, 0x632c, 2);
    lcd_draw_string(264, 180, "CV", &JetBrainsMono14x18, 0xFFFF, 0xec10, 5);

    // 绘制底部时间/能量背景框
    lcd_draw_round_rect(5, 213, 156, 236, 5, 0x31e8, 1);
    lcd_draw_round_rect(164, 213, 315, 236, 5, 0x31e8, 1);

    // 绘制底部图标
    lcd_draw_image(9, 216, &img_time_18x18);
    lcd_draw_image(168, 215, &img_energy_16x20);

    // 绘制底部初始数值
    lcd_draw_string(33, 215, "000:00:00", &KaiTi16x20, 0xFFFF, 0x31e8, -3);
    lcd_draw_string(188, 215, "0000.000Wh", &KaiTi16x20, 0xFFFF, 0x31e8, -3);
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
 * @brief  更新界面数值显示
 * @retval 无
 * @note   仅当数值变化时才重绘，避免频繁刷新LCD
 */
static void UpdateLableValues(void) {
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
        float_to_0000_000Wh(energy, EnergyMsg);
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
        DpsPower_OFF();
        DpsRelease_ON();
        osDelay(80);
        DpsDc_OFF();
        DpsGnd_OFF();

        AppListType APP = APP_LVGL;
        StartBeezer(0);
        osMessageQueuePut(AppSwitchQueueHandle, &APP, 0, 10);
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
        float_to_fixed_00_00(Target_Voltage, VoltageSetMsg);
        lcd_draw_string(197, 54, VoltageSetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, 2);
        DrawVoltageBar(Target_Voltage);

        // 更新电流设置显示和进度条
        float_to_fixed_00_00(Target_Current, Current_SetMsg);
        lcd_draw_string(197, 106, Current_SetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, 2);
        DrawCurrentBar(Target_Current);

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
        float_to_fixed_00_00(Target_Voltage, VoltageSetMsg);
        lcd_draw_string(197, 54, VoltageSetMsg, &ScreenMatrix20x28, 0xFFFF, 0x29a7, 2);
        float_to_fixed_00_00(Target_Current, Current_SetMsg);
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


    DPS_ADC_Init();             // 重新初始化ADC
    DrawMainBasicElement();     // 绘制基础UI界面
    osDelay(150);          // 延时确保UI绘制完成

    // 注册ADC转换完成回调函数，用于处理采集数据
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_COMPLETE_CB_ID, CB_DPS_ADC_ConvCpltCallback);
    // 启动ADC DMA采集（缓冲区长度128，环形模式）
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *) dps_adc_raw_buf, 128);

    AdcTim_OFF();
    __HAL_TIM_SET_PRESCALER(&htim8, 149);   // 设定更新频率64Khz
    __HAL_TIM_SET_AUTORELOAD(&htim8, 14);
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

    // 无限循环（RTOS任务核心逻辑）
    for (;;) {
        // 仅当电源开启时执行PID运算和DAC更新
        if (IsPowerOn == true) {
            // 计算电流PID输出值（目标电流-实际电流的闭环调节）
            uint16_t DAC_Value = PID_Calculate(Current_PID,Voltage_PID);
            // 将PID输出写入DAC（MCP4725），控制输出电流
            MCP4725_WriteFast(&hmcp4725_DC, DAC_Value, MCP4725_MODE_NORMAL);
        }
        osDelay(1); // 任务周期1ms
    }
}
