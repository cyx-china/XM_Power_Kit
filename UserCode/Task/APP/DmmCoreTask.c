#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "adc.h"
#include "lcd_draw_api.h"
#include "stm32f4xx_hal.h"
#include "UserTask.h"
#include "os_handles.h"
#include "SwitchManager.h"
#include "tim.h"
#include "UserDefineManage.h"

// 函数前置声明
static void DrawDmmMainBasicElement(void);
static void Refresh_Voltage(float Voltage);
static void Refresh_Curerrent(float Current);
static void Refresh_Resistance(float Res_Vol);
static void Refresh_Diode(float Diode_Vol);

// 全局变量 - ADC原始数据缓存
// 三个通道，每个通道200个采样点，总计600个数据
volatile uint16_t dmm_adc_raw_buf[600] = {0};

// 全局变量 - 万用表测量值（实际物理值）
volatile float DMM_Voltage = 0.0f;                  // 电压测量实际值
volatile float DMM_Current = 0.0f;                  // 电流测量实际值
volatile float DMM_Resistance_Voltage = 0.0f;       // 电阻测量原始电压值

// 全局变量 - 格式化字符串缓存（用于LCD显示）
static char Vol_formatted_buf[8] = {0};             // 电压格式化缓存
static char Vol_thousandth_buf[2] = {0};            // 电压千分位缓存
static char Cur_formatted_buf[8] = {0};             // 电流格式化缓存
static char Cur_tenthousandth_buf[2] = {0};         // 电流万分位缓存
static char Res_formatted_R200_buf[8] = {0};        // 电阻200Ω档格式化缓存
static char Res_digits_R200_buf[2] = {0};           // 电阻200Ω档百分位缓存
static char Res_formatted_R2K_buf[8] = {0};         // 电阻2K档格式化缓存
static char Res_digits_R2K_buf[3] = {0};            // 电阻2K档千/万分位缓存
static char Dio_formatted_dio_buf[8] = {0};         // 二极管电压格式化缓存
static char Dio_thousandth_dio_buf[2] = {0};        // 二极管电压千分位缓存
static char Dio_formatted_res_buf[8] = {0};         // 二极管电阻格式化缓存
static char Dio_thousandth_res_buf[2] = {0};        // 二极管电阻千分位缓存

// 全局变量 - 刷新控制
static volatile bool Is_Refresh_On = true;          // 数据刷新使能标志

// 全局变量 - 按键与计时
KeyEventMsg_t DMM_Keymsg;                           // 按键事件消息缓存
uint8_t dps_time_count = 0;                         // 刷新率计时计数器

// 枚举定义 - 万用表功能页面
typedef enum {
    DMM_PAGE_Voltage = 0,               // 电压页面
    DMM_PAGE_Current,                   // 电流页面
    DMM_PAGE_Resistance,                // 电阻页面
    DMM_PAGE_Diode,                     // 二极管页面
    DMM_PAGE_NUM                        // 页面总数（用于边界检查）
} DMM_AppPage_t;

DMM_AppPage_t dmm_current_page = DMM_PAGE_Voltage; // 当前激活的功能页面

// 函数指针声明 - 页面按键处理函数
static void dmm_voltage_page_handler(KeyEventMsg_t msg);
static void dmm_current_page_handler(KeyEventMsg_t msg);
static void dmm_Resistance_page_handler(KeyEventMsg_t msg);
static void dmm_diode_page_handler(KeyEventMsg_t msg);

// 页面处理函数表（与DMM_AppPage_t枚举索引对应）
static void (*page_handlers[DMM_PAGE_NUM])(KeyEventMsg_t) = {
    [DMM_PAGE_Voltage]    = dmm_voltage_page_handler,
    [DMM_PAGE_Current]    = dmm_current_page_handler,
    [DMM_PAGE_Resistance] = dmm_Resistance_page_handler,
    [DMM_PAGE_Diode]      = dmm_diode_page_handler,
};

// 枚举定义 - 电阻测量档位
typedef enum {
    DMM_RES_MODE_200 = 0,   // 200Ω档位
    DMM_RES_MODE_2K,        // 2KΩ档位
} DMM_Res_Mode_t;

volatile DMM_Res_Mode_t dmm_res_mode = DMM_RES_MODE_200; // 当前电阻测量档位

/**
 * @brief  万用表核心任务主函数
 * @param  argument: 任务入参（未使用）
 * @note   1. 任务启动后先自挂起，由外部唤醒
 *         2. 循环处理按键事件、定时刷新显示
 *         3. 基础刷新率：10ms一次循环，100ms刷新一次显示
 */
void Start_DmmCoreTask(void *argument) {
    // 任务启动后先自挂起，等待外部唤醒
    osThreadSuspend(DmmCoreTaskHandle);

    for (;;) {
        // 刷新率计时累加
        dps_time_count++;

        // 读取按键消息队列，分发到当前页面的按键处理函数
        if (osMessageQueueGet(KeyEventQueueHandle, &DMM_Keymsg, NULL, 0) == osOK) {
            page_handlers[dmm_current_page](DMM_Keymsg);
        }

        // 每100ms刷新一次显示（dps_time_count>10 且 刷新使能）
        if (dps_time_count > 10) {
            if (Is_Refresh_On) {
                // 根据当前页面刷新对应数据
                switch (dmm_current_page) {
                    case DMM_PAGE_Voltage:
                        Refresh_Voltage(DMM_Voltage);
                        break;
                    case DMM_PAGE_Current:
                        Refresh_Curerrent(DMM_Current);
                        break;
                    case DMM_PAGE_Resistance:
                        Refresh_Resistance(DMM_Resistance_Voltage);
                        break;
                    case DMM_PAGE_Diode:
                        Refresh_Diode(DMM_Resistance_Voltage);
                        break;
                    default: break;
                }
            }
            // 重置刷新率计数器
            dps_time_count = 0;
        }

        // 任务延时10ms，控制循环频率
        osDelay(10);
    }
}

// 宏定义 - LCD显示颜色配置
#define COLOR_MODE_SEL_FILL        0x3c38  // 模式选中时填充色
#define COLOR_MODE_UNSEL_BG        0x1908  // 模式未选中时背景色
#define COLOR_MODE_UNSEL_BORDER    0x21aa  // 模式未选中时边框色
#define COLOR_TEXT_SEL_FG          0xFFFF  // 模式选中时文字色
#define COLOR_TEXT_UNSEL_FG        0x8c51  // 模式未选中时文字色

