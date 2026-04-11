/**
******************************************************************************
  * @file           : AwgTask.c
  * @brief          : 任意波形发生器（AWG）系统核心任务，包括波形生成控制、UI显示管理、按键事件处理，
  *                   以及波形参数（频率/占空比/幅值/偏置）的编辑。
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
#include "ST7789.h"
#include "wave_generator.h"

/************************ 全局宏定义 ************************/
// 波形总数、左侧栏每页显示数
#define WAVE_TOTAL_COUNT        14  // 总波形数
#define WAVE_COLUMN_PAGE_COUNT  6   // 左侧栏每页显示波形数
#define WAVE_COLUMN_MAX_PAGE    (WAVE_TOTAL_COUNT - WAVE_COLUMN_PAGE_COUNT) // 左侧栏最大起始索引（14 - 6 = 8）

// 屏幕尺寸与区域坐标定义
#define SCREEN_WIDTH            320 // 屏幕宽度
#define TOP_BAR_HEIGHT          31  // 顶部状态栏高度
// 左侧波形选择栏坐标
#define LEFT_COLUMN_X_START     6
#define LEFT_COLUMN_Y_START     38
#define LEFT_COLUMN_X_END       83
#define LEFT_COLUMN_Y_END       233
// 波形预览区坐标
#define WAVE_PREVIEW_X_START    90
#define WAVE_PREVIEW_Y_START    38
#define WAVE_PREVIEW_X_END      313
#define WAVE_PREVIEW_Y_END      107
// 参数编辑区坐标
#define PARAM_AREA_X_START      90
#define PARAM_AREA_Y_START      112
#define PARAM_AREA_X_END        313
#define PARAM_AREA_Y_END        232

// 颜色定义（16位RGB565格式）
#define COLOR_TOP_BAR_BG        0x1908 // 顶部栏背景色
#define COLOR_TOP_BAR_LINE      0x11ac // 顶部栏分割线颜色
#define COLOR_MAIN_BG           0x18c6 // 主界面背景色
#define COLOR_WAVE_PREVIEW_BG   0x1908 // 波形预览区背景色
#define COLOR_WAVE_BORDER_OFF   0x528a // 输出关闭时边框色
#define COLOR_WAVE_BORDER_ON    0x9fec // 输出开启时边框色
#define COLOR_PARAM_BG          0x18c6 // 参数项背景色
#define COLOR_TEXT_NORMAL       0x632c // 普通文本颜色
#define COLOR_TEXT_HIGHLIGHT    0xef7d // 高亮文本颜色
#define COLOR_COLUMN_BG         0x1908 // 左侧栏背景色
#define COLOR_COLUMN_SELECT_BG  0x11ac // 左侧栏选中项背景色
#define COLOR_COLUMN_FONT       0xef7d // 左侧栏文本颜色
#define COLOR_TITLE_TEXT        0x24be // 标题文本颜色

// 左侧栏元素尺寸
#define COLUMN_ITEM_HEIGHT      32     // 左侧栏每个波形项高度
#define COLUMN_ITEM_SELECT_H    28     // 左侧栏选中项高亮高度
#define COLUMN_ITEM_Y_OFFSET    42     // 左侧栏第一项Y轴偏移
#define COLUMN_ITEM_TEXT_Y      48     // 左侧栏文本Y轴起始位置
// 参数区元素尺寸
#define PARAM_TEXT_X_OFFSET     105    // 参数名称X轴偏移
#define PARAM_UNIT_X_OFFSET     270    // 参数单位X轴偏移
#define PARAM_VALUE_X_OFFSET    177    // 参数值X轴偏移
#define PARAM_ROW_HEIGHT        29     // 参数项行高
#define PARAM_ROW_START_Y       120    // 参数项起始Y轴

// 参数编辑相关宏
#define TOTAL_PARAMS    14      // 总参数数量
#define SHOW_SIZE       6       // 参数列表单页显示数量
#define MAX_TOP         (TOTAL_PARAMS - SHOW_SIZE)  // 参数列表top最大合法值（14 - 6 = 8）

/************************ 枚举定义 ************************/
/**
 * @brief 波形类型枚举
 * @note  每个枚举值对应一种波形，与WaveAttrTable表项一一对应
 */
typedef enum {
    Wave_Sine = 0,            // 正弦波形
    Wave_Rect,                // 矩形波形
    Wave_Triangle,            // 三角波形
    Wave_DC,                  // 直流信号
    Wave_Half,                // 半波信号
    Wave_Full,                // 全波信号
    Wave_UpStair,             // 正阶梯波
    Wave_DownStair,           // 反阶梯波
    Wave_Exponential,         // 指数上升
    Wave_ExponentialD,        // 指数下降
    Wave_MultiTone,           // 多音信号
    Wave_Sinc,                // 辛克脉冲
    Wave_Lorenz,              // 洛伦兹波
    Wave_Noise,               // 随机噪声
    Wave_Max_Count            // 标记枚举总数
} WaveCursor_e;

/**
 * @brief 参数类型枚举
 * @note  对应频率、占空比、振幅、偏置四个可编辑参数
 */
typedef enum {
    Param_Freq = 0,           // 频率参数
    Param_DutyCycle,          // 占空比参数
    Param_Amplitude,          // 振幅参数
    Param_Bias,               // 偏置参数
    Param_Max_Count           // 枚举总数
} ParamCursor_e;

/**
 * @brief 频率参数编辑位数枚举
 * @note  对应频率值的数位：个位、十位、百位、千位、万位
 */
typedef enum {
    freq_unit = 0,            // 个位（10^0）
    freq_decade,              // 十位（10^1）
    freq_hundredth,           // 百位（10^2）
    freq_thousand,            // 千位（10^3）
    freq_ten_thousand         // 万位（10^4）
} Freq_Digit_e;

