#include <stdio.h>
#include "format.h"
#include <string.h>
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
static void dmm_clear_data_area(void);
static void dmm_redraw_units(void);
static void dmm_draw_range_label(void);
static void dmm_draw_rel_indicator(void);
static float dmm_res_compute(float res_vol);
static bool dmm_start_capture(void);

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

// 页面按键处理：所有页面共用一套（编码器转页 + SET 保持 + MODE 相对值）
static void dmm_common_page_handler(KeyEventMsg_t msg);

// 页面处理函数表（与DMM_AppPage_t枚举索引对应）
static void (*page_handlers[DMM_PAGE_NUM])(KeyEventMsg_t) = {
    [DMM_PAGE_Voltage]    = dmm_common_page_handler,
    [DMM_PAGE_Current]    = dmm_common_page_handler,
    [DMM_PAGE_Resistance] = dmm_common_page_handler,
    [DMM_PAGE_Diode]      = dmm_common_page_handler,
};

/* 电阻档位枚举定义在 SwitchManager.h（按满量程命名：2KΩ / 30KΩ） */
volatile DMM_Res_RangeTypeDef dmm_res_mode = DMM_RES_RANGE_2K;  // 当前电阻档位（自动量程决定）

// 超量程标志（中断里置位，任务里画 OL）
volatile bool DMM_Voltage_OL = false;
volatile bool DMM_Current_OL = false;

// IIR 预热标志：换档/换页后第一个样本直接作为初值，避免读数从旧值爬到新值
volatile bool dmm_iir_prime = true;

// 采集完成计数（任务里做静默停采看门狗）
volatile uint32_t dmm_block_count = 0;

// 相对值（REL）：按 MODE 键把当前读数清零，显示相对变化量
static bool dmm_rel_on = false;
static uint8_t dmm_rel_page = 0xFF;
static float dmm_rel_ref = 0.0f;

// 上一次已绘制的字符串：数值没变就不重画
// （DIN_Medium32x48 每个字模 1536 字节，字形缓存只有 6144 字节，整串重画要重新读 SPI Flash）
static char last_vol_main[10] = {0};
static char last_vol_digit[4] = {0};
static char last_cur_main[10] = {0};
static char last_cur_digit[5] = {0};
static char last_res_main[10] = {0};
static char last_res_digit[5] = {0};
static char last_dio_main[10] = {0};
static char last_dio_digit[5] = {0};