// 结构体定义 - 模式选择框配置
typedef struct {
    const char *symbol;       // 模式符号 (V, A, Ω, D)
    const char *label;        // 模式标签 (电压, 电流...)
    uint16_t x_sym;           // 符号显示X坐标
    uint16_t x_lbl;           // 标签显示X坐标
} DMM_ModeItem;

// 模式选择框配置表（与DMM_AppPage_t枚举索引对应）
static const DMM_ModeItem mode_items[] = {
    {"V", "电压",  40, 29},   // 索引0 - 电压
    {"A", "电流", 116, 105},  // 索引1 - 电流
    {"Ω", "电阻", 192, 182},  // 索引2 - 电阻
    {"D", "二极管", 268, 250} // 索引3 - 二极管
};

/**
 * @brief  绘制单个模式选择框
 * @param  idx: 模式索引（0-3）
 * @param  is_selected: 是否选中（1-选中，0-未选中）
 * @note   1. 选中状态：绘制填充色，无边界
 *         2. 未选中状态：绘制背景色+边界色
 *         3. 统一绘制符号和标签文字
 */
static void DMM_Draw_Single_Box(uint8_t idx, uint8_t is_selected) {
    // 1. 计算选择框坐标（固定尺寸：宽68，高43，圆角8）
    uint16_t x_start = 12 + 76 * idx;
    uint16_t x_end   = 80 + 76 * idx;
    uint16_t y_start = 186;
    uint16_t y_end   = 229;
    uint8_t  radius  = 8;

    // 2. 确定显示颜色
    uint16_t color_bg, color_text_fg;
    if (is_selected) {
        // 选中状态
        color_bg = COLOR_MODE_SEL_FILL;
        color_text_fg = COLOR_TEXT_SEL_FG;
        // 绘制填充圆角矩形（无边界）
        lcd_draw_round_rect(x_start, y_start, x_end, y_end, radius, color_bg, 1);
    } else {
        // 未选中状态
        color_bg = COLOR_MODE_UNSEL_BG;
        color_text_fg = COLOR_TEXT_UNSEL_FG;
        // 绘制背景填充 + 边界
        lcd_draw_round_rect(x_start, y_start, x_end, y_end, radius, color_bg, 1);
        lcd_draw_round_rect(x_start, y_start, x_end, y_end, radius, COLOR_MODE_UNSEL_BORDER, 0);
    }

    // 3. 绘制模式符号和标签文字
    lcd_draw_string(mode_items[idx].x_sym, 188, mode_items[idx].symbol, &yahei16x20, color_text_fg, color_bg, 0);
    lcd_draw_string(mode_items[idx].x_lbl, 210, mode_items[idx].label,  &yahei16x16, color_text_fg, color_bg, 2);
}

/**
 * @brief  绘制万用表模式选择框（批量/增量）
 * @param  new_idx: 当前选中的模式索引 [0, 3]
 * @param  old_idx: 上一次选中的模式索引 [0, 3]
 * @note   1. 入参校验：new_idx超出范围则直接返回
 *         2. 全量刷新：new_idx == old_idx（初始化/强制刷新）
 *         3. 增量刷新：仅重绘旧选中和新选中的两个框
 */
void DMM_Draw_Mode_Boxes(uint8_t new_idx, uint8_t old_idx) {
    // 入参有效性校验
    if (new_idx > 3) return;

    // 判断刷新方式：全量/增量
    if (new_idx == old_idx) {
        // 全量刷新：绘制所有4个模式框
        for (uint8_t i = 0; i < 4; i++) {
            DMM_Draw_Single_Box(i, (i == new_idx));
        }
    } else {
        // 增量刷新：仅更新旧/新选中的两个框
        DMM_Draw_Single_Box(old_idx, 0);  // 旧选中框：设为未选中
        DMM_Draw_Single_Box(new_idx, 1);  // 新选中框：设为选中
    }
}

/**
 * @brief  HOLD功能控制函数
 * @param  is_hold: HOLD使能标志（1-开启，0-关闭）
 * @note   1. 开启HOLD：停止数据刷新，显示"HOLD"高亮
 *         2. 关闭HOLD：恢复数据刷新，显示"HOLD"暗色
 */
static void Hold_ctrl(bool is_hold) {
    if (is_hold) {
        Is_Refresh_On = false;
        lcd_draw_string(250, 47, "HOLD", &JetBrainsMono14x18, 0xe22c, 0x1908, -1);
    } else {
        Is_Refresh_On = true;
        lcd_draw_string(250, 47, "HOLD", &JetBrainsMono14x18, 0xad55, 0x1908, -1);
    }
}

/**
 * @brief  绘制万用表主界面基础元素
 * @note   1. 绘制背景、顶栏、数据区、模式选择框、挡位区等固定元素
 *         2. 初始化显示默认状态（电压模式、AUTO挡位、HOLD暗色）
 */
static void DrawDmmMainBasicElement(void) {
    // 绘制背景
    lcd_draw_rect(0, 0, 319, 31, 0x1908, 1);             // 顶栏背景
    lcd_draw_rect(0, 31, 319, 239, 0x18c6, 1);           // 主体背景
    lcd_draw_round_rect(12, 80, 308, 166, 8, 0x1908, 1); // 数据区背景

    // 绘制模式选择框（默认选中电压模式）
    DMM_Draw_Mode_Boxes(0, 0);

    // 绘制挡位区背景（边框+填充）
    lcd_draw_round_rect(60, 44, 130, 68, 3, 0x1908, 1);  // 挡位背景填充
    lcd_draw_round_rect(60, 44, 130, 68, 3, 0x21aa, 0);  // 挡位背景边框

    // 绘制顶栏
    lcd_draw_string(127, 6, "万用表", &yahei20x20, 0x24be, 0x1908, 3); // 标题
    lcd_draw_line(0, 31, 319, 31, 0x11ac);                             // 顶栏下划线

    // 绘制参数区
    lcd_draw_image(20, 42, &img_DMM_DC_30x30);                         // 直流图标
    lcd_draw_string(69, 47, "AUTO", &JetBrainsMono14x18, 0xFFFF, 0x1908, 0); // 自动挡位
    // HOLD区域背景（边框+填充）
    lcd_draw_round_rect(242, 44, 306, 68, 3, 0x1908, 1);  // HOLD背景填充
    lcd_draw_round_rect(242, 44, 306, 68, 3, 0x21aa, 0);  // HOLD背景边框
    lcd_draw_string(250, 47, "HOLD", &JetBrainsMono14x18, 0xad55, 0x1908, -1); // HOLD文字（默认暗色）

    // 初始化数据区显示
    lcd_draw_string(30, 98, "+00.00", &DIN_Medium32x48, 0xeb0c, 0x1908, 5); // 电压默认值
    lcd_draw_string(240, 98, "0", &DIN_Medium32x48, 0x632c, 0x1908, 0);     // 小数位默认值
    lcd_draw_string(288, 136, "V", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0); // 电压单位
}