/**
 * @brief 占空比参数编辑位数枚举
 * @note  对应占空比值的数位：个位、十位、百位
 */
typedef enum {
    duty_unit = 0,            // 个位（10^0）
    duty_decade,              // 十位（10^1）
    duty_hundredth            // 百位（10^2）
} Duty_Digit_e;

/**
 * @brief 振幅参数编辑位数枚举
 * @note  对应振幅值的数位：百分位、十分位、个位
 */
typedef enum {
    amp_hundredth = 0,        // 百分位（0.01）
    amp_tenth,                // 十分位（0.1）
    amp_unit,                 // 个位（1）
} Amp_digit_e;

/**
 * @brief 偏置参数编辑位数枚举
 * @note  对应偏置值的数位：百分位、十分位、个位
 */
typedef enum {
    bias_hundredth = 0,       // 百分位（0.01）
    bias_tenth,               // 十分位（0.1）
    bias_unit,                // 个位（1）
} Bias_digit_e;

/**
 * @brief 页面状态枚举
 * @note  对应不同的操作页面，用于分发按键事件
 */
typedef enum {
    PAGE_MAIN = 0,            // 主界面_波形选择
    PAGE_PARAM_SELECT,        // 次界面_参数选择
    PAGE_FREQ_DIGIT,          // 频率参数编辑页
    PAGE_DUTY_DIGIT,          // 占空比参数编辑页
    PAGE_AMP_DIGIT,           // 振幅参数编辑页
    PAGE_BIAS_DIGIT,          // 偏置参数编辑页
    PAGE_NUM                  // 页面总数
} AWG_AppPage_t;

/************************ 类型定义 ************************/
/**
 * @brief 波形生成函数指针类型
 * @param freq: 频率值
 * @param amplitude: 振幅值
 * @param bias: 偏置值
 * @param duty_cycle: 占空比值
 * @retval HAL_StatusTypeDef: HAL库状态（成功 /失败）
 * @note  要用本方案，所有的波形发生函数都必须跟下面的定义相同（入参、返回值类型都得一样）
 */
typedef HAL_StatusTypeDef (*WaveGenFunc_t)(float freq, float amplitude, float bias, float duty_cycle);

/**
 * @brief 波形属性结构体
 * @note  聚合波形的枚举索引、名称、预览图、生成函数，统一管理波形属性
 */
typedef struct {
    WaveCursor_e idx;         // 波形枚举索引
    const char *name;         // 波形显示名称
    const void *preview_img;  // 波形预览图片资源指针
    WaveGenFunc_t gen_func;   // 波形生成函数指针
} WaveAttribute_t;

/************************ 全局变量定义 ************************/
// 波形选择相关
WaveCursor_e current_wave = Wave_Sine;  // 当前选中的波形（初始为正弦波）
WaveCursor_e previous_wave = Wave_Sine; // 上一个选中的波形（用于对比更新）
uint8_t ColumnPageIndex = 0;            // 左侧栏当前页起始索引（0 ~ 8）
uint8_t ColumnPageSelectIndex = 0;      // 左侧栏当前页选中项索引（0 ~ 5）
uint8_t last_idx = 0;                   // 左侧栏上一次选中项索引（用于擦除旧高亮）

// 参数选择相关
ParamCursor_e current_param = Param_Freq;  // 当前选中的参数（初始为频率）
ParamCursor_e previous_param = Param_Freq; // 上一个选中的参数（用于对比更新）

// 参数编辑相关
Freq_Digit_e freq_digit_current = freq_unit;        // 频率当前编辑数位   （初始为个位）
char freq_str[6] = "01000";                         // 频率显示字符串     （初始值1000Hz）
Duty_Digit_e duty_digit_current = duty_unit;        // 占空比当前编辑数位 （初始为个位）
char duty_str[4] = "050";                           // 占空比显示字符串   （初始值50%）
Amp_digit_e amp_digit_current = amp_hundredth;      // 振幅当前编辑数位   （初始为百分位）
char amp_str[5] = "1.00";                           // 振幅显示字符串     （初始值1.00V）
Bias_digit_e bias_digit_current = bias_hundredth;   // 偏置当前编辑数位   （初始为百分位）
char bias_str[6] = "+0.00";                         // 偏置显示字符串     （初始值+0.00V）

// 参数数值（实际运算用）
static float FreqValue = 1000.0f;  // 频率值（Hz）
static float DutyValue = 50.0f;    // 占空比值（%）
static float AmpValue = 1.0f;      // 振幅值（V）
static float BiasValue = 0.0f;     // 偏置值（V）

// 数位权重表（用于参数编辑时的数值增减）
const float awg_pow10_table[] = {1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f};

// 页面状态相关
AWG_AppPage_t awg_current_page = PAGE_MAIN;     // 当前激活页面  （初始为主界面）
AWG_AppPage_t awg_previous_page = PAGE_MAIN;    // 上一个页面    （用于返回）
bool IsOutputOn = false;                        // 波形输出使能  （默认关闭）
static int last_top = 0;                        // 参数列表上一次的top值（用于懒滚动）

/************************ 波形属性表 ************************/
/**
 * @brief 全局波形属性表
 * @note  枚举、名称、预览图、生成函数一一绑定，唯一数据源
 */