// OL / 二极管单位状态，避免重复绘制
static bool vol_ol_shown = false;
static bool cur_ol_shown = false;
static bool res_ol_shown = false;
static bool dio_ol_shown = false;
static uint8_t dio_unit_shown = 0;      // 'V' 或 'O'(Ω)，0 表示未知
static bool dio_short = false;          // 通断状态（带迟滞，防止边界抖动断续响）

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

    uint32_t last_block_count = dmm_block_count;
    uint32_t silent_since = osKernelGetTickCount();

    for (;;) {
        // 刷新率计时累加（10 ms 一次循环）
        dps_time_count++;

        // 读取按键消息队列，分发到当前页面的按键处理函数
        if (osMessageQueueGet(KeyEventQueueHandle, &DMM_Keymsg, NULL, 0) == osOK) {
            page_handlers[dmm_current_page](DMM_Keymsg);
        }

        // 静默停采看门狗：中断里每 10ms 完成一块并加计数，200ms 不动就重启采集
        // （ADC 溢出/DMA 出错后 HAL 不会自己恢复，没有这个就只能定格在最后一帧）
        if (dmm_block_count != last_block_count) {
            last_block_count = dmm_block_count;
            silent_since = osKernelGetTickCount();
        } else if ((osKernelGetTickCount() - silent_since) >= 200U) {
            silent_since = osKernelGetTickCount();
            dmm_iir_prime = true;
            if (dmm_start_capture()) {
                AdcTim_ON();
            }
        }

        // 每 50ms 刷新一次显示（读数本身被 IIR 拖慢，更快的刷新只是重复画同样的字）
        if (dps_time_count > 5) {
            if (Is_Refresh_On) {
                // 根据当前页面刷新对应数据
                switch (dmm_current_page) {
                    case DMM_PAGE_Voltage:
                        Refresh_Voltage((float)DMM_Voltage);
                        break;
                    case DMM_PAGE_Current:
                        Refresh_Curerrent((float)DMM_Current);
                        break;
                    case DMM_PAGE_Resistance:
                        Refresh_Resistance((float)DMM_Resistance_Voltage);
                        break;
                    case DMM_PAGE_Diode:
                        Refresh_Diode((float)DMM_Resistance_Voltage);
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

    // 绘制顶栏
    lcd_draw_string(127, 6, "万用表", &yahei20x20, 0x24be, 0x1908, 3); // 标题
    lcd_draw_line(0, 31, 319, 31, 0x11ac);                             // 顶栏下划线

    // 绘制参数区
    lcd_draw_image(20, 42, &img_DMM_DC_30x30);                         // 直流图标
    dmm_draw_range_label();                                            // 挡位/量程（V/A 显示满量程，Ω 显示 AUTO）
    dmm_draw_rel_indicator();                                          // REL 指示（默认熄灭）
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
    if (formatted_buf == NULL || thousandth_buf == NULL) {
        return -1;
    }
    // ±00.00 + 千分位（第 3 位小数）
    fmt_fixed_ex(formatted_buf, 8, num, 2, 6, FMT_SIGN | FMT_ZERO_PAD, thousandth_buf, 1);
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
    if (formatted_buf == NULL || tenthousandth_buf == NULL) {
        return -1;
    }
    // ±0.000 + 万分位（第 4 位小数）
    fmt_fixed_ex(formatted_buf, 8, num, 3, 6, FMT_SIGN | FMT_ZERO_PAD, tenthousandth_buf, 1);
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
    if (formatted_buf == NULL || hundredth_buf == NULL) {
        return -1;
    }
    // 0000.0 + 百分位（第 2 位小数）
    fmt_fixed_ex(formatted_buf, 8, num, 1, 6, FMT_ZERO_PAD, hundredth_buf, 1);
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
    if (formatted_buf == NULL || digits_buf == NULL) {
        return -1;
    }
    // 00.00 + 千分位/万分位
    fmt_fixed_ex(formatted_buf, 8, num, 2, 5, FMT_ZERO_PAD, digits_buf, 2);
    return 0;
}

/* ==================== 显示/量程辅助 ==================== */

/**
 * @brief  清空数据区，并让所有"上一次显示的字符串"失效
 * @note   影子串失效保证下一帧一定会重画；单位文字也在数据区内，被一起清掉，调用后需补画
 */
static void dmm_clear_data_area(void) {
    lcd_draw_round_rect(12, 80, 308, 166, 8, 0x1908, 1);

    last_vol_main[0] = '\0';
    last_vol_digit[0] = '\0';
    last_cur_main[0] = '\0';
    last_cur_digit[0] = '\0';
    last_res_main[0] = '\0';
    last_res_digit[0] = '\0';
    last_dio_main[0] = '\0';
    last_dio_digit[0] = '\0';
}

/**
 * @brief  按当前页面与电阻档位重画右侧单位
 */
static void dmm_redraw_units(void) {
    switch (dmm_current_page) {
        case DMM_PAGE_Voltage:
            lcd_draw_string(288, 136, "V", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
            break;
        case DMM_PAGE_Current:
            lcd_draw_string(288, 136, "A", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
            break;
        case DMM_PAGE_Resistance:
            lcd_draw_string(288, 136, "Ω", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
            if (dmm_res_mode == DMM_RES_RANGE_30K) {
                lcd_draw_string(288, 110, "K", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
            }
            break;
        case DMM_PAGE_Diode:
            lcd_draw_string(288, 136, "V", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
            dio_unit_shown = 'V';
            break;
        default:
            break;
    }
}

/**
 * @brief  比较并更新影子字符串
 * @return true 表示内容和上次不同（调用者需要重画）
 */
static bool dmm_take_text(char *shadow, uint8_t size, const char *text) {
    if (strcmp(shadow, text) == 0) {
        return false;
    }
    strncpy(shadow, text, (size_t)size - 1U);
    shadow[size - 1U] = '\0';
    return true;
}

/**
 * @brief  按当前档位计算电阻值（Ω）
 * @note   (Vref - Vx) 为 0 时结果是 inf，悬空且 Vref 也是 0 时是 NaN，
 *         两种都交给调用方按超量程处理
 */
static float dmm_res_compute(float res_vol) {
    float r_ref = (dmm_res_mode == DMM_RES_RANGE_2K) ? UserParam.DMM_Res_R200 : UserParam.DMM_Res_R2K;
    float v_ref = (dmm_res_mode == DMM_RES_RANGE_2K) ? UserParam.DMM_Res_R200_Voltage : UserParam.DMM_Res_R2K_Voltage;
    return r_ref * res_vol / (v_ref - res_vol);
}

/**
 * @brief  绘制左上角的挡位/量程标签
 * @note   电压/电流没有硬件量程，显示由校准系数算出的满量程（如 "74V" / "4.4A"），
 *         电阻档做自动量程，显示 "AUTO"（原来这里是写死的 "AUTO"，电压电流其实并不自动）
 */
static void dmm_draw_range_label(void) {
    char buf[10];
    uint8_t len;

    switch (dmm_current_page) {
        case DMM_PAGE_Voltage:
            fmt_fixed_unit(buf, sizeof(buf), 3.3f * UserParam.DMM_Voltage_Factor_R, 0, 0, 0, "V");
            break;
        case DMM_PAGE_Current:
            fmt_fixed_unit(buf, sizeof(buf), 3.3f * UserParam.DMM_Current_Factor, 1, 0, 0, "A");
            break;
        case DMM_PAGE_Resistance:
            strcpy(buf, "AUTO");
            break;
        case DMM_PAGE_Diode:
            strcpy(buf, "▶|");
            break;
        default:
            buf[0] = '\0';
            break;
    }

    lcd_draw_round_rect(60, 44, 130, 68, 3, 0x1908, 1);   // 背景填充
    lcd_draw_round_rect(60, 44, 130, 68, 3, 0x21aa, 0);   // 边框

    len = (uint8_t)strlen(buf);
    if (len > 0 && len <= 5) {
        // 框宽 70，字符步进 14，居中：x = 60 + (70 - 14n)/2
        lcd_draw_string((uint16_t)(95 - 7 * len), 47, buf, &JetBrainsMono14x18, 0xFFFF, 0x1908, 0);
    }
}

/**
 * @brief  绘制 REL 指示（熄灭时只清空那块区域）
 */
static void dmm_draw_rel_indicator(void) {
    lcd_draw_rect(132, 44, 240, 68, 0x1908, 1);
    if (dmm_rel_on) {
        lcd_draw_string(150, 47, "REL", &JetBrainsMono14x18, 0xe22c, 0x1908, 0);
    }
}

/**
 * @brief  切换相对值（REL）状态：按 MODE 键把当前读数归零
 * @note   二极管档不做相对值；换页面时自动清除
 */
static void dmm_rel_toggle(void) {
    if (dmm_rel_on) {
        dmm_rel_on = false;
    } else {
        switch (dmm_current_page) {
            case DMM_PAGE_Voltage:  dmm_rel_ref = (float)DMM_Voltage; break;
            case DMM_PAGE_Current:  dmm_rel_ref = (float)DMM_Current; break;
            case DMM_PAGE_Resistance: dmm_rel_ref = dmm_res_compute((float)DMM_Resistance_Voltage); break;
            default: StartBeezer(0); return;      // 二极管档不支持
        }
        // NaN / inf 保护：悬空时算出来的电阻可能是 inf，不能当基准
        if (!(dmm_rel_ref == dmm_rel_ref) || dmm_rel_ref > 1.0e9f || dmm_rel_ref < -1.0e9f) {
            StartBeezer(0);
            return;
        }
        dmm_rel_on = true;
        dmm_rel_page = (uint8_t)dmm_current_page;
    }

    dmm_draw_rel_indicator();
    dmm_clear_data_area();
    dmm_redraw_units();
    StartBeezer(0);
}

/**
 * @brief  切换电阻档（自动量程调用）
 */
static void dmm_switch_res_range(DMM_Res_RangeTypeDef range) {
    if (dmm_res_mode == range) {
        return;
    }
    dmm_res_mode = range;
    DMM_Res_Range_Ctrl(range);      // 硬件切档，内部含 20ms 稳定延时
    dmm_iir_prime = true;           // 换档后的第一个样本直接当滤波初值
    dmm_clear_data_area();
    dmm_redraw_units();
}

/**
 * @brief  自动量程：贴近满量程就升档，读数很小就降档
 * @param  res: 当前档位算出的电阻值（可能是 inf / NaN）
 * @return true 表示刚刚切了档（本帧不画，等新档位的读数）
 * @note   2KΩ 档在 1800Ω 升到 30KΩ；30KΩ 档在 180Ω 降回 2KΩ。
 *         升降门限差 10 倍，正好是两档基准电阻之比，不会来回跳。
 */
static bool dmm_res_autorange(float res) {
    if (!(res >= 0.0f)) {           // NaN：不切档，交给 OL 分支
        return false;
    }
    if (dmm_res_mode == DMM_RES_RANGE_2K && res > 1800.0f) {
        dmm_switch_res_range(DMM_RES_RANGE_30K);
        return true;
    }
    if (dmm_res_mode == DMM_RES_RANGE_30K && res < 180.0f) {
        dmm_switch_res_range(DMM_RES_RANGE_2K);
        return true;
    }
    return false;
}

/**
 * @brief  刷新电压数据显示
 * @param  Voltage: 电压实际值
 * @note   1. 格式化电压值为±00.00 + 千分位
 *         2. 绘制到LCD对应位置
 */
static void Refresh_Voltage(float Voltage) {
    // 超量程（ADC 削顶）：数值已经饱和，直接显示 OL
    if (DMM_Voltage_OL) {
        if (vol_ol_shown) { return; }
        dmm_clear_data_area();
        dmm_redraw_units();
        lcd_draw_string(126, 98, "OL", &DIN_Medium32x48, 0xe22c, 0x1908, 10);
        vol_ol_shown = true;
        return;
    }
    vol_ol_shown = false;

    // 相对值
    if (dmm_rel_on && dmm_rel_page == (uint8_t)DMM_PAGE_Voltage) {
        Voltage -= dmm_rel_ref;
    }

    FormatFloatWithThousandth(Voltage, Vol_formatted_buf, Vol_thousandth_buf);

    // 数值没变就不重画（整串重画要从 SPI Flash 重新读字模，很贵）
    bool changed_main = dmm_take_text(last_vol_main, sizeof(last_vol_main), Vol_formatted_buf);
    bool changed_digit = dmm_take_text(last_vol_digit, sizeof(last_vol_digit), Vol_thousandth_buf);
    if (!changed_main && !changed_digit) { return; }

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
    if (DMM_Current_OL) {
        if (cur_ol_shown) { return; }
        dmm_clear_data_area();
        dmm_redraw_units();
        lcd_draw_string(126, 98, "OL", &DIN_Medium32x48, 0xe22c, 0x1908, 10);
        cur_ol_shown = true;
        return;
    }
    cur_ol_shown = false;

    if (dmm_rel_on && dmm_rel_page == (uint8_t)DMM_PAGE_Current) {
        Current -= dmm_rel_ref;
    }

    FormatFloatWithTenThousandth(Current, Cur_formatted_buf, Cur_tenthousandth_buf);

    bool changed_main = dmm_take_text(last_cur_main, sizeof(last_cur_main), Cur_formatted_buf);
    bool changed_digit = dmm_take_text(last_cur_digit, sizeof(last_cur_digit), Cur_tenthousandth_buf);
    if (!changed_main && !changed_digit) { return; }

    lcd_draw_string(30, 98, Cur_formatted_buf, &DIN_Medium32x48, 0x5cbd, 0x1908, 5);
    lcd_draw_string(240, 98, Cur_tenthousandth_buf, &DIN_Medium32x48, 0x632c, 0x1908, 0);
}

// 全局变量 - 电阻OL状态标记（避免重复绘制OL）

/**
 * @brief  刷新电阻数据显示
 * @param  Res_Vol: 电阻测量原始电压值
 * @note   1. 按当前档位算电阻值，先做自动量程判断
 *         2. 超量程/悬空显示 OL
 *         3. 2KΩ 档显示 Ω 整数 + 百分位；30KΩ 档显示 KΩ + 千/万分位
 */
static void Refresh_Resistance(float Res_Vol) {
    float Res = dmm_res_compute(Res_Vol);

    // 自动量程：贴近满量程升档、读数很小降档（切了档本帧不画，等新档位的读数）
    if (dmm_res_autorange(Res)) {
        return;
    }

    // 超量程判断（2KΩ 档 >2000，30KΩ 档 >30000；NaN 也算不上有效读数）
    float full_scale = (dmm_res_mode == DMM_RES_RANGE_2K) ? 2000.0f : 30000.0f;
    if (!(Res >= 0.0f) || Res > full_scale) {
        if (res_ol_shown) {
            return;
        }
        dmm_clear_data_area();
        dmm_redraw_units();
        lcd_draw_string(126, 98, "OL", &DIN_Medium32x48, 0xe22c, 0x1908, 10);
        res_ol_shown = true;
        return;
    }
    res_ol_shown = false;

    if (dmm_rel_on && dmm_rel_page == (uint8_t)DMM_PAGE_Resistance) {
        Res -= dmm_rel_ref;
    }

    if (dmm_res_mode == DMM_RES_RANGE_2K) {
        // 2KΩ 档：0000.0 + 百分位
        FormatFloatWithHundredth(Res, Res_formatted_R200_buf, Res_digits_R200_buf);
        bool changed_main = dmm_take_text(last_res_main, sizeof(last_res_main), Res_formatted_R200_buf);
        bool changed_digit = dmm_take_text(last_res_digit, sizeof(last_res_digit), Res_digits_R200_buf);
        if (!changed_main && !changed_digit) { return; }
        lcd_draw_string(30, 98, Res_formatted_R200_buf, &DIN_Medium32x48, 0xdb3f, 0x1908, 5);
        lcd_draw_string(240, 98, Res_digits_R200_buf, &DIN_Medium32x48, 0x632c, 0x1908, 0);
    } else {
        // 30KΩ 档：00.00（单位 KΩ）+ 千/万分位
        FormatFloatWithTwoDigits(Res / 1000.0f, Res_formatted_R2K_buf, Res_digits_R2K_buf);
        bool changed_main = dmm_take_text(last_res_main, sizeof(last_res_main), Res_formatted_R2K_buf);
        bool changed_digit = dmm_take_text(last_res_digit, sizeof(last_res_digit), Res_digits_R2K_buf);
        if (!changed_main && !changed_digit) { return; }
        lcd_draw_string(30, 98, Res_formatted_R2K_buf, &DIN_Medium32x48, 0xdb3f, 0x1908, 5);
        lcd_draw_string(203, 98, Res_digits_R2K_buf, &DIN_Medium32x48, 0x632c, 0x1908, 5);
    }
}

/**
 * @brief  刷新二极管/蜂鸣数据显示
 * @param  Diode_Vol: 二极管测量原始电压值
 * @note   1. 电压 >1V 视为开路，显示 OL
 *         2. 等效电阻 ≤30Ω（退出门限 33Ω）判定为短路：显示电阻并持续蜂鸣
 *         3. 否则显示二极管正向压降
 */
static void Refresh_Diode(float Diode_Vol) {
    if (Diode_Vol > 1.0f) {
        if (dio_ol_shown) {
            return;
        }
        dmm_clear_data_area();
        dmm_redraw_units();
        lcd_draw_string(126, 98, "OL", &DIN_Medium32x48, 0xe22c, 0x1908, 10);
        dio_ol_shown = true;
        return;
    }
    dio_ol_shown = false;

    // 等效电阻（用 2KΩ 基准档的校准参数，与原来一致）
    float Res = UserParam.DMM_Res_R2K * Diode_Vol / (UserParam.DMM_Res_R2K_Voltage - Diode_Vol);

    // 通断判断带迟滞：进入 30Ω、退出 33Ω，避免在门限附近抖动导致蜂鸣断断续续
    if (!dio_short && Res <= 30.0f) {
        dio_short = true;
    } else if (dio_short && Res > 33.0f) {
        dio_short = false;
    }

    if (dio_short) {
        FormatFloatWithHundredth(Res, Dio_formatted_res_buf, Dio_thousandth_res_buf);
        bool changed_main = dmm_take_text(last_dio_main, sizeof(last_dio_main), Dio_formatted_res_buf);
        bool changed_digit = dmm_take_text(last_dio_digit, sizeof(last_dio_digit), Dio_thousandth_res_buf);
        if (changed_main || changed_digit) {
            lcd_draw_string(30, 98, Dio_formatted_res_buf, &DIN_Medium32x48, 0xdb3f, 0x1908, 5);
            lcd_draw_string(240, 98, Dio_thousandth_res_buf, &DIN_Medium32x48, 0x632c, 0x1908, 0);
        }
        if (dio_unit_shown != 'O') {
            lcd_draw_string(288, 136, "Ω", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
            dio_unit_shown = 'O';
        }
        StartBeezer(110);       // 短路：每帧续一次，形成连续蜂鸣
    } else {
        FormatFloatWithTenThousandth(Diode_Vol, Dio_formatted_dio_buf, Dio_thousandth_dio_buf);
        bool changed_main = dmm_take_text(last_dio_main, sizeof(last_dio_main), Dio_formatted_dio_buf);
        bool changed_digit = dmm_take_text(last_dio_digit, sizeof(last_dio_digit), Dio_thousandth_dio_buf);
        if (changed_main || changed_digit) {
            lcd_draw_string(30, 98, Dio_formatted_dio_buf, &DIN_Medium32x48, 0x5cbd, 0x1908, 5);
            lcd_draw_string(240, 98, Dio_thousandth_dio_buf, &DIN_Medium32x48, 0x632c, 0x1908, 0);
        }
        if (dio_unit_shown != 'V') {
            lcd_draw_string(288, 136, "V", &JetBrainsMono16x22, 0xFFFF, 0x1908, 0);
            dio_unit_shown = 'V';
        }
    }
}

/**
 * @brief  启动/重启 ADC DMA 采集（600 个半字 = 200 点 × 3 通道）
 * @return true 表示启动成功
 * @note   HAL_ADC_Start_DMA 在流仍处于 BUSY 时直接返回 HAL_BUSY 且不做任何事，
 *         所以先 Stop 再 Start；仍失败就交给调用方的看门狗稍后重试
 */
static bool dmm_start_capture(void) {
    (void)HAL_ADC_Stop_DMA(&hadc1);
    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_EOC | ADC_FLAG_OVR);

    HAL_StatusTypeDef st = HAL_ADC_Start_DMA(&hadc1, (uint32_t *)dmm_adc_raw_buf, 600);
    if (st != HAL_OK) {
        (void)HAL_ADC_Stop_DMA(&hadc1);
        st = HAL_ADC_Start_DMA(&hadc1, (uint32_t *)dmm_adc_raw_buf, 600);
    }
    return (st == HAL_OK);
}

/**
 * @brief  切换页面（编码器转一次换一页，首尾循环）
 * @note   原来每个页面处理函数各抄一遍，漏一处就会「换页后读数从旧值慢慢爬」；
 *         这里统一处理：硬件模式、电阻档位、HOLD、IIR 预热、显示重画
 */
static void dmm_goto_page(DMM_AppPage_t page) {
    DMM_AppPage_t old_page = dmm_current_page;
    if (page == old_page) {
        return;
    }
    dmm_current_page = page;

    // 换页清除相对值，避免在别的档位上显示上一个档位的差值
    dmm_rel_on = false;

    // 关闭 HOLD，恢复刷新
    Hold_ctrl(false);

    switch (page) {
        case DMM_PAGE_Voltage:
            DMM_Switch_Mode(Dmm_Mode_Voltage);
            break;
        case DMM_PAGE_Current:
            DMM_Switch_Mode(Dmm_Mode_Current);
            break;
        case DMM_PAGE_Resistance:
            DMM_Switch_Mode(Dmm_Mode_Resistance);
            DMM_Res_Range_Ctrl(dmm_res_mode);        // 按当前档位重新配置基准电阻
            break;
        case DMM_PAGE_Diode:
            DMM_Switch_Mode(Dmm_Mode_Diode);
            DMM_Res_Range_Ctrl(DMM_RES_RANGE_30K);   // 二极管/通断用 2KΩ 基准测微小电阻
            break;
        default:
            break;
    }

    // 换页后第一个样本直接作为 IIR 初值，读数不会从上个页面的值爬过来
    dmm_iir_prime = true;

    DMM_Draw_Mode_Boxes((uint8_t)page, (uint8_t)old_page);
    dmm_draw_range_label();
    dmm_draw_rel_indicator();
    dmm_clear_data_area();
    dmm_redraw_units();

    StartBeezer(0);
}

/**
 * @brief  通用页面按键处理
 * @note   1. 编码器右转：下一页；左转：上一页（首尾循环）
 *         2. SET 单击：HOLD 开关
 *         3. MODE 单击：相对值（REL）开关
 *         4. 编码器长按：退出到 LVGL
 *         原来「上/下键手动切电阻档」的分支删掉了——电阻档现在由自动量程决定
 */
static void dmm_common_page_handler(KeyEventMsg_t msg) {
    if (msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        dmm_goto_page((DMM_AppPage_t)((dmm_current_page + 1) % DMM_PAGE_NUM));
        return;
    }

    if (msg.event == ENCODER_EVENT_PRESS_LEFT) {
        dmm_goto_page((DMM_AppPage_t)((dmm_current_page + DMM_PAGE_NUM - 1) % DMM_PAGE_NUM));
        return;
    }

    if (msg.key == KEY_SET && msg.event == KEY_EVENT_CLICK) {
        Hold_ctrl(!Is_Refresh_On);
        StartBeezer(0);
        return;
    }

    if (msg.key == KEY_MODE && msg.event == KEY_EVENT_CLICK) {
        dmm_rel_toggle();
        return;
    }

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
 * @brief  带极值剔除的求和：先求总和，再减掉最小 4 个和最大 4 个
 * @param  base: 通道首地址（交错缓冲里的第一个样本）
 * @param  stride: 相邻样本的间隔（交错比 = 3）
 * @param  n: 样本个数
 * @return 中间 n-8 个样本的和
 * @note   原来三个通道各拷 200 个点到栈上再 qsort：1.2 KB 中断栈 + 约 1.3 ms 排序；
 *         这里维护 8 个极值的插入表，只有整数比较，栈占用为 0，也不长时间屏蔽中断
 */
static uint32_t trimmed_sum_stride(const volatile uint16_t *base, uint16_t stride, uint16_t n) {
    uint16_t lo[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};   // 最小的 4 个（升序）
    uint16_t hi[4] = {0, 0, 0, 0};                        // 最大的 4 个（升序）
    uint32_t sum = 0;

    for (uint16_t i = 0; i < n; i++) {
        uint16_t v = base[i * stride];
        sum += v;

        if (v < lo[3]) {
            if (v < lo[0]) {
                lo[3] = lo[2]; lo[2] = lo[1]; lo[1] = lo[0]; lo[0] = v;
            } else if (v < lo[1]) {
                lo[3] = lo[2]; lo[2] = lo[1]; lo[1] = v;
            } else if (v < lo[2]) {
                lo[3] = lo[2]; lo[2] = v;
            } else {
                lo[3] = v;
            }
        }

        if (v > hi[0]) {
            if (v > hi[3]) {
                hi[0] = hi[1]; hi[1] = hi[2]; hi[2] = hi[3]; hi[3] = v;
            } else if (v > hi[2]) {
                hi[0] = hi[1]; hi[1] = hi[2]; hi[2] = v;
            } else if (v > hi[1]) {
                hi[0] = hi[1]; hi[1] = v;
            } else {
                hi[0] = v;
            }
        }
    }

    for (uint8_t i = 0; i < 4; i++) {
        sum -= lo[i];
        sum -= hi[i];
    }
    return sum;
}

/**
 * @brief  ADC转换完成回调函数
 * @param  hadc: ADC句柄
 * @note   1. 仅处理ADC1的转换完成事件
 *         2. 600 个交错样本按步长 3 分离三通道，各 200 点
 *         3. 去毛刺（去掉 4 小 4 大）+ 过采样到 14bit + 校准 + IIR
 *         4. 换档/换页后的第一个样本直接作为 IIR 初值
 */
void CB_DMM_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        AdcTim_OFF(); // 关闭ADC触发定时器

        // 三通道在交错缓冲里分别隔 0/1/2 个半字
        uint32_t sum1 = trimmed_sum_stride(&dmm_adc_raw_buf[0], 3, 200);   // 电压
        uint32_t sum2 = trimmed_sum_stride(&dmm_adc_raw_buf[1], 3, 200);   // 电流
        uint32_t sum3 = trimmed_sum_stride(&dmm_adc_raw_buf[2], 3, 200);   // 电阻

        // 过采样到 14bit + 均值（192 点求和后 /12，中点 8192）
        uint16_t ch1_14bit = (uint16_t)((sum1 >> 2) / 12U);
        uint16_t ch2_14bit = (uint16_t)((sum2 >> 2) / 12U);
        uint16_t ch3_14bit = (uint16_t)((sum3 >> 2) / 12U);

        // 超量程：贴到量程两端说明前端已经饱和，读数没有意义
        DMM_Voltage_OL = (ch1_14bit >= 16300U) || (ch1_14bit <= 80U);
        DMM_Current_OL = (ch2_14bit >= 16300U) || (ch2_14bit <= 80U);

        // 电压校准（正负分别用 R/B 系数）
        int32_t temp1 = (int32_t)ch1_14bit - (8192 - UserParam.DMM_Voltage_Original);
        float raw_voltage = (float)temp1 * (3.3f / 16383.0f) *
                            (temp1 >= 0 ? UserParam.DMM_Voltage_Factor_R : UserParam.DMM_Voltage_Factor_B);
        // 电流校准
        int32_t temp2 = (int32_t)ch2_14bit - (8192 + UserParam.DMM_Current_Original);
        float raw_current = (float)temp2 * (3.3f / 16383.0f) * UserParam.DMM_Current_Factor;
        // 电阻校准（零点按当前档位取）
        // 二极管/通断档固定用 2KΩ 基准，零点要跟着基准电阻走
        bool use_2k_ref = (dmm_current_page == DMM_PAGE_Diode) || (dmm_res_mode == DMM_RES_RANGE_30K);
        int32_t res_original = use_2k_ref ? (int32_t)UserParam.DMM_Res_R2K_Original
                                          : (int32_t)UserParam.DMM_Res_R200_Original;
        float raw_resistance_voltage = (float)((int32_t)ch3_14bit + res_original) * (3.3f / 16383.0f);

        // IIR：换档/换页后第一个样本直接当初值，避免读数从旧值爬到新值
        if (dmm_iir_prime) {
            iir_prev_voltage = raw_voltage;
            iir_prev_current = raw_current;
            iir_prev_resistance = raw_resistance_voltage;
            dmm_iir_prime = false;
        }

        DMM_Voltage = iir_filter(raw_voltage, iir_prev_voltage, IIR_ALPHA_VOLTAGE);
        DMM_Current = iir_filter(raw_current, iir_prev_current, IIR_ALPHA_CURRENT);
        DMM_Resistance_Voltage = iir_filter(raw_resistance_voltage, iir_prev_resistance, IIR_ALPHA_RESISTANCE);

        iir_prev_voltage = DMM_Voltage;
        iir_prev_current = DMM_Current;
        iir_prev_resistance = DMM_Resistance_Voltage;

        dmm_block_count++;
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
    // 电阻档位复位（2KΩ 满量程档 = 200Ω 基准）
    dmm_res_mode = DMM_RES_RANGE_2K;
    // OL / 单位 / 通断状态复位
    vol_ol_shown = false;
    cur_ol_shown = false;
    res_ol_shown = false;
    dio_ol_shown = false;
    dio_unit_shown = 0;
    dio_short = false;
    dmm_rel_on = false;
    // IIR 预热：第一个样本直接当初值
    dmm_iir_prime = true;
    dmm_block_count = 0;
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

    // 配置定时器8：20Khz触发（预分频0，自动重装7199）
    __HAL_TIM_SET_PRESCALER(&htim8, 0);
    __HAL_TIM_SET_AUTORELOAD(&htim8, 7200 - 1);

    // 启动ADC DMA采集（600 个半字 = 200 点 × 3 通道）；失败就交给看门狗重试
    if (dmm_start_capture()) {
        AdcTim_ON();                         // 开启ADC触发
    }

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
    StartBeezer(0);                                        // 停掉二极管档可能还在响的蜂鸣
    DMM_Switch_Mode(Dmm_Mode_Voltage);                   // 切换到电压模式
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET); // 关闭电压档硬件

    DMM_ADC_DeInit(); // 反初始化ADC

    osThreadSuspend(DmmCoreTaskHandle); // 挂起DMM核心任务
}