/**
 * @brief  将浮点数格式化为±00.00，并提取千分位
 * @param  num: 输入浮点数（范围±40）
 * @param  formatted_buf: 存储±00.00格式的缓存（至少8字节）
 * @param  thousandth_buf: 存储千分位字符的缓存（至少2字节）
 * @return 0-成功，-1-空指针错误
 * @note   1. 格式化规则：±00.00（包含符号、两位整数、两位小数）
 *         2. 千分位提取：绝对值×1000后取个位，修正浮点误差
 */
int FormatFloatWithThousandth(float num, char *formatted_buf, char *thousandth_buf) {
    // 空指针校验
    if (formatted_buf == NULL || thousandth_buf == NULL) {
        return -1;
    }

    // 格式化为主字符串（±00.00）
    snprintf(formatted_buf, 8, "%+06.2f", num);

    // 提取千分位（修正浮点误差）
    double scaled = fabs(num) * 1000.0 + 1e-6;
    int thousandth_digit = (int)scaled % 10;

    // 转换为字符并补结束符
    thousandth_buf[0] = '0' + thousandth_digit;
    thousandth_buf[1] = '\0';

    return 0;
}

/**
 * @brief  将浮点数格式化为±0.000，并提取万分位
 * @param  num: 输入浮点数
 * @param  formatted_buf: 存储±0.000格式的缓存（至少8字节）
 * @param  tenthousandth_buf: 存储万分位字符的缓存（至少2字节）
 * @return 0-成功，-1-空指针错误
 * @note   1. 格式化规则：±0.000（包含符号、一位整数、三位小数）
 *         2. 万分位提取：绝对值×10000后取个位，修正浮点误差
 */
int FormatFloatWithTenThousandth(float num, char *formatted_buf, char *tenthousandth_buf) {
    // 空指针校验
    if (formatted_buf == NULL || tenthousandth_buf == NULL) {
        return -1;
    }

    // 格式化为主字符串（±0.000）
    snprintf(formatted_buf, 8, "%+06.3f", num);

    // 提取万分位（修正浮点误差）
    double scaled = fabs(num) * 10000.0 + 1e-7;
    int tenthousandth_digit = (int)scaled % 10;

    // 转换为字符并补结束符
    tenthousandth_buf[0] = '0' + tenthousandth_digit;
    tenthousandth_buf[1] = '\0';

    return 0;
}

/**
 * @brief  将浮点数格式化为0000.0，并提取百分位
 * @param  num: 输入浮点数
 * @param  formatted_buf: 存储0000.0格式的缓存（至少8字节）
 * @param  hundredth_buf: 存储百分位字符的缓存（至少2字节）
 * @return 0-成功，-1-空指针错误
 * @note   1. 格式化规则：0000.0（四位整数、一位小数，无符号）
 *         2. 百分位提取：绝对值×100后取个位，修正浮点误差
 */
int FormatFloatWithHundredth(float num, char *formatted_buf, char *hundredth_buf) {
    // 空指针校验
    if (formatted_buf == NULL || hundredth_buf == NULL) {
        return -1;
    }

    // 格式化为主字符串（0000.0）
    snprintf(formatted_buf, 8, "%06.1f", num);

    // 提取百分位（修正浮点误差）
    double scaled = fabs(num) * 100.0 + 1e-5;
    int hundredth_digit = (int)scaled % 10;

    // 转换为字符并补结束符
    hundredth_buf[0] = '0' + hundredth_digit;
    hundredth_buf[1] = '\0';

    return 0;
}

/**
 * @brief  将浮点数格式化为00.00，并提取千分位+万分位
 * @param  num: 输入浮点数
 * @param  formatted_buf: 存储00.00格式的缓存（至少8字节）
 * @param  digits_buf: 存储千/万分位的缓存（至少3字节：[千分位,万分位,\0]）
 * @return 0-成功，-1-空指针错误
 * @note   1. 格式化规则：00.00（两位整数、两位小数，无符号）
 *         2. 千/万分位提取：分别×1000/10000后取个位，修正浮点误差
 */
int FormatFloatWithTwoDigits(float num, char *formatted_buf, char *digits_buf) {
    // 空指针校验
    if (formatted_buf == NULL || digits_buf == NULL) {
        return -1;
    }

    // 格式化为主字符串（00.00）
    snprintf(formatted_buf, 8, "%05.2f", num);

    // 提取千分位
    double scaled_thou = fabs(num) * 1000.0 + 1e-7;
    int thousandth_digit = (int)scaled_thou % 10;
    digits_buf[0] = '0' + thousandth_digit;

    // 提取万分位
    double scaled_ten_thou = fabs(num) * 10000.0 + 1e-7;
    int tenthousandth_digit = (int)scaled_ten_thou % 10;
    digits_buf[1] = '0' + tenthousandth_digit;

    // 补字符串结束符
    digits_buf[2] = '\0';

    return 0;
}

/**
 * @brief  刷新电压数据显示
 * @param  Voltage: 电压实际值
 * @note   1. 格式化电压值为±00.00 + 千分位
 *         2. 绘制到LCD对应位置
 */
static void Refresh_Voltage(float Voltage) {
    FormatFloatWithThousandth(Voltage, Vol_formatted_buf, Vol_thousandth_buf);
    lcd_draw_string(30, 98, Vol_formatted_buf, &DIN_Medium32x48, 0xeb0c, 0x1908, 5);
    lcd_draw_string(240, 98, Vol_thousandth_buf, &DIN_Medium32x48, 0x632c, 0x1908, 0);
}