static const WaveAttribute_t WaveAttrTable[WAVE_TOTAL_COUNT] = {
    {Wave_Sine,            "正弦波形",   &img_wave1_212x57, WaveGen_Sin},
    {Wave_Rect,            "矩形波形",   &img_wave2_212x57, WaveGen_Square},
    {Wave_Triangle,        "三角波形",   &img_wave3_212x57, WaveGen_Sawtooth},
    {Wave_DC,              "直流信号",   &img_wave4_212x57, WaveGen_DC},
    {Wave_Half,            "半波信号",   &img_wave5_212x57, WaveGen_HalfWaveRect},
    {Wave_Full,            "全波信号",   &img_wave6_212x57, WaveGen_FullWaveRect},
    {Wave_UpStair,         "正阶梯波",   &img_wave7_212x57, WaveGen_UpStep},
    {Wave_DownStair,       "反阶梯波",   &img_wave8_212x57, WaveGen_DownStep},
    {Wave_Exponential,     "指数上升",   &img_wave9_212x57, WaveGen_ExpRise},
    {Wave_ExponentialD,    "指数下降",   &img_wave10_212x57, WaveGen_ExpDecay},
    {Wave_MultiTone,       "多音信号",   &img_wave11_212x57, WaveGen_MultiTone},
    {Wave_Sinc,            "辛克脉冲",   &img_wave12_212x57, WaveGen_Sinc},
    {Wave_Lorenz,          "洛伦兹波",   &img_wave13_212x57, WaveGen_Lorenz},
    {Wave_Noise,           "随机噪声",   &img_wave14_212x57, WaveGen_Noise}
};

/************************ 静态函数声明 ************************/
static void DrawAwgMainBasicElement(void);                                                 // 绘制基础界面元素
static void DrawLeftColumn(uint8_t page_start_idx, uint8_t select_idx);             // 绘制左侧波形选择栏
static void DrawWavePreview(WaveCursor_e wave_idx);                                 // 绘制波形预览图
static void DrawIndiccateBox(bool is_output);                                       // 绘制输出状态提示框
static void GenerateWave(WaveCursor_e wave_idx);                                    // 生成指定波形
static void calc_render_params(WaveCursor_e p, uint8_t *out_top, uint8_t *out_rel); // 计算列表渲染参数

/************************ 页面处理函数声明 ************************/
void awg_main_page_handler(KeyEventMsg_t msg);      // 主界面按键处理
void param_select_handler(KeyEventMsg_t msg);       // 参数选择页按键处理
void freq_digit_handler(KeyEventMsg_t msg);         // 频率编辑页按键处理
void duty_digit_handler(KeyEventMsg_t msg);         // 占空比编辑页按键处理
void amp_digit_handler(KeyEventMsg_t msg);          // 振幅编辑页按键处理
void bias_digit_handler(KeyEventMsg_t msg);         // 偏置编辑页按键处理

/************************ 核心绘制函数 ************************/
/**
 * @brief 绘制基础界面元素
 * @note  包括顶栏、背景、左侧栏、预览区、参数区等基础布局
 */
static void DrawAwgMainBasicElement(void) {
    //绘制顶部状态栏
    lcd_draw_rect(0, 0, SCREEN_WIDTH - 1, TOP_BAR_HEIGHT, COLOR_TOP_BAR_BG, 1);
    lcd_draw_line(0, TOP_BAR_HEIGHT, SCREEN_WIDTH - 1, TOP_BAR_HEIGHT, COLOR_TOP_BAR_LINE);
    lcd_draw_string(106, 6, "波形发生器", &yahei20x20, COLOR_TITLE_TEXT, COLOR_TOP_BAR_BG, 3);

    // 绘制主背景
    lcd_draw_rect(0, TOP_BAR_HEIGHT + 1, SCREEN_WIDTH - 1, 239, COLOR_MAIN_BG, 1);
    // 绘制左侧栏背景
    lcd_draw_round_rect(LEFT_COLUMN_X_START, LEFT_COLUMN_Y_START,
                        LEFT_COLUMN_X_END, LEFT_COLUMN_Y_END, 8,
                        COLOR_COLUMN_BG, 1);
    // 绘制左侧波形选择栏
    DrawLeftColumn(ColumnPageIndex, ColumnPageSelectIndex);

    // 3. 绘制波形预览区背景
    lcd_draw_round_rect(WAVE_PREVIEW_X_START, WAVE_PREVIEW_Y_START,
                        WAVE_PREVIEW_X_END, WAVE_PREVIEW_Y_END, 8,
                        COLOR_WAVE_PREVIEW_BG, 1);
    // 绘制初始波形预览图（正弦波）
    DrawWavePreview(Wave_Sine);

    // 绘制输出状态提示框
    DrawIndiccateBox(false);

    // 绘制参数区背景
    lcd_draw_round_rect(PARAM_AREA_X_START, PARAM_AREA_Y_START,
                        PARAM_AREA_X_END, PARAM_AREA_Y_END, 8,
                        COLOR_WAVE_PREVIEW_BG, 1);

    // 绘制参数项基础布局
    typedef struct {
        const char *name;    // 参数名称
        const char *unit;    // 参数单位
        const char *value;   // 参数初始值
        uint8_t x_val;       // 参数值X轴偏移
    } ParamItem_t;
    const ParamItem_t param_items[] = {
        {"频率", "Hz", "01000", 177},  // 频率参数
        {"占空", "%", "050", 190},     // 占空比参数
        {"幅值", "V", "1.00", 188},    // 振幅参数
        {"偏置", "V", "+0.00", 175}    // 偏置参数
    };

    // 遍历绘制每个参数项
    for (uint8_t i = 0; i < sizeof(param_items) / sizeof(ParamItem_t); i++) {
        uint16_t y_start = PARAM_ROW_START_Y + i * PARAM_ROW_HEIGHT - 3;
        uint16_t y_end = y_start + 23;
        // 绘制参数项背景
        lcd_draw_round_rect(100, y_start, 303, y_end, 12, COLOR_PARAM_BG, 1);
        // 绘制参数名称
        lcd_draw_string(PARAM_TEXT_X_OFFSET, PARAM_ROW_START_Y + i * PARAM_ROW_HEIGHT,
                        param_items[i].name, &yahei16x16,
                        COLOR_TEXT_NORMAL, COLOR_PARAM_BG, 2);
        // 绘制参数单位
        lcd_draw_string(PARAM_UNIT_X_OFFSET + (i == 1 ? 6 : (i >= 2 ? 8 : 0)),
                        PARAM_ROW_START_Y + i * PARAM_ROW_HEIGHT,
                        param_items[i].unit, &yahei16x16,
                        COLOR_TEXT_NORMAL, COLOR_PARAM_BG, (int8_t)(i == 0 ? -2 : 0));
        // 绘制参数值
        lcd_draw_string(param_items[i].x_val, PARAM_ROW_START_Y + i * PARAM_ROW_HEIGHT,
                        param_items[i].value, &yahei16x16,
                        COLOR_TEXT_HIGHLIGHT, COLOR_PARAM_BG, -3);
    }
}