/**
 * @brief  刷新电流数据显示
 * @param  Current: 电流实际值
 * @note   1. 格式化电流值为±0.000 + 万分位
 *         2. 绘制到LCD对应位置
 */
static void Refresh_Curerrent(float Current) {
    FormatFloatWithTenThousandth(Current, Cur_formatted_buf, Cur_tenthousandth_buf);
    lcd_draw_string(30, 98, Cur_formatted_buf, &DIN_Medium32x48, 0x5cbd, 0x1908, 5);
    lcd_draw_string(240, 98, Cur_tenthousandth_buf, &DIN_Medium32x48, 0x632c, 0x1908, 0);
}

// 全局变量 - 电阻OL状态标记（避免重复绘制OL）
bool Res_Is_Last_OL = false;

/**
 * @brief  刷新电阻数据显示
 * @param  Res_Vol: 电阻测量原始电压值
 * @note   1. 根据当前档位计算电阻值
 *         2. 超量程显示OL，否则显示对应档位的电阻值
 *         3. 200Ω档：显示0000.0 + 百分位；2K档：显示00.00 + 千/万分位
 */
static void Refresh_Resistance(float Res_Vol) {
    // 根据当前档位计算电阻值
    float Res = (dmm_res_mode == DMM_RES_MODE_200 ? UserParam.DMM_Res_R200 : UserParam.DMM_Res_R2K) * Res_Vol / (
        (dmm_res_mode == DMM_RES_MODE_200
         ? UserParam.DMM_Res_R200_Voltage
         : UserParam.DMM_Res_R2K_Voltage) - Res_Vol);

    // 超量程判断（200Ω档>2000，2K档>30000）
    if (Res > (dmm_res_mode == DMM_RES_MODE_200 ? 2000.0f : 30000.0f) || Res < 0.0f) {
        // 避免重复绘制OL
        if (Res_Is_Last_OL) {
            return;
        }
        // 清空数据区背景
        lcd_draw_round_rect(12, 80, 308, 166, 8, 0x1908, 1);
        // 绘制电阻单位
        lcd_draw_string(288, 136, "Ω", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
        // 2K档额外绘制K单位
        if (dmm_res_mode == DMM_RES_MODE_2K) {
            lcd_draw_string(288, 110, "K", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
        }
        // 绘制OL（超量程）
        lcd_draw_string(126, 98, "OL", &DIN_Medium32x48, 0xe22c, 0x1908, 10);
        Res_Is_Last_OL = true;
        return;
    }

    // 退出OL状态
    Res_Is_Last_OL = false;

    // 根据档位刷新显示
    if (dmm_res_mode == DMM_RES_MODE_200) {
        // 200Ω档：0000.0 + 百分位
        FormatFloatWithHundredth(Res, Res_formatted_R200_buf, Res_digits_R200_buf);
        lcd_draw_string(30, 98, Res_formatted_R200_buf, &DIN_Medium32x48, 0xdb3f, 0x1908, 5);
        lcd_draw_string(240, 98, Res_digits_R200_buf, &DIN_Medium32x48, 0x632c, 0x1908, 0);
    } else {
        // 2K档：00.00（除以1000） + 千/万分位
        FormatFloatWithTwoDigits(Res / 1000, Res_formatted_R2K_buf, Res_digits_R2K_buf);
        lcd_draw_string(30, 98, Res_formatted_R2K_buf, &DIN_Medium32x48, 0xdb3f, 0x1908, 5);
        lcd_draw_string(203, 98, Res_digits_R2K_buf, &DIN_Medium32x48, 0x632c, 0x1908, 5);
    }
}

// 全局变量 - 二极管OL状态标记（避免重复绘制OL）
bool Dio_Is_Last_OL = false;

/**
 * @brief  刷新二极管/蜂鸣数据显示
 * @param  Diode_Vol: 二极管测量原始电压值
 * @note   1. 电压>1V显示OL（悬空/大电阻）
 *         2. 电阻>30Ω：显示二极管电压（±0.000 + 千分位）
 *         3. 电阻≤30Ω：显示电阻值（0000.0 + 百分位），触发蜂鸣
 */
static void Refresh_Diode(float Diode_Vol) {
    // 超量程判断（电压>1V）
    if (Diode_Vol > 1.0f) {
        // 避免重复绘制OL
        if (Dio_Is_Last_OL) {
            return;
        }
        // 清空数据区背景
        lcd_draw_round_rect(12, 80, 308, 166, 8, 0x1908, 1);
        // 绘制电压单位
        lcd_draw_string(288, 136, "V", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
        // 绘制OL（超量程）
        lcd_draw_string(126, 98, "OL", &DIN_Medium32x48, 0xe22c, 0x1908, 10);
        Dio_Is_Last_OL = true;
        return;
    }

    // 退出OL状态
    Dio_Is_Last_OL = false;

    // 计算等效电阻
    float Res = UserParam.DMM_Res_R2K * Diode_Vol / (UserParam.DMM_Res_R2K_Voltage - Diode_Vol);

    // 判断显示模式：二极管电压 / 蜂鸣电阻
    if (Res > 30.0f) {
        // 二极管模式：显示电压（±0.000 + 千分位）
        FormatFloatWithTenThousandth(Diode_Vol, Dio_formatted_dio_buf, Dio_thousandth_dio_buf);
        lcd_draw_string(30, 98, Dio_formatted_dio_buf, &DIN_Medium32x48, 0x5cbd, 0x1908, 5);
        lcd_draw_string(240, 98, Dio_thousandth_dio_buf, &DIN_Medium32x48, 0x632c, 0x1908, 0);
        lcd_draw_string(288, 136, "V", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0); // 电压单位
    } else {
        // 蜂鸣模式：显示电阻（0000.0 + 百分位）
        FormatFloatWithHundredth(Res, Dio_formatted_res_buf, Dio_thousandth_res_buf);
        lcd_draw_string(30, 98, Dio_formatted_res_buf, &DIN_Medium32x48, 0xdb3f, 0x1908, 5);
        lcd_draw_string(240, 98, Dio_thousandth_res_buf, &DIN_Medium32x48, 0x632c, 0x1908, 0);
        lcd_draw_string(288, 136, "Ω", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0); // 电阻单位

        // 触发蜂鸣
        StartBeezer(110);
    }
}

/**
 * @brief  电压页面按键处理函数
 * @param  msg: 按键事件消息
 * @note   1. 编码器右键：切换到电流页面
 *         2. SET键单击：切换HOLD状态
 *         3. 编码器长按：退出到LVGL页面
 */
static void dmm_voltage_page_handler(KeyEventMsg_t msg) {
    // 编码器按下右转：切换到电流页面
    if (msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        dmm_current_page = DMM_PAGE_Current;
        // 清空数据区背景
        lcd_draw_round_rect(12, 80, 308, 166, 8, 0x1908, 1);
        // 更新单位为A
        lcd_draw_string(288, 136, "A", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
        // 刷新模式选择框
        DMM_Draw_Mode_Boxes(DMM_PAGE_Current, DMM_PAGE_Voltage);
        // 切换硬件模式
        DMM_Switch_Mode(Dmm_Mode_Current);
        // 关闭HOLD
        Hold_ctrl(false);
        // 停止蜂鸣
        StartBeezer(0);
        return;
    }

    // SET键单击：切换HOLD状态
    if (msg.key == KEY_SET && msg.event == KEY_EVENT_CLICK) {
        Is_Refresh_On == true ? Hold_ctrl(true) : Hold_ctrl(false);
        StartBeezer(0);
        return;
    }

    // 编码器长按：退出到LVGL页面
    if (msg.key == KEY_ENCODER && msg.event == KEY_EVENT_LONG_PRESS) {
        AppListType APP = APP_LVGL;
        StartBeezer(0);
        osMessageQueuePut(AppSwitchQueueHandle, &APP, 0, 10);
        return;
    }
}

/**
 * @brief  电流页面按键处理函数
 * @param  msg: 按键事件消息
 * @note   1. 编码器左键：切换到电压页面
 *         2. SET键单击：切换HOLD状态
 *         3. 编码器长按：退出到LVGL页面
 *         4. 编码器右键：切换到电阻页面
 */
static void dmm_current_page_handler(KeyEventMsg_t msg) {
    // 编码器按下左转：切换到电压页面
    if (msg.event == ENCODER_EVENT_PRESS_LEFT) {
        dmm_current_page = DMM_PAGE_Voltage;
        // 清空数据区背景
        lcd_draw_round_rect(12, 80, 308, 166, 8, 0x1908, 1);
        // 更新单位为V
        lcd_draw_string(288, 136, "V", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
        // 刷新模式选择框
        DMM_Draw_Mode_Boxes(DMM_PAGE_Voltage, DMM_PAGE_Current);
        // 切换硬件模式
        DMM_Switch_Mode(Dmm_Mode_Voltage);
        // 关闭HOLD
        Hold_ctrl(false);
        // 停止蜂鸣
        StartBeezer(0);
        return;
    }

    // SET键单击：切换HOLD状态
    if (msg.key == KEY_SET && msg.event == KEY_EVENT_CLICK) {
        Is_Refresh_On == true ? Hold_ctrl(true) : Hold_ctrl(false);
        StartBeezer(0);
        return;
    }

    // 编码器长按：退出到LVGL页面
    if (msg.key == KEY_ENCODER && msg.event == KEY_EVENT_LONG_PRESS) {
        AppListType APP = APP_LVGL;
        StartBeezer(0);
        osMessageQueuePut(AppSwitchQueueHandle, &APP, 0, 10);
        return;
    }

    // 编码器按下右转：切换到电阻页面
    if (msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        dmm_current_page = DMM_PAGE_Resistance;
        // 默认选中200Ω档位
        dmm_res_mode = DMM_RES_MODE_200;
        // 刷新挡位区背景
        lcd_draw_round_rect(60, 44, 130, 68, 3, 0x1908, 1);
        lcd_draw_round_rect(60, 44, 130, 68, 3, 0x21aa, 0);
        // 显示2KΩ挡位文字（默认）
        lcd_draw_string(74, 47, "2KΩ", &JetBrainsMono14x18, 0xFFFF, 0x1908, 0);
        // 清空数据区背景
        lcd_draw_round_rect(12, 80, 308, 166, 8, 0x1908, 1);
        // 更新单位为Ω
        lcd_draw_string(288, 136, "Ω", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
        // 刷新模式选择框
        DMM_Draw_Mode_Boxes(DMM_PAGE_Resistance, DMM_PAGE_Current);
        // 切换硬件模式
        DMM_Switch_Mode(Dmm_Mode_Resistance);
        // 关闭HOLD
        Hold_ctrl(false);
        // 重置电阻OL状态
        Res_Is_Last_OL = false;
        // 停止蜂鸣
        StartBeezer(0);
        return;
    }
}

/**
 * @brief  电阻页面按键处理函数
 * @param  msg: 按键事件消息
 * @note   1. 编码器左键：切换到电流页面
 *         2. SET键单击：切换HOLD状态
 *         3. 编码器长按：退出到LVGL页面
 *         4. 上键单击：200Ω→2KΩ档位切换
 *         5. 下键单击：2KΩ→200Ω档位切换
 *         6. 编码器右键：切换到二极管页面
 */
static void dmm_Resistance_page_handler(KeyEventMsg_t msg) {
    // 编码器按下左转：切换到电流页面
    if (msg.event == ENCODER_EVENT_PRESS_LEFT) {
        dmm_current_page = DMM_PAGE_Current;
        // 刷新挡位区背景
        lcd_draw_round_rect(60, 44, 130, 68, 3, 0x1908, 1);
        lcd_draw_round_rect(60, 44, 130, 68, 3, 0x21aa, 0);
        // 显示AUTO挡位文字
        lcd_draw_string(69, 47, "AUTO", &JetBrainsMono14x18, 0xFFFF, 0x1908, 0);
        // 清空数据区背景
        lcd_draw_round_rect(12, 80, 308, 166, 8, 0x1908, 1);
        // 更新单位为A
        lcd_draw_string(288, 136, "A", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
        // 刷新模式选择框
        DMM_Draw_Mode_Boxes(DMM_PAGE_Current, DMM_PAGE_Resistance);
        // 切换硬件模式
        DMM_Switch_Mode(Dmm_Mode_Current);
        // 关闭HOLD
        Hold_ctrl(false);
        // 停止蜂鸣
        StartBeezer(0);
        return;
    }

    // SET键单击：切换HOLD状态
    if (msg.key == KEY_SET && msg.event == KEY_EVENT_CLICK) {
        Is_Refresh_On == true ? Hold_ctrl(true) : Hold_ctrl(false);
        StartBeezer(0);
        return;
    }

    // 编码器长按：退出到LVGL页面
    if (msg.key == KEY_ENCODER && msg.event == KEY_EVENT_LONG_PRESS) {
        AppListType APP = APP_LVGL;
        StartBeezer(0);
        osMessageQueuePut(AppSwitchQueueHandle, &APP, 0, 10);
    }

    // 上键单击：200Ω→2KΩ档位切换
    if (msg.key == KEY_UP && msg.event == KEY_EVENT_CLICK && dmm_res_mode == DMM_RES_MODE_200) {
        dmm_res_mode = DMM_RES_MODE_2K;
        // 刷新挡位区边框
        lcd_draw_round_rect(60, 44, 130, 68, 3, 0x21aa, 0);
        // 显示30KΩ挡位文字
        lcd_draw_string(67, 47, "30KΩ", &JetBrainsMono14x18, 0xFFFF, 0x1908, 0);

        // 硬件档位切换：关闭200Ω，打开2KΩ
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_RESET);
        // 关闭HOLD
        Hold_ctrl(false);
        osDelay(50);

        // 清空数据区背景
        lcd_draw_round_rect(12, 80, 308, 166, 8, 0x1908, 1);
        // 更新单位（Ω + K）
        lcd_draw_string(288, 136, "Ω", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
        lcd_draw_string(288, 110, "K", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);

        Res_Is_Last_OL = false;

        // 停止蜂鸣
        StartBeezer(0);
        return;
    }

    // 下键单击：2KΩ→200Ω档位切换
    if (msg.key == KEY_DOWN && msg.event == KEY_EVENT_CLICK && dmm_res_mode == DMM_RES_MODE_2K) {
        dmm_res_mode = DMM_RES_MODE_200;
        // 刷新挡位区边框
        lcd_draw_round_rect(60, 44, 130, 68, 3, 0x21aa, 0);
        // 刷新挡位区背景
        lcd_draw_round_rect(60, 44, 130, 68, 3, 0x1908, 1);
        lcd_draw_round_rect(60, 44, 130, 68, 3, 0x21aa, 0);
        // 显示2KΩ挡位文字（默认）
        lcd_draw_string(74, 47, "2KΩ", &JetBrainsMono14x18, 0xFFFF, 0x1908, 0);

        // 硬件档位切换：打开200Ω，关闭2KΩ
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_SET);
        // 关闭HOLD
        Hold_ctrl(false);
        osDelay(50);

        // 清空数据区背景
        lcd_draw_round_rect(12, 80, 308, 166, 8, 0x1908, 1);
        // 更新单位（仅Ω）
        lcd_draw_string(288, 136, "Ω", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);

        Res_Is_Last_OL = false;

        // 停止蜂鸣
        StartBeezer(0);
        return;
    }

    // 编码器按下右转：切换到二极管页面
    if (msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        dmm_current_page = DMM_PAGE_Diode;
        // 刷新挡位区背景
        lcd_draw_round_rect(60, 44, 130, 68, 3, 0x1908, 1);
        lcd_draw_round_rect(60, 44, 130, 68, 3, 0x21aa, 0);
        // 显示二极管挡位文字
        lcd_draw_string(84, 47, "▶|", &JetBrainsMono14x18, 0xFFFF, 0x1908, 0);
        // 清空数据区背景
        lcd_draw_round_rect(12, 80, 308, 166, 8, 0x1908, 1);
        // 更新单位为V
        lcd_draw_string(288, 136, "V", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);

        // 刷新模式选择框
        DMM_Draw_Mode_Boxes(DMM_PAGE_Diode, DMM_PAGE_Resistance);
        // 切换硬件模式
        DMM_Switch_Mode(Dmm_Mode_Diode);
        // 关闭HOLD
        Hold_ctrl(false);
        // 重置二极管OL状态
        Dio_Is_Last_OL = false;
        dmm_res_mode = DMM_RES_MODE_2K;
        // 停止蜂鸣
        StartBeezer(0);
        return;
    }
}

/**
 * @brief  二极管页面按键处理函数
 * @param  msg: 按键事件消息
 * @note   1. 编码器左键：切换到电阻页面
 *         2. SET键单击：切换HOLD状态
 *         3. 编码器长按：退出到LVGL页面
 */
static void dmm_diode_page_handler(KeyEventMsg_t msg) {
    // 编码器按下左转：切换到电阻页面
    if (msg.event == ENCODER_EVENT_PRESS_LEFT) {
        dmm_current_page = DMM_PAGE_Resistance;
        dmm_res_mode = DMM_RES_MODE_200;
        // 刷新挡位区背景
        lcd_draw_round_rect(60, 44, 130, 68, 3, 0x1908, 1);
        lcd_draw_round_rect(60, 44, 130, 68, 3, 0x21aa, 0);
        // 显示2KΩ挡位文字
        lcd_draw_string(74, 47, "2KΩ", &JetBrainsMono14x18, 0xFFFF, 0x1908, 0);
        // 清空数据区背景
        lcd_draw_round_rect(12, 80, 308, 166, 8, 0x1908, 1);
        // 更新单位为Ω
        lcd_draw_string(288, 136, "Ω", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
        // 刷新模式选择框
        DMM_Draw_Mode_Boxes(DMM_PAGE_Resistance, DMM_PAGE_Diode);
        // 切换硬件模式
        DMM_Switch_Mode(Dmm_Mode_Resistance);
        // 关闭HOLD
        Hold_ctrl(false);
        // 重置电阻OL状态
        Res_Is_Last_OL = false;
        dmm_res_mode = DMM_RES_MODE_200;
        // 停止蜂鸣
        StartBeezer(0);
        return;
    }

    // SET键单击：切换HOLD状态
    if (msg.key == KEY_SET && msg.event == KEY_EVENT_CLICK) {
        Is_Refresh_On == true ? Hold_ctrl(true) : Hold_ctrl(false);
        StartBeezer(0);
        return;
    }

    // 编码器长按：退出到LVGL页面
    if (msg.key == KEY_ENCODER && msg.event == KEY_EVENT_LONG_PRESS) {
        AppListType APP = APP_LVGL;
        StartBeezer(0);
        osMessageQueuePut(AppSwitchQueueHandle, &APP, 0, 10);
    }
}

/**
 * @brief  万用表ADC初始化函数
 * @note   1. 配置ADC1通道2/1/0（GPIOA2/A1/A0），对应电压/电流/电阻采集
 *         2. 触发源：定时器8 TRGO上升沿
 *         3. 采集模式：扫描+DMA环形，12位分辨率，480周期采样时间
 *         4. DMA配置：外设→内存，半字对齐，环形模式，高优先级
 */
void DMM_ADC_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};   // ADC通道配置结构体
    GPIO_InitTypeDef GPIO_InitStruct = {0}; // GPIO配置结构体

    // -------------------------- ADC1 基础配置 --------------------------
    hadc1.Instance = ADC1;                                              // 选择ADC1
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;               // 时钟分频PCLK/2
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;                         // 12位分辨率
    hadc1.Init.ScanConvMode = ENABLE;                                   // 使能扫描模式
    hadc1.Init.ContinuousConvMode = DISABLE;                            // 关闭连续转换（外部触发）
    hadc1.Init.DiscontinuousConvMode = DISABLE;                         // 关闭间断模式
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;  // 上升沿触发
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T8_TRGO;         // 触发源：定时器8 TRGO
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;                         // 数据右对齐
    hadc1.Init.NbrOfConversion = 3;                                     // 3个转换通道
    hadc1.Init.DMAContinuousRequests = ENABLE;                          // 使能DMA连续请求
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;                         // 序列转换完成触发EOC
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {                               // ADC初始化
        Error_Handler();
    }

    // -------------------------- ADC通道配置 --------------------------
    // 通道2（GPIOA2）：电压采集，优先级1
    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    // 通道1（GPIOA1）：电流采集，优先级2
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = 2;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    // 通道0（GPIOA0）：电阻采集，优先级3
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 3;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    // -------------------------- GPIO 初始化 --------------------------
    __HAL_RCC_ADC1_CLK_ENABLE();                                // 使能ADC1时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();                               // 使能GPIOA时钟
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2; // ADC引脚
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;                    // 模拟输入
    GPIO_InitStruct.Pull = GPIO_NOPULL;                         // 无上下拉
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // -------------------------- DMA 配置 --------------------------
    hdma_adc1.Instance = DMA2_Stream4;                              // DMA2流4（ADC1默认）
    hdma_adc1.Init.Channel = DMA_CHANNEL_0;                         // DMA通道0
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;                // 外设→内存
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;                    // 外设地址不递增
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;                        // 内存地址递增
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;   // 外设半字对齐
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;      // 内存半字对齐
    hdma_adc1.Init.Mode = DMA_CIRCULAR;                             // 环形模式
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;                    // 高优先级
    hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;                 // 关闭FIFO
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) {
        Error_Handler();
    }

    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1); // 关联ADC和DMA句柄
}

/**
 * @brief  万用表ADC反初始化函数
 * @note   1. 安全停止ADC转换和DMA传输
 *         2. 释放ADC、GPIO、DMA相关资源
 *         3. 等待DMA传输完成（超时100ms），避免数据异常
 */
void DMM_ADC_DeInit(void) {
    HAL_ADC_Stop(&hadc1);                           // 停止ADC转换
    HAL_ADC_Stop_DMA(&hadc1);                       // 停止ADC DMA传输
    // 等待DMA传输完成（超时100ms）
    HAL_DMA_PollForTransfer(&hdma_adc1, HAL_DMA_FULL_TRANSFER, 100);

    HAL_ADC_DeInit(&hadc1);                                     // ADC反初始化
    __HAL_RCC_ADC1_CLK_DISABLE();                               // 关闭ADC1时钟
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2); // 释放GPIO
    HAL_DMA_DeInit(&hdma_adc1);                                 // DMA反初始化
}

// 宏定义 - IIR滤波系数（越小越平滑，响应越慢）
#define IIR_ALPHA_VOLTAGE    0.1f         // 电压滤波系数
#define IIR_ALPHA_CURRENT    0.1f          // 电流滤波系数
#define IIR_ALPHA_RESISTANCE 0.1f          // 电阻滤波系数

// 全局变量 - IIR滤波历史值
static float iir_prev_voltage = 0.0f;       // 电压上一次滤波值
static float iir_prev_current = 0.0f;       // 电流上一次滤波值
static float iir_prev_resistance = 0.0f;    // 电阻上一次滤波值

/**
 * @brief  IIR低通滤波函数
 * @param  current_val: 当前采样值
 * @param  prev_val: 上一次滤波值
 * @param  alpha: 滤波系数（0~1）
 * @return 本次滤波结果
 * @note   公式：y(n) = α×x(n) + (1-α)×y(n-1)
 */
static float iir_filter(float current_val, float prev_val, float alpha) {
    return alpha * current_val + (1.0f - alpha) * prev_val;
}

/**
 * @brief  uint16_t类型比较函数（用于qsort）
 * @param  a: 比较值1
 * @param  b: 比较值2
 * @return -1(a<b) / 0(a==b) / 1(a>b)
 */
int compare_uint16_ternary(const void *a, const void *b) {
    const uint16_t num1 = *(const uint16_t *)a;
    const uint16_t num2 = *(const uint16_t *)b;

    return (num1 < num2) ? -1 : (num1 > num2) ? 1 : 0;
}

/**
 * @brief  ADC转换完成回调函数
 * @param  hadc: ADC句柄
 * @note   1. 仅处理ADC1的转换完成事件
 *         2. 数据处理流程：解交错→排序→去毛刺→均值→过采样→校准→IIR滤波
 *         3. 去毛刺：去掉前4和后4个极值，取中间192个数据求和
 *         4. 过采样：12位→14位，提升精度
 */
void CB_DMM_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        AdcTim_OFF(); // 关闭ADC触发定时器

        // 1. 解交错：将600个交错数据拆分为3个通道各200个点
        uint16_t ch1_data[200]; // 电压通道
        uint16_t ch2_data[200]; // 电流通道
        uint16_t ch3_data[200]; // 电阻通道
        int idx = 0;
        for (int i = 0; i < 600; i += 3) {
            ch1_data[idx] = dmm_adc_raw_buf[i];    // 通道2（电压）
            ch2_data[idx] = dmm_adc_raw_buf[i+1];  // 通道1（电流）
            ch3_data[idx] = dmm_adc_raw_buf[i+2];  // 通道0（电阻）
            idx++;
        }

        // 2. 排序（用于后续去毛刺）
        qsort(ch1_data, 200, sizeof(uint16_t), compare_uint16_ternary);
        qsort(ch2_data, 200, sizeof(uint16_t), compare_uint16_ternary);
        qsort(ch3_data, 200, sizeof(uint16_t), compare_uint16_ternary);

        // 3. 求和（去毛刺：去掉前4和后4个极值）
        uint32_t ch1_sum = 0, ch2_sum = 0, ch3_sum = 0;
        for (uint8_t i = 4; i < 196; i++) {
            ch1_sum += ch1_data[i];
            ch2_sum += ch2_data[i];
            ch3_sum += ch3_data[i];
        }

        // 4. 过采样到14bit + 均值滤波（192个数据求和后/12）
        uint16_t ch1_14bit = 0, ch2_14bit = 0, ch3_14bit = 0;
        ch1_14bit = (ch1_sum >> 2) / 12;
        ch2_14bit = (ch2_sum >> 2) / 12;
        ch3_14bit = (ch3_sum >> 2) / 12;

        // 5. 校准计算（转换为实际物理值）
        // 电压校准
        int32_t temp1 = (int32_t)ch1_14bit - (8192 - UserParam.DMM_Voltage_Original);
        float raw_voltage = (float) temp1 * (3.3f / 16383.0f) * (temp1 >= 0
                                                               ? UserParam.DMM_Voltage_Factor_R
                                                               : UserParam.DMM_Voltage_Factor_B);
        // 电流校准
        int32_t temp2 = (int32_t)ch2_14bit - (8192 + UserParam.DMM_Current_Original);
        float raw_current = (float) temp2 * (3.3f / 16383.0f) * UserParam.DMM_Current_Factor;

        // 电阻校准
        int16_t Res_Original;
        dmm_res_mode == DMM_RES_MODE_200 ? Res_Original = UserParam.DMM_Res_R200_Original : (Res_Original = UserParam.DMM_Res_R2K_Original);
        float raw_resistance_voltage = (float)(ch3_14bit + Res_Original) * (3.3f / 16383.0f);

        // 6. IIR滤波
        DMM_Voltage = iir_filter(raw_voltage, iir_prev_voltage, IIR_ALPHA_VOLTAGE);
        DMM_Current = iir_filter(raw_current, iir_prev_current, IIR_ALPHA_CURRENT);
        DMM_Resistance_Voltage = iir_filter(raw_resistance_voltage, iir_prev_resistance, IIR_ALPHA_RESISTANCE);

        // 7. 更新滤波历史值
        iir_prev_voltage = DMM_Voltage;
        iir_prev_current = DMM_Current;
        iir_prev_resistance = DMM_Resistance_Voltage;

        AdcTim_ON(); // 重新开启ADC触发定时器
    }
}

/**
 * @brief  恢复万用表任务
 * @note   1. 初始化ADC→绘制界面→注册回调→启动DMA采集
 *         2. 配置定时器8：20Khz触发，100次/秒有效数据
 *         3. 默认启动电压模式，恢复输入检测任务
 */
void Resume_DmmTask(void) {
    // 测量物理值复位
    DMM_Voltage = 0.0f;
    DMM_Current = 0.0f;
    DMM_Resistance_Voltage = 0.0f;
    // 刷新标志复位
    Is_Refresh_On = true;
    // 计时计数器复位
    dps_time_count = 0;
    // 当前页面复位（默认电压页面）
    dmm_current_page = DMM_PAGE_Voltage;
    // 电阻档位复位（默认200Ω档）
    dmm_res_mode = DMM_RES_MODE_200;
    // OL状态标记复位
    Res_Is_Last_OL = false;
    Dio_Is_Last_OL = false;
    // IIR滤波历史值复位
    iir_prev_voltage = 0.0f;
    iir_prev_current = 0.0f;
    iir_prev_resistance = 0.0f;
    // ADC原始数据缓冲区清零
    for (int i = 0; i < 600; i++) {
        dmm_adc_raw_buf[i] = 0;
    }

    DMM_ADC_Init();                          // 初始化ADC
    DrawDmmMainBasicElement();               // 绘制主界面
    osDelay(100);                            // 延时确保界面绘制完成
    // 注册ADC转换完成回调
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_COMPLETE_CB_ID, CB_DMM_ADC_ConvCpltCallback);
    AdcTim_OFF();                            // 先关闭ADC触发

    // 启动ADC DMA采集（600个数据）
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *) dmm_adc_raw_buf, 600);
    // 配置定时器8：20Khz触发（预分频0，自动重装7199）
    __HAL_TIM_SET_PRESCALER(&htim8, 0);
    __HAL_TIM_SET_AUTORELOAD(&htim8, 7200 - 1);
    AdcTim_ON();                             // 开启ADC触发

    DMM_Switch_Mode(Dmm_Mode_Voltage);       // 默认电压模式
    Resume_IndevDetectTask();                // 恢复输入设备检测
    osThreadResume(DmmCoreTaskHandle);       // 恢复DMM核心任务
}

/**
 * @brief  挂起万用表任务
 * @note   1. 切换到电压模式，关闭电压档硬件
 *         2. 反初始化ADC，释放资源
 *         3. 挂起DMM核心任务
 */
void Suspend_DmmTask(void) {
    DMM_Switch_Mode(Dmm_Mode_Voltage);                   // 切换到电压模式
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET); // 关闭电压档硬件

    DMM_ADC_DeInit(); // 反初始化ADC

    osThreadSuspend(DmmCoreTaskHandle); // 挂起DMM核心任务
}