/**
 * @brief 绘制左侧波形选择栏
 * @param page_start_idx: 左侧栏当前页起始波形索引（0 ~ 8）
 * @param select_idx: 当前页选中的波形项索引（0 ~ 5）
 * @note  会先擦除上一次选中项的高亮，再绘制新选中项和当前页波形列表
 */
static void DrawLeftColumn(uint8_t page_start_idx, uint8_t select_idx) {
    // 参数合法性校验
    if (page_start_idx > WAVE_COLUMN_MAX_PAGE || select_idx >= WAVE_COLUMN_PAGE_COUNT) {
        return;
    }

    // 擦除上一个选中项的高亮背景
    lcd_draw_round_rect(LEFT_COLUMN_X_START + 4, COLUMN_ITEM_Y_OFFSET + last_idx * COLUMN_ITEM_HEIGHT,
                        LEFT_COLUMN_X_END - 4,
                        COLUMN_ITEM_Y_OFFSET + last_idx * COLUMN_ITEM_HEIGHT + COLUMN_ITEM_SELECT_H, 8,
                        COLOR_COLUMN_BG, 1);
    last_idx = select_idx; // 更新上一次选中索引

    // 绘制当前选中项的高亮背景
    lcd_draw_round_rect(LEFT_COLUMN_X_START + 4, COLUMN_ITEM_Y_OFFSET + select_idx * COLUMN_ITEM_HEIGHT,
                        LEFT_COLUMN_X_END - 4,
                        COLUMN_ITEM_Y_OFFSET + select_idx * COLUMN_ITEM_HEIGHT + COLUMN_ITEM_SELECT_H, 8,
                        COLOR_COLUMN_SELECT_BG, 1);

    // 绘制当前页的波形名称列表
    for (uint8_t i = 0; i < WAVE_COLUMN_PAGE_COUNT; i++) {
        uint8_t wave_table_idx = page_start_idx + i;
        if (wave_table_idx >= WAVE_TOTAL_COUNT) {break;}    // 超出总波形数则停止
        // 从属性表获取波形名称
        const char *wave_name = WaveAttrTable[wave_table_idx].name;
        // 选中项用高亮背景，其他用默认背景
        uint16_t text_bg = (i == select_idx) ? COLOR_COLUMN_SELECT_BG : COLOR_COLUMN_BG;

        // 绘制波形名称
        lcd_draw_string(LEFT_COLUMN_X_START + 7, COLUMN_ITEM_TEXT_Y + i * COLUMN_ITEM_HEIGHT,
                        wave_name, &yahei16x16,
                        COLOR_COLUMN_FONT, text_bg, 0);
    }
}

/**
 * @brief 绘制波形预览图
 * @param wave_idx: 波形枚举索引（0 ~ 13）
 * @note  从WaveAttrTable获取对应预览图资源并绘制
 */
static void DrawWavePreview(WaveCursor_e wave_idx) {
    // 参数合法性校验
    if (wave_idx >= Wave_Max_Count) {
        return;
    }

    // 从属性表获取预览图资源并绘制
    const void *preview_img = WaveAttrTable[wave_idx].preview_img;
    lcd_draw_image(WAVE_PREVIEW_X_START + 6, WAVE_PREVIEW_Y_START + 6, preview_img);
}

/**
 * @brief 绘制输出状态提示框
 * @param is_output: 是否开启输出（true-开启，false-关闭）
 * @note  通过边框颜色区分输出状态，双层边框增强视觉效果
 */
static void DrawIndiccateBox(bool is_output) {
    uint16_t box_color = is_output ? COLOR_WAVE_BORDER_ON : COLOR_WAVE_BORDER_OFF;
    // 绘制外层边框
    lcd_draw_rect(WAVE_PREVIEW_X_START + 5, WAVE_PREVIEW_Y_START + 5,
                  WAVE_PREVIEW_X_END - 5, WAVE_PREVIEW_Y_END - 5, box_color, 0);
    // 绘制内层边框
    lcd_draw_rect(WAVE_PREVIEW_X_START + 4, WAVE_PREVIEW_Y_START + 4,
                  WAVE_PREVIEW_X_END - 4, WAVE_PREVIEW_Y_END - 4, box_color, 0);
}

/**
 * @brief 生成指定波形
 * @param wave_idx: 波形枚举索引（0 ~ 13）
 * @note  从 WaveAttrTable获取对应生成函数，传入参数并执行
 */
static void GenerateWave(WaveCursor_e wave_idx) {
    // 参数合法性校验
    if (wave_idx >= Wave_Max_Count) {return;}

    // 从属性表获取生成函数并执行
    WaveAttrTable[wave_idx].gen_func(FreqValue, AmpValue, BiasValue, DutyValue / 100);
}

/**
 * @brief 计算列表渲染参数（懒滚动逻辑）
 * @param p: 当前选中的波形索引
 * @param out_top: 输出当前页起始索引
 * @param out_rel: 输出当前选中项在页面内的相对索引
 * @note  仅当last_top超出合法区间时调整，减少不必要的刷新
 */
static void calc_render_params(WaveCursor_e p, uint8_t *out_top, uint8_t *out_rel) {
    // 计算top的合法区间（保证选中项在显示区内）
    int low = p - (SHOW_SIZE - 1);      // 下界：选中项为最后一个
    int high = p;                       // 上界：选中项为第一个

    // 边界钳位：low不能小于0，high不能大于MAX_TOP
    low = (low < 0) ? 0 : low;
    high = (high > MAX_TOP) ? MAX_TOP : high;

    // 懒滚动：仅当last_top超出合法区间时调整
    if (last_top < low) {
        last_top = low;
    } else if (last_top > high) {
        last_top = high;
    }

    // 输出计算结果
    *out_top = last_top;
    *out_rel = p - last_top;
}

/************************ 页面处理函数 ************************/
/**
 * @brief 主界面按键处理函数
 * @param msg: 按键事件消息
 * @note  处理波形选择、输出开关、页面切换等操作
 */
void awg_main_page_handler(KeyEventMsg_t msg) {
    // 编码器右转/下键：选中波形向下切换
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT || msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        previous_wave = current_wave;
        // 边界处理：不超过最大波形索引
        current_wave = (current_wave < Wave_Max_Count - 1) ? (current_wave + 1) : Wave_Max_Count - 1;

        // 输出开启时，实时更新波形
        if (IsOutputOn == true) {GenerateWave(current_wave);}
    }
    // 编码器左转/上键：选中波形向上切换
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT || msg.event == ENCODER_EVENT_PRESS_LEFT) {
        previous_wave = current_wave;
        // 边界处理：不小于最小波形索引
        current_wave = (current_wave > Wave_Sine) ? (current_wave - 1) : Wave_Sine;

        // 输出开启时，实时更新波形
        if (IsOutputOn == true) {GenerateWave(current_wave);}
    }
    // 确认键（短按）：进入参数选择页面
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        awg_current_page = PAGE_PARAM_SELECT;
        awg_previous_page = PAGE_MAIN;

        // 绘制参数选择页默认焦点框
        lcd_draw_round_rect(100, 117, 303, 140, 12, COLOR_TITLE_TEXT, 0);
        // 重置参数选中状态
        current_param = Param_Freq;
        previous_param = Param_Freq;
        StartBeezer(0); // 蜂鸣器反馈
    }
    // SET键（长按）：翻转输出状态
    else if (msg.key == KEY_SET && msg.event == KEY_EVENT_LONG_PRESS) {
        IsOutputOn = !IsOutputOn;       // 翻转输出状态
        DrawIndiccateBox(IsOutputOn);   // 更新输出提示框

        // 输出开启则生成波形，关闭则停止
        if (IsOutputOn) {GenerateWave(current_wave);}
        else {WaveGen_Stop();}

        StartBeezer(0); // 蜂鸣器反馈
        return;
    }
    // 编码器按键（长按）：返回LVGL主页面
    else if (msg.key == KEY_ENCODER && msg.event == KEY_EVENT_LONG_PRESS) {
        AppListType APP = APP_LVGL;
        StartBeezer(0); // 蜂鸣器反馈
        osMessageQueuePut(AppSwitchQueueHandle, &APP, 0, 10);
        return;
    } else {
        return; // 其他按键不处理
    }

    // 蜂鸣器反馈
    StartBeezer(0);

    // 波形切换后，更新左侧栏和预览图
    if (current_wave != previous_wave) {
        uint8_t wave_idx, select_idx;
        // 计算列表渲染参数
        calc_render_params(current_wave, &wave_idx, &select_idx);
        // 更新左侧栏显示
        DrawLeftColumn(wave_idx, select_idx);
        // 更新波形预览图
        DrawWavePreview(current_wave);
    }
}

/**
 * @brief 参数选择页按键处理函数
 * @param msg: 按键事件消息
 * @note  处理参数选择、页面返回、进入参数编辑页等操作
 */
void param_select_handler(KeyEventMsg_t msg) {
    // 编码器右转/下键：选中参数向下切换
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT || msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        previous_param = current_param;
        // 循环切换：到最后一个参数后回到第一个
        current_param = (current_param < Param_Bias) ? (current_param + 1) : Param_Freq;
    }
    // 编码器左转/上键：选中参数向上切换
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT || msg.event == ENCODER_EVENT_PRESS_LEFT) {
        previous_param = current_param;
        // 循环切换：到第一个参数后回到最后一个
        current_param = (current_param > Param_Freq) ? (current_param - 1) : Param_Bias;
    }
    // 确认键（长按）：返回主界面
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_LONG_PRESS) {
        // 擦除当前参数的焦点框
        lcd_draw_round_rect(100, PARAM_ROW_START_Y + current_param * PARAM_ROW_HEIGHT - 3, 303,
                            PARAM_ROW_START_Y + current_param * PARAM_ROW_HEIGHT + 20, 12, COLOR_PARAM_BG, 0);
        StartBeezer(0); // 蜂鸣器反馈

        // 切换回主界面
        awg_current_page = PAGE_MAIN;
        awg_previous_page = PAGE_PARAM_SELECT;
        return;
    }
    // 确认键（短按）：进入对应参数编辑页
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        // 切换到对应参数编辑页
        awg_current_page = current_param + PAGE_FREQ_DIGIT;
        awg_previous_page = PAGE_PARAM_SELECT;

        StartBeezer(0); // 蜂鸣器反馈

        // 绘制参数编辑页初始焦点条
        switch (awg_current_page) {
            case PAGE_FREQ_DIGIT: // 频率编辑页
                freq_digit_current = freq_unit;
                lcd_draw_line(177 + 13 * 4, 137, 177 + 13 * 4 + 10, 137, 0x87d3);
                break;
            case PAGE_DUTY_DIGIT: // 占空比编辑页
                duty_digit_current = duty_unit;
                lcd_draw_line(177 + 13 * 3, 166, 177 + 13 * 3 + 10, 166, 0x87d3);
                break;
            case PAGE_AMP_DIGIT: // 振幅编辑页
                amp_digit_current = amp_hundredth;
                lcd_draw_line(182 + 13 * 3, 195, 182 + 13 * 3 + 10, 195, 0x87d3);
                break;
            case PAGE_BIAS_DIGIT: // 偏置编辑页
                bias_digit_current = bias_hundredth;
                lcd_draw_line(182 + 13 * 3, 224, 182 + 13 * 3 + 10, 224, 0x87d3);
                break;
            default:
                break;
        }
        return;
    } else {
        return; // 其他按键不处理
    }

    // 蜂鸣器反馈
    StartBeezer(0);

    // 擦除上一个参数的焦点框
    lcd_draw_round_rect(100, PARAM_ROW_START_Y + previous_param * PARAM_ROW_HEIGHT - 3, 303,
                        PARAM_ROW_START_Y + previous_param * PARAM_ROW_HEIGHT + 20, 12, COLOR_PARAM_BG, 0);
    // 绘制当前参数的焦点框
    lcd_draw_round_rect(100, PARAM_ROW_START_Y + current_param * PARAM_ROW_HEIGHT - 3, 303,
                        PARAM_ROW_START_Y + current_param * PARAM_ROW_HEIGHT + 20, 12, COLOR_TITLE_TEXT, 0);
}

/**
 * @brief 频率参数编辑页按键处理函数
 * @param msg: 按键事件消息
 * @note  处理频率数位选择、数值增减、返回参数选择页等操作
 */
void freq_digit_handler(KeyEventMsg_t msg) {
    // 编码器左转（按下）：切换到更高数位
    if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_PRESS_LEFT) {
        freq_digit_current = (freq_digit_current < freq_ten_thousand) ? (freq_digit_current + 1) : freq_unit;
    }
    // 编码器右转（按下）：切换到更低数位
    else if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        freq_digit_current = (freq_digit_current > freq_unit) ? (freq_digit_current - 1) : freq_ten_thousand;
    }
    // 编码器左转：减少对应数位的数值
    else if (msg.event == ENCODER_EVENT_LEFT) {
        // 边界处理：不小于10Hz
        FreqValue = (FreqValue - awg_pow10_table[freq_digit_current]) < 10
                        ? 10
                        : (FreqValue - awg_pow10_table[freq_digit_current]);

        // 更新频率显示字符串
        snprintf(freq_str, 6, "%05" PRIu32, (uint32_t)FreqValue);
        lcd_draw_string(177, PARAM_ROW_START_Y, freq_str, &yahei16x16,
                        COLOR_TEXT_HIGHLIGHT, COLOR_PARAM_BG, -3);

        StartBeezer(0); // 蜂鸣器反馈
        // 输出开启时，实时更新波形
        if (IsOutputOn == true) {
            GenerateWave(current_wave);
        }
        return;
    }
    // 编码器右转：增加对应数位的数值
    else if (msg.event == ENCODER_EVENT_RIGHT) {
        // 边界处理：不大于50000Hz
        FreqValue = (FreqValue + awg_pow10_table[freq_digit_current]) > 50000
                        ? 50000
                        : (FreqValue + awg_pow10_table[freq_digit_current]);

        // 更新频率显示字符串
        snprintf(freq_str, 6, "%05" PRIu32, (uint32_t)FreqValue);
        lcd_draw_string(177, PARAM_ROW_START_Y, freq_str, &yahei16x16,
                        COLOR_TEXT_HIGHLIGHT, COLOR_PARAM_BG, -3);

        StartBeezer(0); // 蜂鸣器反馈
        // 输出开启时，实时更新波形
        if (IsOutputOn == true) {
            GenerateWave(current_wave);
        }
        return;
    }
    // 确认键：返回参数选择页
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        awg_current_page = PAGE_PARAM_SELECT;
        awg_previous_page = PAGE_FREQ_DIGIT;
        // 清空数位选择示意条
        lcd_draw_line(177, 137, 257, 137, COLOR_PARAM_BG);
        StartBeezer(0);
        return;
    } else {
        return; // 其他按键不处理
    }

    // 蜂鸣器反馈
    StartBeezer(0);

    // 清空旧的数位选择示意条
    lcd_draw_line(177, 137, 257, 137, COLOR_PARAM_BG);
    // 绘制新的数位选择示意条
    lcd_draw_line(177 + 13 * (4 - freq_digit_current), 137,
                  177 + 13 * (4 - freq_digit_current) + 10, 137, 0x87d3);
}

/**
 * @brief 占空比参数编辑页按键处理函数
 * @param msg: 按键事件消息
 * @note  处理占空比数位选择、数值增减、返回参数选择页等操作
 */
void duty_digit_handler(KeyEventMsg_t msg) {
    // 编码器左转（按下）：切换到更高数位
    if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_PRESS_LEFT) {
        duty_digit_current = (duty_digit_current < duty_hundredth) ? (duty_digit_current + 1) : duty_unit;
    }
    // 编码器右转（按下）：切换到更低数位
    else if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        duty_digit_current = (duty_digit_current > duty_unit) ? (duty_digit_current - 1) : duty_hundredth;
    }
    // 编码器左转：减少对应数位的数值
    else if (msg.event == ENCODER_EVENT_LEFT) {
        // 边界处理：不小于1%
        DutyValue = (DutyValue - awg_pow10_table[duty_digit_current]) < 1
                        ? 1
                        : (DutyValue - awg_pow10_table[duty_digit_current]);

        // 更新占空比显示字符串
        snprintf(duty_str, 4, "%03" PRIu32, (uint32_t)DutyValue);
        lcd_draw_string(190, PARAM_ROW_START_Y + 1 * PARAM_ROW_HEIGHT, duty_str, &yahei16x16,
                        COLOR_TEXT_HIGHLIGHT, COLOR_PARAM_BG, -3);

        StartBeezer(0); // 蜂鸣器反馈
        // 输出开启时，实时更新波形
        if (IsOutputOn == true) {
            GenerateWave(current_wave);
        }
        return;
    }
    // 编码器右转：增加对应数位的数值
    else if (msg.event == ENCODER_EVENT_RIGHT) {
        // 边界处理：不大于100%
        DutyValue = (DutyValue + awg_pow10_table[duty_digit_current]) > 100
                        ? 100
                        : (DutyValue + awg_pow10_table[duty_digit_current]);

        // 更新占空比显示字符串
        snprintf(duty_str, 4, "%03" PRIu32, (uint32_t)DutyValue);
        lcd_draw_string(190, PARAM_ROW_START_Y + 1 * PARAM_ROW_HEIGHT, duty_str, &yahei16x16,
                        COLOR_TEXT_HIGHLIGHT, COLOR_PARAM_BG, -3);

        StartBeezer(0); // 蜂鸣器反馈
        // 输出开启时，实时更新波形
        if (IsOutputOn == true) {
            GenerateWave(current_wave);
        }
        return;
    }
    // 确认键：返回参数选择页
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        awg_current_page = PAGE_PARAM_SELECT;
        awg_previous_page = PAGE_DUTY_DIGIT;
        // 清空数位选择示意条
        lcd_draw_line(177, 166, 257, 166, COLOR_PARAM_BG);
        StartBeezer(0);
        return;
    } else {
        return; // 其他按键不处理
    }

    // 蜂鸣器反馈
    StartBeezer(0);

    // 清空旧的数位选择示意条
    lcd_draw_line(177, 166, 257, 166, COLOR_PARAM_BG);
    // 绘制新的数位选择示意条
    lcd_draw_line(177 + 13 * (3 - duty_digit_current), 166,
                  177 + 13 * (3 - duty_digit_current) + 10, 166, 0x87d3);
}

/**
 * @brief 振幅参数编辑页按键处理函数
 * @param msg: 按键事件消息
 * @note  处理振幅数位选择、数值增减、返回参数选择页等操作
 */
void amp_digit_handler(KeyEventMsg_t msg) {
    // 编码器左转（按下）：切换到更高数位
    if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_PRESS_LEFT) {
        amp_digit_current = (amp_digit_current < amp_unit) ? (amp_digit_current + 1) : amp_hundredth;
    }
    // 编码器右转（按下）：切换到更低数位
    else if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        amp_digit_current = (amp_digit_current > amp_hundredth) ? (amp_digit_current - 1) : amp_unit;
    }
    // 编码器左转：减少对应数位的数值
    else if (msg.event == ENCODER_EVENT_LEFT) {
        // 边界处理：不小于0.01V
        AmpValue = (AmpValue - 0.01f * awg_pow10_table[amp_digit_current]) < 0.01f
                       ? 0.01f
                       : (AmpValue - 0.01f * awg_pow10_table[amp_digit_current]);

        // 更新振幅显示字符串
        snprintf(amp_str, sizeof(amp_str), "%.2f", AmpValue);
        lcd_draw_string(188, PARAM_ROW_START_Y + 2 * PARAM_ROW_HEIGHT, amp_str, &yahei16x16,
                        COLOR_TEXT_HIGHLIGHT, COLOR_PARAM_BG, -3);

        StartBeezer(0); // 蜂鸣器反馈
        // 输出开启时，实时更新波形
        if (IsOutputOn == true) {
            GenerateWave(current_wave);
        }
        return;
    }
    // 编码器右转：增加对应数位的数值
    else if (msg.event == ENCODER_EVENT_RIGHT) {
        // 边界处理：不大于5.00V
        AmpValue = (AmpValue + 0.01f * awg_pow10_table[amp_digit_current]) > 5.00f
                       ? 5.00f
                       : (AmpValue + 0.01f * awg_pow10_table[amp_digit_current]);

        // 更新振幅显示字符串
        snprintf(amp_str, sizeof(amp_str), "%.2f", AmpValue);
        lcd_draw_string(188, PARAM_ROW_START_Y + 2 * PARAM_ROW_HEIGHT, amp_str, &yahei16x16,
                        COLOR_TEXT_HIGHLIGHT, COLOR_PARAM_BG, -3);

        StartBeezer(0); // 蜂鸣器反馈
        // 输出开启时，实时更新波形
        if (IsOutputOn == true) {
            GenerateWave(current_wave);
        }
        return;
    }
    // 确认键：返回参数选择页
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        awg_current_page = PAGE_PARAM_SELECT;
        awg_previous_page = PAGE_AMP_DIGIT;
        // 清空数位选择示意条
        lcd_draw_line(177, 195, 257, 195, COLOR_PARAM_BG);
        StartBeezer(0);
        return;
    } else {
        return; // 其他按键不处理
    }

    // 蜂鸣器反馈
    StartBeezer(0);

    // 清空旧的数位选择示意条
    lcd_draw_line(177, 195, 257, 195, COLOR_PARAM_BG);
    // 绘制新的数位选择示意条
    if (amp_digit_current == amp_hundredth || amp_digit_current == amp_tenth) {
        lcd_draw_line(182 + 13 * (3 - amp_digit_current), 195,
                      182 + 13 * (3 - amp_digit_current) + 10, 195, 0x87d3);
    } else {
        lcd_draw_line(182 + 13 * 1 - 8, 195, 182 + 13 * 1 + 10 - 8, 195, 0x87d3);
    }
}

/**
 * @brief 偏置参数编辑页按键处理函数
 * @param msg: 按键事件消息
 * @note  处理偏置数位选择、数值增减、返回参数选择页等操作
 */
void bias_digit_handler(KeyEventMsg_t msg) {
    // 编码器左转（按下）：切换到更高数位
    if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_PRESS_LEFT) {
        bias_digit_current = (bias_digit_current < bias_unit) ? (bias_digit_current + 1) : bias_hundredth;
    }
    // 编码器右转（按下）：切换到更低数位
    else if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        bias_digit_current = (bias_digit_current > bias_hundredth) ? (bias_digit_current - 1) : bias_unit;
    }
    // 编码器左转：减少对应数位的数值
    else if (msg.event == ENCODER_EVENT_LEFT) {
        // 边界处理：不小于-5.00V
        BiasValue = (BiasValue - 0.01f * awg_pow10_table[bias_digit_current]) < -5.00f
                        ? -5.00f
                        : (BiasValue - 0.01f * awg_pow10_table[bias_digit_current]);

        // 更新偏置显示字符串
        snprintf(bias_str, sizeof(bias_str), "%+.2f", BiasValue);
        lcd_draw_string(175, PARAM_ROW_START_Y + 3 * PARAM_ROW_HEIGHT, bias_str, &yahei16x16,
                        COLOR_TEXT_HIGHLIGHT, COLOR_PARAM_BG, -3);

        StartBeezer(0); // 蜂鸣器反馈
        // 输出开启时，实时更新波形
        if (IsOutputOn == true) {
            GenerateWave(current_wave);
        }
        return;
    }
    // 编码器右转：增加对应数位的数值
    else if (msg.event == ENCODER_EVENT_RIGHT) {
        // 边界处理：不大于5.00V
        BiasValue = (BiasValue + 0.01f * awg_pow10_table[bias_digit_current]) > 5.00f
                        ? 5.00f
                        : (BiasValue + 0.01f * awg_pow10_table[bias_digit_current]);

        // 更新偏置显示字符串
        snprintf(bias_str, sizeof(bias_str), "%+.2f", BiasValue);
        lcd_draw_string(175, PARAM_ROW_START_Y + 3 * PARAM_ROW_HEIGHT, bias_str, &yahei16x16,
                        COLOR_TEXT_HIGHLIGHT, COLOR_PARAM_BG, -3);

        StartBeezer(0); // 蜂鸣器反馈
        // 输出开启时，实时更新波形
        if (IsOutputOn == true) {
            GenerateWave(current_wave);
        }
        return;
    }
    // 确认键：返回参数选择页
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        awg_current_page = PAGE_PARAM_SELECT;
        awg_previous_page = PAGE_BIAS_DIGIT;
        // 清空数位选择示意条
        lcd_draw_line(177, 224, 257, 224, COLOR_PARAM_BG);
        StartBeezer(0);
        return;
    } else {
        return; // 其他按键不处理
    }

    // 蜂鸣器反馈
    StartBeezer(0);

    // 清空旧的数位选择示意条
    lcd_draw_line(177, 224, 257, 224, COLOR_PARAM_BG);
    // 绘制新的数位选择示意条
    if (bias_digit_current == bias_hundredth || bias_digit_current == bias_tenth) {
        lcd_draw_line(182 + 13 * (3 - bias_digit_current), 224,
                      182 + 13 * (3 - bias_digit_current) + 10, 224, 0x87d3);
    } else {
        lcd_draw_line(182 + 13 * 1 - 8, 224, 182 + 13 * 1 + 10 - 8, 224, 0x87d3);
    }
}

/************************ 页面处理函数映射表 ************************/
static void (*page_handlers[PAGE_NUM])(KeyEventMsg_t) = {
    [PAGE_MAIN] = awg_main_page_handler,
    [PAGE_PARAM_SELECT] = param_select_handler,
    [PAGE_FREQ_DIGIT] = freq_digit_handler,
    [PAGE_DUTY_DIGIT] = duty_digit_handler,
    [PAGE_AMP_DIGIT] = amp_digit_handler,
    [PAGE_BIAS_DIGIT] = bias_digit_handler,
};

/************************ 任务入口函数 ************************/
KeyEventMsg_t AWG_Keymsg; // 按键事件消息缓存

/**
 * @brief AWG任务入口函数
 * @param argument: 任务参数（未使用）
 * @note  循环读取按键消息，并分发到当前页面的处理函数
 */
void Start_AwgTask(void *argument) {
    // 任务启动时先挂起，等待外部唤醒
    osThreadSuspend(AwgTaskHandle);

    for (;;) {
        // 读取按键事件消息
        if (osMessageQueueGet(KeyEventQueueHandle, &AWG_Keymsg, NULL, 0) == osOK) {
            // 分发到当前页面的处理函数
            page_handlers[awg_current_page](AWG_Keymsg);
        }
    }
}

/************************ 任务控制函数 ************************/
/**
 * @brief 挂起AWG任务
 * @note  停止波形输出，挂起输入设备检测任务和AWG任务
 */
void Suspend_AwgTask(void) {
    WaveGen_Stop(); // 停止波形输出
    Suspend_IndevDetectTask(); // 挂起输入设备检测任务
    osThreadSuspend(AwgTaskHandle); // 挂起AWG任务
}

/**
 * @brief 恢复AWG任务
 * @note  重置页面状态，重新绘制界面，恢复输入设备检测任务和AWG任务
 */
void Resume_AwgTask(void) {
    // 重置页面状态
    awg_current_page = PAGE_MAIN;
    awg_previous_page = PAGE_MAIN;

    // 重置左侧栏状态
    ColumnPageIndex = 0;
    ColumnPageSelectIndex = 0;

    // 重置波形选择状态
    current_wave = Wave_Sine;
    previous_wave = Wave_Sine;

    // 重新绘制基础界面
    DrawAwgMainBasicElement();

    // 恢复任务运行
    osThreadResume(AwgTaskHandle);
    Resume_IndevDetectTask(); // 恢复输入设备检测任务
}