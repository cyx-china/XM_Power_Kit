/**
******************************************************************************
  * @file           : DsoCoreTask.c
  * @brief          : 数字示波器（DSO）系统核心任务，包括ADC采样数据处理、触发逻辑控制、
  *                   波形拉伸插值、LCD波形绘制、硬件中断回调及示波器参数管理
  * @date           : 2026/2/21
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  *
  * This DSO core task module is independently developed by the author.
  * It is released under the CC-BY-NC-SA 4.0 open source license.
  * And the author's right of attribution is reserved.
  ******************************************************************************
  */

/* ============================ 头文件包含 ============================ */
#include "DsoCoreTask.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fft.h"
#include "lcd_draw_api.h"
#include "main.h"
#include "UserTask.h"
#include "os_handles.h"
#include "SwitchManager.h"
#include "tim.h"
#include "UserDefineManage.h"

/* ============================ 静态全局缓冲区定义 ============================ */
// ADC环形缓冲区：存储ADC采样的原始数据，采用环形结构循环存储
static uint8_t DSO_ADC_BUFFER[DSO_Buffer_Size] = {0};

// 排序后的原始采样点缓冲区
// 结构说明：
// [0..1439] 触发点前的1440个采样点 | [1440] 触发点 | [1441..2880] 触发点后的1440个采样点
static uint8_t raw_sorted_buf[SORTED_BUF_SIZE];

// 时基拉伸/缩放处理后的缓冲区（高时基下通过插值扩展数据）
static uint8_t stretched_buf[SORTED_BUF_SIZE];

// 显示点缓冲区：存储最终用于LCD绘制的波形数据（经过峰值抽取处理）
static uint8_t DSO_SAMPLE_SHOW_BUFFER[DSO_ShowBuffer_Deep] = {0};

// 网格/波形刷新缓冲区：缓存LCD绘制的网格和波形数据，减少刷屏耗时（提升显示效率）
static uint16_t grid_buffer[1608] = {0};

/* ============================ 全局变量定义 ============================ */
// ---------------- 触发相关状态标志位 ----------------
volatile bool Is_Trig = false;          // 触发标志：是否已触发
volatile bool Is_conv_finish = false;   // ADC转换完成标志
volatile bool post_trigger_counting = false; // 后触发计数标志

volatile bool Is_Pause = false;         // 暂停标志：是否处于暂停状态

// ---------------- 触发相关参数 ----------------
volatile uint32_t   trigger_ndtr_at_capture = 0; // 捕获时刻的DMA剩余计数
volatile uint32_t   trigger_pos = 0;            // 触发点在ADC缓冲区中的位置
volatile int8_t     trigger_offset = 0;         // 触发电压偏移量
volatile int16_t    software_offset = 0;        // 软件触发微调偏移量

// ---------------- 示波器核心配置参数 ----------------
TimeBase_e           TimeBase = time_200us;     // 时基档位（默认200us/格）
Vdiv_e               V_div = div_500mv;         // 电压档位（默认500mV/格）
CoupleTypeDef        CoupleType = Couple_DC;    // 耦合方式（默认直流）
AttenuationTypeDef   AttenuationType = Attenuation_1; // 硬件衰减倍数
EdgeType_e           EdgeType = EDGE_RISING;    // 触发边沿类型（默认上升沿）
TriggerMode_e        TriggerMode = TrigMode_Auto; // 触发模式（默认自动）

// ---------------- 显示偏移参数 ----------------
int16_t offset_h = 0;  // 水平偏移量
int8_t offset_w = 0;   // 垂直偏移量
int8_t offset_v = 0;   // 触发电压偏移量

// ---------------- 波形特征参数 ----------------
float Wave_Freq = 0.0f; // 波形频率 (KHz)
float Wave_Duty = 0.0f; // 波形占空比 (%)
float Wave_Vpp  = 0.0f; // 波形峰峰值 (V)

// ---------------- 格式化字符缓冲区 ----------------
char Freq_buf[10] = {0}; // 频率显示字符串缓冲区
char Duty_buf[10] = {0}; // 占空比显示字符串缓冲区
char Vpp_buf[10]  = {0}; // 峰峰值显示字符串缓冲区

// ---------------- 按键响应区 ----------------
KeyEventMsg_t DSO_Keymsg; // 示波器按键事件消息
DSO_AppPage_t dso_current_page = DSO_PAGE_MAIN;             // 当前激活的功能页面
uint8_t Bias_triangle = 0;                                  // 偏移三角选中：0-左右偏移；1-上下偏移;2-调节触发
DSO_SettingMenu_t dso_current_setting = Setting_TimeBase;   // 当前激活的设置选项
DSO_SettingMenu_t dso_past_setting = -1;      // 上一个激活的设置选项

/* ============================ 函数声明 ============================ */
// 界面绘制与刷新函数
static void Refresh_DSO_Waveform(const uint8_t* sample_data, int16_t h_offset, int8_t v_offset);
static void DrawDsoMainBasicElement(void);
void Refresh_WaveInfo(void);

// 触发相关函数
static void TriggerHandler(void);
static int16_t Software_Trigger_FineTune(const uint8_t* buffer, EdgeType_e edge_type);
void TriggerVoltage_Set(int8_t offset);

// 数据处理函数
static void SortRawSamplesToBuffer(uint32_t trigger_pos);
static void ApplyStretchToBuffer(void);
static uint8_t CatmullRomInterp(uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3, float t);
void DSO_BuildShowBuffer(void);
static void Calculat_ZoomFactors(void);
void Div_Set(Vdiv_e div_mode);
void TimeBase_Set(TimeBase_e base);

// 波形参数计算与格式化函数
static void Calculate_WaveParas(void);
static void format_frequency(float freq_khz, char *buffer, size_t buffer_size);
static void format_percentage(float value, char *buffer, size_t buffer_size);
static void format_voltage(float voltage_v, char *buffer, size_t buffer_size);

// 中断回调函数
void CB_Trigger_CaptureCallback(TIM_HandleTypeDef *htim);
void CB_DSO_TIM11_UpdateCallback(TIM_HandleTypeDef *htim);
void CB_ADC_HalfConvCplt(ADC_HandleTypeDef *hadc);
void CB_ADC_ConvCplt(ADC_HandleTypeDef *hadc);

// 核心任务函数
void Start_DsoCoreTask(void *argument);
static void (*page_handlers[DSO_PAGE_NUM])(KeyEventMsg_t);

/* ============================ 中断回调函数 ============================ */
/**
 * @brief  TIM3触发捕获中断回调函数（硬件边沿触发）
 * @param  htim: 定时器句柄
 * @retval None
 */
void CB_Trigger_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance != TIM3) return;
    TriggerHandler();
}

/**
 * @brief  TIM11更新中断回调函数（定时触发）
 * @param  htim: 定时器句柄
 * @retval None
 */
void CB_DSO_TIM11_UpdateCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance != TIM11) return;
    TriggerHandler();
}

/**
 * @brief  ADC半转换完成回调函数
 * @param  hadc: ADC句柄
 * @retval None
 */
void CB_ADC_HalfConvCplt(ADC_HandleTypeDef *hadc) {
    if (!post_trigger_counting) return;

    uint32_t current_ndtr = __HAL_DMA_GET_COUNTER(&hdma_adc1);
    uint32_t post = (trigger_ndtr_at_capture - current_ndtr + DSO_Buffer_Size) % DSO_Buffer_Size;

    // 后触发采样达到 POST_TRIGGER_POINTS 点，停止采样并标记完成
    if (post >= POST_TRIGGER_POINTS) {
        AdcTim_OFF();
        Is_conv_finish = true;
        post_trigger_counting = false;
    }
}

/**
 * @brief  ADC全转换完成回调函数
 * @param  hadc: ADC句柄
 * @retval None
 */
void CB_ADC_ConvCplt(ADC_HandleTypeDef *hadc) {
    if (!post_trigger_counting) return;

    uint32_t current_ndtr = __HAL_DMA_GET_COUNTER(&hdma_adc1);
    uint32_t post = (trigger_ndtr_at_capture - current_ndtr + DSO_Buffer_Size) % DSO_Buffer_Size;

    if (post >= POST_TRIGGER_POINTS) {
        AdcTim_OFF();
        Is_conv_finish = true;
        post_trigger_counting = false;
    }
}

/* ============================ 核心任务入口函数 ============================ */
/**
 * @brief  DSO核心任务主循环
 * @param  argument: 任务入参（未使用）
 * @retval None
 */
void Start_DsoCoreTask(void *argument) {
    osThreadSuspend(DsoCoreTaskHandle); // 初始挂起任务，等待Resume

    uint16_t time_count = 0;

    for (;;) {
        time_count ++;
        // 处理按键消息
        if (osMessageQueueGet(KeyEventQueueHandle, &DSO_Keymsg, NULL, 0) == osOK) {
            page_handlers[dso_current_page](DSO_Keymsg);    // 按键操作映射
        }
        // ADC转换完成后处理波形数据
        if (Is_conv_finish && !Is_Pause) { // 波形转换完成且未暂停

            if (TriggerMode == TrigMode_Single) {
                Is_Pause = !Is_Pause;
                lcd_draw_rect(130,0,190,17,0x1082,1);
                lcd_draw_string(135,0,"STOP",&JetBrainsMono14x18,0xf000,0x1082,-2);
            }

            // 按触发点排序原始采样数据
            SortRawSamplesToBuffer(trigger_pos);
            // 应用时基拉伸/插值处理
            ApplyStretchToBuffer();
            // 构建显示缓冲区
            DSO_BuildShowBuffer();
            // 软件触发微调
            software_offset = Software_Trigger_FineTune(DSO_SAMPLE_SHOW_BUFFER, EdgeType);
            // 刷新LCD波形显示
            Refresh_DSO_Waveform(DSO_SAMPLE_SHOW_BUFFER, offset_h, offset_w);

            // 重置状态标志
            Is_conv_finish = false;
            Is_Trig = false;

            // 重置定时器计数器
            __HAL_TIM_SET_COUNTER(&htim11, 0);
            __HAL_TIM_SET_COUNTER(&htim8, 0);

            // 重启ADC采样
            HAL_ADC_Start_DMA(&hadc1, (uint32_t*)DSO_ADC_BUFFER, DSO_Buffer_Size);
            AdcTim_ON();
            osDelay(time_base[TimeBase].refresh_time / 2);      // 先让ADDC采集满一个周期

            // 重启触发相关定时器中断
            if (TriggerMode != TrigMode_Single) {HAL_TIM_Base_Start_IT(&htim11);}
            __HAL_TIM_ENABLE_IT(&htim3, TIM_IT_CC3);

            post_trigger_counting = false;
        }
        // 周期性计算并刷新波形参数
        if (time_count > 10 && !Is_Pause) {
            time_count = 0;
            Calculate_WaveParas();
            Refresh_WaveInfo();
        }

        osDelay(10); // 任务延时
    }
}

/* ============================ 界面绘制与刷新函数 ============================ */
/**
 * @brief  绘制触发边沿标识（上升沿/下降沿）
 * @param  x: 标识X坐标
 * @param  y: 标识Y坐标
 * @param  edge_type: 边沿类型（0-上升沿，1-下降沿）
 * @param  color: 标识颜色
 * @retval None
 */
void Draw_TrigEdge(uint16_t x, uint16_t y, uint8_t edge_type,uint16_t color) {
    if (edge_type == EDGE_RISING) {
        lcd_draw_line(x, y+13, x+4, y+13, color);
        lcd_draw_line(x+4, y, x+4, y+13, color);
        lcd_draw_line(x+4, y, x+8, y, color);
        lcd_draw_IsoscelesTriangle(x+4, y+4, Triangle_UP, 5, color);
    } else if (edge_type == EDGE_FALLING) {
        lcd_draw_line(x, y, x+4, y, color);
        lcd_draw_line(x+4, y, x+4, y+13, color);
        lcd_draw_line(x+4, y+13, x+8, y+13, color);
        lcd_draw_IsoscelesTriangle(x+4, y+9, Triangle_DOWN, 5, color);
    }
}

/**
 * @brief  绘制示波器主界面基础元素（边框、文字、触发标识等）
 * @retval None
 */
static void DrawDsoMainBasicElement(void) {
    // 绘制顶部状态栏背景
    lcd_draw_rect(0, 0, 319, 18, 0x1082, 1);
    // 绘制波形显示区域背景
    lcd_draw_rect(0, 18, 319, 221, 0x0000, 1);
    // 绘制底部参数栏背景
    lcd_draw_rect(0, 221, 319, 239, 0x1082, 1);

    // 绘制顶部状态栏文字/标识
    lcd_draw_string(141, 0, "RUN", &JetBrainsMono14x18, 0x07c4, 0x1082, -2);
    lcd_draw_string(7, 2, time_base[TimeBase].TimeBase_msg, &JetBrainsMono10x14, 0xffff, 0x1082, 0);
    lcd_draw_string(70, 2, div_base[V_div].DivBase_msg, &JetBrainsMono10x14, 0xffff, 0x1082, 0);
    Draw_TrigEdge(199, 2, EdgeType,0xffff);
    lcd_draw_IsoscelesTriangle(226, 12, Triangle_DOWN, 6, 0xfe60);
    lcd_draw_string(243, 0, TriggerMode_Msg[TriggerMode], &yahei16x16, 0xffff, 0x1082, 1);
    lcd_draw_string(290, 2, CoupleType_Msg[CoupleType], &JetBrainsMono10x14, 0xffff, 0x1082, 1);

    // 绘制底部参数栏文字
    lcd_draw_string(8, 224, "频率:", &yahei12x12, 0xffff, 0x1082, 1);
    lcd_draw_string(121, 224, "占空:", &yahei12x12, 0xffff, 0x1082, 1);
    lcd_draw_string(208, 224, "峰峰值:", &yahei12x12, 0xffff, 0x1082, 1);

    // 刷新波形参数显示
    Refresh_WaveInfo();

    // 绘制垂直参考三角
    lcd_draw_IsoscelesTriangle(7, 119, Triangle_RIGHT, 6, 0x057f);
    // 初始化触发电压
    TriggerVoltage_Set(0);
}

/**
 * @brief  刷新LCD上的波形显示
 * @param  sample_data: 显示缓冲区数据指针
 * @param  h_offset: 水平偏移量
 * @param  v_offset: 垂直偏移量
 * @retval None
 */
static void Refresh_DSO_Waveform(const uint8_t* sample_data, int16_t h_offset, int8_t v_offset) {
    static int16_t last_group_y = -1;
    int16_t final_h_offset = software_offset - h_offset;
    int16_t start_sample_idx = 150 + final_h_offset;
    if (start_sample_idx < 0) start_sample_idx = 0;
    if (start_sample_idx > 300) start_sample_idx = 300;

    const int16_t tri_wave_x0 = (159 + h_offset) - 9;
    const int16_t tri_wave_y0 = 24 - 19;
    const uint16_t tri_color = 0xfe60;
    const uint8_t tri_height = 6;

    // 绘制主波形区域（37列，每列8个像素）
    for (uint8_t col_block = 0; col_block < 37; col_block++) {
        uint16_t* dst = grid_buffer;
        // 填充网格背景
        for (uint16_t row = 0; row < 201; row++) {
            uint8_t byte = grid_data[row][col_block];
            *dst++ = (byte & 0x80) ? GRID_COLOR : BG_COLOR;
            *dst++ = (byte & 0x40) ? GRID_COLOR : BG_COLOR;
            *dst++ = (byte & 0x20) ? GRID_COLOR : BG_COLOR;
            *dst++ = (byte & 0x10) ? GRID_COLOR : BG_COLOR;
            *dst++ = (byte & 0x08) ? GRID_COLOR : BG_COLOR;
            *dst++ = (byte & 0x04) ? GRID_COLOR : BG_COLOR;
            *dst++ = (byte & 0x02) ? GRID_COLOR : BG_COLOR;
            *dst++ = (byte & 0x01) ? GRID_COLOR : BG_COLOR;
        }

        // 计算当前列的波形Y坐标
        int16_t y_cols[8] = {0};
        uint32_t idx = start_sample_idx + col_block * 8;
        for (uint8_t i = 0; i < 8; i++) {
            int32_t raw_y = y_lookup[sample_data[idx++]];
            y_cols[i] = (int16_t)CLAMP(raw_y - v_offset, 0, 200);
        }

        // 绘制波形线段（连接上一列最后一个点）
        if (last_group_y != -1) {
            int16_t min_y = (last_group_y < y_cols[0]) ? last_group_y : y_cols[0];
            int16_t max_y = (last_group_y > y_cols[0]) ? last_group_y : y_cols[0];
            for (int16_t y = min_y; y <= max_y; y++) grid_buffer[y * 8 + 0] = WAVE_COLOR;
        } else {
            grid_buffer[y_cols[0] * 8 + 0] = WAVE_COLOR;
        }
        // 绘制当前列内部的波形线段
        for (uint8_t i = 0; i < 7; i++) {
            int16_t min_y = (y_cols[i] < y_cols[i+1]) ? y_cols[i] : y_cols[i+1];
            int16_t max_y = (y_cols[i] > y_cols[i+1]) ? y_cols[i] : y_cols[i+1];
            for (int16_t y = min_y; y <= max_y; y++) grid_buffer[y * 8 + (i+1)] = WAVE_COLOR;
        }

        // 绘制触发三角标识
        const int16_t blk_x_start = col_block * 8;
        const int16_t blk_x_end = blk_x_start + 7;
        for (uint8_t i = 0; i < tri_height; i++) {
            const int16_t tx1 = tri_wave_x0 - i;
            const int16_t tx2 = tri_wave_x0 + i;
            const int16_t ty = tri_wave_y0 - i;
            if (ty < 0 || ty > 200) continue;

            const int16_t dx1 = (tx1 > blk_x_start) ? tx1 : blk_x_start;
            const int16_t dx2 = (tx2 < blk_x_end) ? tx2 : blk_x_end;
            if (dx1 > dx2) continue;

            for (int16_t x = dx1; x <= dx2; x++) {
                grid_buffer[ty * 8 + (x - blk_x_start)] = tri_color;
            }
        }

        // 输出到LCD
        lcd_draw_data(9 + col_block * 8, 19, 9 + col_block * 8 + 7, 219, grid_buffer, 1608);
        last_group_y = y_cols[7];
    }

    // 绘制最后5列（补充到300像素宽度）
    uint8_t last_col_block = 37;
    uint16_t* dst_last = grid_buffer;
    // 填充网格背景
    for (uint16_t row = 0; row < 201; row++) {
        uint8_t byte = grid_data[row][last_col_block];
        *dst_last++ = (byte & 0x80) ? GRID_COLOR : BG_COLOR;
        *dst_last++ = (byte & 0x40) ? GRID_COLOR : BG_COLOR;
        *dst_last++ = (byte & 0x20) ? GRID_COLOR : BG_COLOR;
        *dst_last++ = (byte & 0x10) ? GRID_COLOR : BG_COLOR;
        *dst_last++ = (byte & 0x08) ? GRID_COLOR : BG_COLOR;
    }

    // 计算最后列的波形Y坐标
    int16_t y_cols_last[5] = {0};
    uint32_t idx_last = start_sample_idx + last_col_block * 8;
    for (uint8_t i = 0; i < 5; i++) {
        int32_t raw_y = y_lookup[sample_data[idx_last++]];
        y_cols_last[i] = (int16_t)CLAMP(raw_y - v_offset, 0, 200);
    }

    // 绘制最后列的波形线段
    if (last_group_y != -1) {
        int16_t min_y = (last_group_y < y_cols_last[0]) ? last_group_y : y_cols_last[0];
        int16_t max_y = (last_group_y > y_cols_last[0]) ? last_group_y : y_cols_last[0];
        for (int16_t y = min_y; y <= max_y; y++) grid_buffer[y * 5 + 0] = WAVE_COLOR;
    }
    for (uint8_t i = 0; i < 4; i++) {
        int16_t min_y = (y_cols_last[i] < y_cols_last[i+1]) ? y_cols_last[i] : y_cols_last[i+1];
        int16_t max_y = (y_cols_last[i] > y_cols_last[i+1]) ? y_cols_last[i] : y_cols_last[i+1];
        for (int16_t y = min_y; y <= max_y; y++) grid_buffer[y * 5 + (i+1)] = WAVE_COLOR;
    }

    // 绘制最后列的触发三角标识
    const int16_t blk_x_start_last = last_col_block * 8;
    const int16_t blk_x_end_last = 300;
    for (uint8_t i = 0; i < tri_height; i++) {
        const int16_t tx1 = tri_wave_x0 - i;
        const int16_t tx2 = tri_wave_x0 + i;
        const int16_t ty = tri_wave_y0 - i;
        if (ty < 0 || ty > 200) continue;

        const int16_t dx1 = (tx1 > blk_x_start_last) ? tx1 : blk_x_start_last;
        const int16_t dx2 = (tx2 < blk_x_end_last) ? tx2 : blk_x_end_last;
        if (dx1 > dx2) continue;

        for (int16_t x = dx1; x <= dx2; x++) {
            grid_buffer[ty * 5 + (x - blk_x_start_last)] = tri_color;
        }
    }

    // 输出最后列到LCD
    lcd_draw_data(9 + last_col_block * 8, 19, 9 + last_col_block * 8 + 4, 219, grid_buffer, 1005);
}

/**
 * @brief  刷新LCD上的波形参数（频率、占空比、峰峰值）
 * @retval None
 */
void Refresh_WaveInfo(void) {
    // 格式化波形参数为字符串
    format_frequency(Wave_Freq, Freq_buf, sizeof(Freq_buf));
    format_percentage(Wave_Duty, Duty_buf, sizeof(Duty_buf));
    format_voltage(Wave_Vpp, Vpp_buf, sizeof(Vpp_buf));

    // 绘制到LCD
    lcd_draw_string(40, 224, Freq_buf, &MapleMono9x12, 0xffff, 0x1082, 0);
    lcd_draw_rect(153,224,195,236,0x1082,1);
    lcd_draw_string(153, 224, Duty_buf, &MapleMono9x12, 0xffff, 0x1082, 0);
    lcd_draw_rect(253,224,319,236,0x1082,1);
    lcd_draw_string(253, 224, Vpp_buf, &MapleMono9x12, 0xffff, 0x1082, 0);
}

/* ============================ 触发相关函数 ============================ */
/**
 * @brief  触发事件处理函数（硬件触发/定时触发统一处理）
 * @retval None
 */
static void TriggerHandler(void) {
    if (Is_Trig) return; // 已触发则跳过

    // 关闭触发相关中断，防止重复触发
    __HAL_TIM_DISABLE_IT(&htim3, TIM_IT_CC3);
    HAL_TIM_Base_Stop_IT(&htim11);

    // 设置触发状态
    Is_Trig = true;
    trigger_pos = (DSO_Buffer_Size - trigger_ndtr_at_capture) % DSO_Buffer_Size;
    post_trigger_counting = true;
}

/**
 * @brief  设置触发电压（通过DAC输出）
 * @param  offset: 触发电压偏移量（-100 ~ 100）
 * @retval None
 */
void TriggerVoltage_Set(int8_t offset) {
    // 限制偏移量范围
    int8_t offset_safe = CLAMP(offset, -100, 100);
    // 转换为DAC值（0~4095）
    uint16_t dac_value = (uint16_t)(((int32_t)(offset_safe + 100) * 4095 + 100) / 200);
    // 写入DAC
    MCP4725_WriteFast(&hmcp4725_OSC, dac_value, MCP4725_MODE_NORMAL);

    // 刷新触发电压标识
    lcd_draw_rect(311, 20, 319, 219, 0x0000, 1);
    lcd_draw_IsoscelesTriangle(311, 119 - offset_safe, Triangle_LEFT, 6, 0x07c2);

    // 转换为采样值偏移量（-127 ~ 127）
    trigger_offset = (int8_t)(((int16_t)offset_safe + 100) * 254 / 200 - 127);
}

/**
 * @brief  软件触发微调（在硬件触发基础上优化触发位置）
 * @param  buffer: 显示缓冲区指针
 * @param  edge_type: 触发边沿类型
 * @retval 最优偏移量
 */
static int16_t Software_Trigger_FineTune(const uint8_t* buffer, EdgeType_e edge_type) {
    const int16_t center = 300;          // 理想触发位置
    const int16_t search_range = 125;    // 搜索范围
    const uint8_t edge_threshold = 5;    // 边沿变化阈值
    const uint8_t voltage_window = 5;    // 电压匹配窗口

    uint8_t threshold_value = 127 + trigger_offset; // 触发阈值电压

    int16_t best_offset = 0;
    int32_t best_score = -1;

    // 遍历搜索范围内的所有点
    for (int16_t i = center - search_range; i <= center + search_range; i++) {
        if (i < 1 || i > 599) continue; // 超出缓冲区范围则跳过

        // 计算相邻点的电压变化
        uint8_t pos1 = buffer[i-1];
        uint8_t pos2 = buffer[i+1];
        int16_t diff = (int16_t)pos2 - (int16_t)pos1;

        // 判断是否为有效边沿
        bool is_valid_edge = false;
        if (edge_type == EDGE_RISING) {
            is_valid_edge = (diff > edge_threshold) &&
                            (abs((int16_t)buffer[i] - (int16_t)threshold_value) <= voltage_window);
        } else if (edge_type == EDGE_FALLING) {
            is_valid_edge = (diff < -edge_threshold) &&
                            (abs((int16_t)buffer[i] - (int16_t)threshold_value) <= voltage_window);
        }

        // 判断是否为过阈值边沿
        bool is_rect_edge = false;
        if (edge_type == EDGE_RISING && diff > 0) {
            is_rect_edge = (pos1 <= threshold_value && threshold_value <= pos2) ||
                           (pos2 <= threshold_value && threshold_value <= pos1);
        }
        if (edge_type == EDGE_FALLING && diff < 0) {
            is_rect_edge = (pos1 <= threshold_value && threshold_value <= pos2) ||
                           (pos2 <= threshold_value && threshold_value <= pos1);
        }

        // 计算评分（越靠近中心评分越高）
        if (is_valid_edge || is_rect_edge) {
            int16_t offset = i - center;
            int32_t score = 10000 - abs(offset) * 100;
            if (score > best_score) {
                best_score = score;
                best_offset = offset;
            }
        }
    }
    return best_offset;
}

/* ============================ 数据处理函数 ============================ */
/**
 * @brief  计算电压档位缩放因子
 * @retval None
 */
static void Calculat_ZoomFactors(void) {
    for (int i = 0; i < div_num; i++) {
        // 计算实际衰减倍数
        float atten = (div_base[i].attenuation == Attenuation_1) ? 1.0f : (1.0f / UserParam.OSC_Factor);
        // 计算放大倍数
        float gain = 1.0f;
        switch (div_base[i].magnify) {
            case Magnify_1:   gain = 1.0f; break;
            case Magnify_2:   gain = UserParam.OSC_AMP_X2; break;
            case Magnify_4:   gain = UserParam.OSC_AMP_X4; break;
            case Magnify_8:   gain = UserParam.OSC_AMP_X8; break;
            case Magnify_16:  gain = UserParam.OSC_AMP_X16; break;
            case Magnify_32:  gain = UserParam.OSC_AMP_X32; break;
            case Magnify_64:  gain = UserParam.OSC_AMP_X64; break;
            default: gain = 1.0f; break;
        }
        // 计算最终缩放因子
        div_base[i].zoom_factor = div_base[i].target_gain / (gain * atten);
    }
}

/**
 * @brief  设置电压档位（更新硬件衰减和放大倍数）
 * @param  div_mode: 电压档位
 * @retval None
 */
void Div_Set(Vdiv_e div_mode) {
    OSC_Attenuation_Ctrl(div_base[div_mode].attenuation); // 设置硬件衰减
    OSC_Magnify_Ctrl(div_base[div_mode].magnify);         // 设置硬件放大
}

void TimeBase_Set(TimeBase_e base) {
    AdcTim_OFF();                                       // 关闭TIM8
    HAL_TIM_Base_Stop_IT(&htim11);                      // 关闭TIM11定时中断
    HAL_TIM_IC_Stop_IT(&htim3, TIM_CHANNEL_3);   // 关闭TIM3捕获中断
    Is_Trig = false;                                    // 重置触发标志
    Is_conv_finish = false;                             // 重置ADC转换完成标志
    post_trigger_counting = false;                      // 重置后触发计数标志
    HAL_ADC_Stop_DMA(&hadc1);                           // 关闭ADC & DMA

    // 配置TIM8（ADC采样触发源）参数
    __HAL_TIM_SET_PRESCALER(&htim8, time_base[TimeBase].trigger_timer_psc);
    __HAL_TIM_SET_AUTORELOAD(&htim8, time_base[TimeBase].trigger_timer_arr);

    // 配置TIM11（定时触发）参数
    __HAL_TIM_SET_PRESCALER(&htim11, time_base[TimeBase].refresh_timer_psc);
    __HAL_TIM_SET_AUTORELOAD(&htim11, time_base[TimeBase].refresh_timer_arr);

    // 启动ADC DMA采样
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)DSO_ADC_BUFFER, DSO_Buffer_Size);

    // 启动定时器和中断
    AdcTim_ON();                          // 启动TIM8
    HAL_TIM_Base_Start_IT(&htim11);       // 启动TIM11定时中断
    HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_3); // 启动TIM3捕获中断
}


/**
 * @brief  将环形缓冲区数据按触发点排序到raw_sorted_buf
 * @param  trigger_pos: 触发点在ADC缓冲区中的位置
 * @retval None
 */
static void SortRawSamplesToBuffer(uint32_t trigger_pos) {
    const uint32_t pre_len = PRE_TRIGGER_POINTS;  // 触发前采样点数
    const uint32_t post_len = POST_TRIGGER_POINTS;// 触发后采样点数

    // 复制触发前数据
    if (trigger_pos >= pre_len) {
        memcpy(raw_sorted_buf, &DSO_ADC_BUFFER[trigger_pos - pre_len], pre_len);
    } else {
        // 环形缓冲区跨边界处理
        uint32_t len1 = pre_len - trigger_pos;
        memcpy(raw_sorted_buf, &DSO_ADC_BUFFER[DSO_Buffer_Size - len1], len1);
        memcpy(raw_sorted_buf + len1, DSO_ADC_BUFFER, trigger_pos);
    }

    // 复制触发点
    raw_sorted_buf[pre_len] = DSO_ADC_BUFFER[trigger_pos];

    // 复制触发后数据
    uint32_t next_pos = (trigger_pos + 1) % DSO_Buffer_Size;
    if (next_pos + post_len <= DSO_Buffer_Size) {
        memcpy(&raw_sorted_buf[pre_len + 1], &DSO_ADC_BUFFER[next_pos], post_len);
    } else {
        // 环形缓冲区跨边界处理
        uint32_t len1 = DSO_Buffer_Size - next_pos;
        memcpy(&raw_sorted_buf[pre_len + 1], &DSO_ADC_BUFFER[next_pos], len1);
        memcpy(&raw_sorted_buf[pre_len + 1 + len1], DSO_ADC_BUFFER, post_len - len1);
    }
}

/**
 * @brief  Catmull-Rom插值算法（用于波形拉伸）
 * @param  p0-p3: 插值相邻四个点
 * @param  t: 插值系数（0~1）
 * @retval 插值结果
 */
static uint8_t CatmullRomInterp(uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;

    // Catmull-Rom插值公式
    float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
    float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
    float c = -0.5f * p0 + 0.5f * p2;
    float d = p1;

    float value = a * t3 + b * t2 + c * t + d;
    // 限制输出范围
    if (value < 0.0f) return 0;
    if (value > 255.0f) return 255;
    return (uint8_t)value;
}

/**
 * @brief  对采样数据应用时基拉伸/插值处理
 * @retval None
 */
static void ApplyStretchToBuffer(void) {
    // 根据时基确定拉伸因子
    float stretch_factor;
    switch (TimeBase) {
        case time_20us:  stretch_factor = 2.5f;     break;
        case time_10us:  stretch_factor = 5.0f;     break;
        case time_5us:   stretch_factor = 10.0f;    break;
        default:         stretch_factor = 1.0f;     break;
    }

    // 拉伸因子≤1.1时直接复制（无需插值）
    if (stretch_factor <= 1.1f) {
        memcpy(stretched_buf, raw_sorted_buf, SORTED_BUF_SIZE);
    } else {
        // 拉伸因子>1.1时进行Catmull-Rom插值
        for (int i = 0; i < SORTED_BUF_SIZE; i++) {
            float raw_pos = i / stretch_factor;
            uint8_t max_val = 0;
            // 多组插值取最大值（提升波形清晰度）
            for (int k = -6; k <= 6; k++) {
                float pos = raw_pos + k * 0.15f;
                if (pos < 0.0f || pos >= SORTED_BUF_SIZE) continue;
                int32_t idx = (int32_t)pos;
                float t = pos - idx;
                // 处理边界点
                uint8_t p0 = raw_sorted_buf[(idx-1 < 0) ? 0 : idx-1];
                uint8_t p1 = raw_sorted_buf[idx];
                uint8_t p2 = raw_sorted_buf[(idx+1 >= SORTED_BUF_SIZE) ? SORTED_BUF_SIZE-1 : idx+1];
                uint8_t p3 = raw_sorted_buf[(idx+2 >= SORTED_BUF_SIZE) ? SORTED_BUF_SIZE-1 : idx+2];
                // 插值计算
                uint8_t interp_val = CatmullRomInterp(p0, p1, p2, p3, t);
                if (interp_val > max_val) max_val = interp_val;
            }
            stretched_buf[i] = max_val;
        }
    }

    // 应用电压档位缩放
    for (int i = 0; i < SORTED_BUF_SIZE; i++) {
        float code = (float)stretched_buf[i];
        stretched_buf[i] = (uint8_t) CLAMP(
            UserParam.OSC_Original + (code - UserParam.OSC_Original) * div_base[V_div].zoom_factor, 0.0f, 255.0f);
    }
}

/**
 * @brief  构建显示缓冲区（峰值抽取，将多采样点合并为单个显示点）
 * @retval None
 */
void DSO_BuildShowBuffer(void) {
    // 处理前触发区域（0~299）：PRE_TRIGGER_POINTS → 300个显示点
    for (int show_idx = 0; show_idx < 300; show_idx++) {
        uint32_t m_start = (show_idx * PRE_TRIGGER_POINTS) / 300;
        uint32_t m_end   = (((show_idx + 1) * PRE_TRIGGER_POINTS) + 299) / 300 - 1;
        uint8_t max_val = 0;
        // 抽取区间内最大值
        for (uint32_t m = m_start; m <= m_end; m++) {
            uint8_t val = stretched_buf[m];
            if (val > max_val) max_val = val;
        }
        DSO_SAMPLE_SHOW_BUFFER[show_idx] = max_val;
    }

    // 处理触发点（300）
    DSO_SAMPLE_SHOW_BUFFER[300] = stretched_buf[PRE_TRIGGER_POINTS];

    // 处理后触发区域（301~600）：POST_TRIGGER_POINTS → 300个显示点
    for (int show_idx = 301; show_idx < DSO_ShowBuffer_Deep; show_idx++) {
        int r_k = show_idx - 301;
        uint32_t b_start = (r_k * POST_TRIGGER_POINTS) / 300;
        uint32_t b_end   = (((r_k + 1) * POST_TRIGGER_POINTS) + 299) / 300 - 1;
        uint8_t max_val = 0;
        // 抽取区间内最大值
        for (uint32_t b = b_start; b <= b_end; b++) {
            uint8_t val = stretched_buf[PRE_TRIGGER_POINTS + 1 + b];
            if (val > max_val) max_val = val;
        }
        DSO_SAMPLE_SHOW_BUFFER[show_idx] = max_val;
    }
}

/* ============================ 波形参数计算与格式化函数 ============================ */
/**
 * @brief  计算波形参数（频率、占空比、峰峰值）
 * @retval None
 */
static void Calculate_WaveParas(void) {
    uint8_t current_max = DSO_SAMPLE_SHOW_BUFFER[0]; // 波形最大值
    uint8_t current_min = DSO_SAMPLE_SHOW_BUFFER[0]; // 波形最小值
    uint16_t high_cnt = 0;                           // 高电平计数

    // 遍历显示缓冲区，统计最大/最小值和高电平次数
    for (uint16_t i = 0;i < 601;i++) {
        uint8_t val = DSO_SAMPLE_SHOW_BUFFER[i];
        // 更新最大值
        if (val > current_max) {
            current_max = val;
        }
        // 更新最小值
        if (val < current_min) {
            current_min = val;
        }
        // 统计高电平（≥触发阈值）
        if (val >= 127 + trigger_offset) {
            high_cnt++;
        }
    }

    // 计算波形参数
    Wave_Freq = Get_Main_AC_Freq(raw_sorted_buf, time_base[TimeBase].sample_rate) / 1000;    // FFT计算频率 (KHz)
    Wave_Duty = (float)high_cnt / 601.0f * 100.0f;                                          // 占空比 (%)
    Wave_Vpp  =  (float)(current_max - current_min) / 256 * (div_base[V_div].div_value * 8); // 峰峰值 (V)
}

/**
 * @brief  格式化频率字符串（适配不同量程显示）
 * @param  freq_khz: 频率值 (KHz)
 * @param  buffer: 输出缓冲区
 * @param  buffer_size: 缓冲区大小
 * @retval None
 */
static void format_frequency(float freq_khz, char *buffer, size_t buffer_size) {
    float rounded;

    // <10KHz：保留3位小数
    if (freq_khz < 10.0f) {
        rounded = roundf(freq_khz * 1000.0f) / 1000.0f;
        snprintf(buffer, buffer_size, "%.3fKHz", rounded);
        return;
    }

    // 10~100KHz：保留2位小数，整数补零到2位
    if (freq_khz < 100.0f) {
        rounded = roundf(freq_khz * 100.0f) / 100.0f;
        snprintf(buffer, buffer_size, "%05.2fKHz", rounded);
        return;
    }

    // ≥100KHz：保留1位小数，整数补零到3位
    rounded = roundf(freq_khz * 10.0f) / 10.0f;
    snprintf(buffer, buffer_size, "%05.1fKHz", rounded);
}

/**
 * @brief  格式化百分比字符串（占空比专用）
 * @param  value: 百分比值
 * @param  buffer: 输出缓冲区
 * @param  buffer_size: 缓冲区大小
 * @retval None
 */
static void format_percentage(float value, char *buffer, size_t buffer_size) {
    float rounded;

    // <10%：保留2位小数
    if (value < 10.0f) {
        rounded = roundf(value * 100.0f) / 100.0f;
        snprintf(buffer, buffer_size, "%.2f%%", rounded);
        return;
    }

    // ≥10%：保留1位小数，整数补零到2位
    rounded = roundf(value * 10.0f) / 10.0f;

    if (rounded >= 100.0f - 1e-6f) {
        snprintf(buffer, buffer_size, "100%%");
    } else {
        snprintf(buffer, buffer_size, "%04.1f%%", rounded);
    }
}

/**
 * @brief  格式化电压字符串（峰峰值专用，适配mV/V单位）
 * @param  voltage_v: 电压值 (V)
 * @param  buffer: 输出缓冲区
 * @param  buffer_size: 缓冲区大小
 * @retval None
 */
static void format_voltage(float voltage_v, char *buffer, size_t buffer_size) {
    float value, rounded;

    // <1V：转换为mV显示
    if (voltage_v < 1.0f) {
        value = voltage_v * 1000.0f;

        // <10mV：保留3位小数
        if (value < 10.0f) {
            rounded = roundf(value * 1000.0f) / 1000.0f;
            snprintf(buffer, buffer_size, "%.3fmV", rounded);
            return;
        }

        // 10~100mV：保留2位小数，整数补零到2位
        if (value < 100.0f) {
            rounded = roundf(value * 100.0f) / 100.0f;
            snprintf(buffer, buffer_size, "%05.2fmV", rounded);
            return;
        }

        // 100~1000mV：保留1位小数，整数补零到3位
        rounded = roundf(value * 10.0f) / 10.0f;
        if (rounded < 1000.0f) {
            snprintf(buffer, buffer_size, "%05.1fmV", rounded);
            return;
        }

        // ≥1000mV：转换回V单位
        voltage_v = rounded / 1000.0f;
    }

    // ≥1V：以V为单位显示
    value = voltage_v;

    // 1~10V：保留3位小数
    if (value < 10.0f) {
        rounded = roundf(value * 1000.0f) / 1000.0f;
        snprintf(buffer, buffer_size, "%.3fV", rounded);
        return;
    }

    // ≥10V：保留2位小数，整数补零到2位
    rounded = roundf(value * 100.0f) / 100.0f;
    snprintf(buffer, buffer_size, "%05.2fV", rounded);
}

/* ============================ 硬件初始化/反初始化函数 ============================ */
/**
 * @brief  初始化DSO的ADC采集相关硬件
 * @note   配置ADC、GPIO、DMA，设置ADC触发源为TIM8 TRGO
 * @retval None
 */
void DSO_ADC_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 配置ADC1核心参数
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution = ADC_RESOLUTION_8B;          // 8位分辨率
    hadc1.Init.ScanConvMode = DISABLE;                  // 单通道模式
    hadc1.Init.ContinuousConvMode = DISABLE;            // 非连续转换（由TIM8触发）
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING; // 上升沿触发
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T8_TRGO; // TIM8 TRGO触发
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;         // 右对齐
    hadc1.Init.NbrOfConversion = 1;                     // 单次转换
    hadc1.Init.DMAContinuousRequests = ENABLE;          // 开启DMA连续请求
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    // 配置ADC通道3（PA3）
    sConfig.Channel = ADC_CHANNEL_3;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES; // 最短采样时间（提高采样率）
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    // 使能时钟
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // 配置PA3为模拟输入（ADC通道3）
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 配置PB0/PB1（未使用，预留）
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 配置DMA2 Stream4（ADC1 DMA）
    hdma_adc1.Instance = DMA2_Stream4;
    hdma_adc1.Init.Channel = DMA_CHANNEL_0;                     // 通道0
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;            // 外设到内存
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;                // 外设地址不递增
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;                    // 内存地址递增
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;   // 外设数据宽度：字节
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;      // 内存数据宽度：字节
    hdma_adc1.Init.Mode = DMA_CIRCULAR;                         // 环形模式
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;                // 高优先级
    hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;             // 关闭FIFO
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) {
        Error_Handler();
    }

    // 关联ADC和DMA句柄
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);
}

/**
 * @brief  反初始化DSO的ADC采集相关硬件
 * @note   停止ADC/DMA，关闭时钟，释放GPIO
 * @retval None
 */
void DSO_ADC_DeInit(void) {
    // 停止ADC和DMA
    HAL_ADC_Stop(&hadc1);
    HAL_ADC_Stop_DMA(&hadc1);
    HAL_DMA_PollForTransfer(&hdma_adc1, HAL_DMA_FULL_TRANSFER, 100);

    // 反初始化ADC、GPIO、DMA
    HAL_ADC_DeInit(&hadc1);
    __HAL_RCC_ADC1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_3);
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_0 | GPIO_PIN_1);
    HAL_DMA_DeInit(&hdma_adc1);
}

/**
 * @brief  挂起DSO核心任务
 * @note   预留函数，当前无实现
 * @retval None
 */
void Suspend_DsoCoreTask(void) {
    AdcTim_OFF();                                       // 关闭TIM8
    HAL_TIM_Base_Stop_IT(&htim11);                      // 关闭TIM11定时中断
    HAL_TIM_IC_Stop_IT(&htim3, TIM_CHANNEL_3);   // 关闭TIM3捕获中断
    Is_Trig = false;                                    // 重置触发标志
    Is_conv_finish = false;                             // 重置ADC转换完成标志
    post_trigger_counting = false;                      // 重置后触发计数标志
    HAL_ADC_Stop_DMA(&hadc1);                           // 关闭ADC & DMA
    DSO_ADC_DeInit();

    OSC_Couple_Ctrl(Couple_AC);                         // 耦合方式：AC
    Div_Set(div_5v);                           // 最大化前级
    Suspend_IndevDetectTask();                          // 挂起输入设备检测任务
    osThreadSuspend(DsoCoreTaskHandle);                 // 挂起DSO核心任务
}

void Resume_DsoCoreTask(void) {
    /* ==================== 新增：恢复所有变量默认值并清零缓冲区 ==================== */
    // 清零各缓冲区
    memset(DSO_ADC_BUFFER,         0, DSO_Buffer_Size);
    memset(raw_sorted_buf,         0, SORTED_BUF_SIZE);
    memset(stretched_buf,          0, SORTED_BUF_SIZE);
    memset(DSO_SAMPLE_SHOW_BUFFER, 0, DSO_ShowBuffer_Deep);
    memset(grid_buffer,            0, sizeof(grid_buffer));  // 1608 * sizeof(uint16_t)
    // 触发标志位
    Is_Trig                 = false;
    Is_conv_finish          = false;
    post_trigger_counting   = false;
    Is_Pause                = false;
    // 触发参数
    trigger_ndtr_at_capture = 0;
    trigger_pos             = 0;
    trigger_offset          = 0;
    software_offset         = 0;
    // 核心配置
    TimeBase        = time_200us;
    V_div           = div_500mv;
    CoupleType      = Couple_DC;
    AttenuationType = Attenuation_1;
    EdgeType        = EDGE_RISING;
    TriggerMode     = TrigMode_Auto;
    // 显示偏移
    offset_h = 0;
    offset_w = 0;
    offset_v = 0;
    // 波形特征
    Wave_Freq = 0.0f;
    Wave_Duty = 0.0f;
    Wave_Vpp  = 0.0f;
    // 字符串缓冲区清零
    memset(Freq_buf, 0, sizeof(Freq_buf));
    memset(Duty_buf, 0, sizeof(Duty_buf));
    memset(Vpp_buf,  0, sizeof(Vpp_buf));
    // 按键与页面状态
    memset(&DSO_Keymsg,0, sizeof(DSO_Keymsg));
    dso_current_page    = DSO_PAGE_MAIN;
    Bias_triangle       = 0;
    dso_current_setting = Setting_TimeBase;
    dso_past_setting    = -1;   // 无效选中状态

    /* ========================== 初始化流程 ========================== */
    Calculat_ZoomFactors();                 // 计算缩放因子（依赖已重置的 TimeBase、V_div）
    Div_Set(div_500mv);            // 设置硬件衰减/放大组合
    OSC_Couple_Ctrl(Couple_DC);             // 耦合方式：DC

    DrawDsoMainBasicElement();              // 绘制基础界面
    osDelay(100);                      // 延时确保界面绘制完成

    // 初始化ADC
    DSO_ADC_Init();
    // 注册回调
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_HALF_CB_ID, CB_ADC_HalfConvCplt);
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_COMPLETE_CB_ID, CB_ADC_ConvCplt);
    HAL_TIM_RegisterCallback(&htim3, HAL_TIM_IC_CAPTURE_CB_ID, CB_Trigger_CaptureCallback);
    HAL_TIM_RegisterCallback(&htim11, HAL_TIM_PERIOD_ELAPSED_CB_ID, CB_DSO_TIM11_UpdateCallback);

    // 配置定时器参数
    __HAL_TIM_SET_PRESCALER(&htim8, time_base[TimeBase].trigger_timer_psc);
    __HAL_TIM_SET_AUTORELOAD(&htim8, time_base[TimeBase].trigger_timer_arr);
    __HAL_TIM_SET_PRESCALER(&htim11, time_base[TimeBase].refresh_timer_psc);
    __HAL_TIM_SET_AUTORELOAD(&htim11, time_base[TimeBase].refresh_timer_arr);

    // 启动ADC DMA采样
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)DSO_ADC_BUFFER, DSO_Buffer_Size);

    // 启动定时器和中断
    AdcTim_ON();                            // 启动TIM8
    HAL_TIM_Base_Start_IT(&htim11);         // 启动TIM11定时中断
    HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_3); // 启动TIM3捕获中断

    // 恢复相关任务
    Resume_IndevDetectTask();
    osThreadResume(DsoCoreTaskHandle);
}


// ================================================================================================================== //
                /* ============================ 操作响应函数 ============================ */
static void dso_main_page_handler(KeyEventMsg_t msg);
static void dso_setting_page_handler(KeyEventMsg_t msg);

static void (*page_handlers[DSO_PAGE_NUM])(KeyEventMsg_t) = {
    [DSO_PAGE_MAIN]    = dso_main_page_handler,
    [DSO_PAGE_SETTING] = dso_setting_page_handler,
};

static void dso_main_page_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_SET && msg.event == KEY_EVENT_CLICK) {
        Is_Pause = !Is_Pause;
        lcd_draw_rect(130,0,190,17,0x1082,1);
        if (!Is_Pause) {
            lcd_draw_string(141, 0, "RUN", &JetBrainsMono14x18, 0x07c4, 0x1082, -2);
        }
        else {
            lcd_draw_string(135,0,"STOP",&JetBrainsMono14x18,0xf000,0x1082,-2);
        }
    }
    else if (msg.key == KEY_MODE && msg.event == KEY_EVENT_CLICK) {
        Bias_triangle = (Bias_triangle + 1) % 3;  // 0,1,2,0 循环
        lcd_draw_rect(220,0,234,17,0x1082,1);

        if (Bias_triangle == 0) {
            lcd_draw_IsoscelesTriangle(226, 12, Triangle_DOWN, 6, 0xfe60);
        }
        else if (Bias_triangle == 1){
            lcd_draw_IsoscelesTriangle(229, 9, Triangle_RIGHT, 6, 0x057f);
        }
        else if (Bias_triangle == 2) {
            lcd_draw_IsoscelesTriangle(223, 9, Triangle_LEFT, 6, 0x07c2);
        }
    }
    else if (msg.key == KEY_ENCODER && msg.event == KEY_EVENT_CLICK) {
        offset_h = 0;
        offset_w = 0;
        offset_v = 0;
        if (Is_Pause){Refresh_DSO_Waveform(DSO_SAMPLE_SHOW_BUFFER, offset_h, offset_w);}
        lcd_draw_rect(0,25,8,210,0x0000,1);
        lcd_draw_IsoscelesTriangle(7, 119, Triangle_RIGHT, 6, 0x057f);
        TriggerVoltage_Set(0);
    }
    else if (msg.event == ENCODER_EVENT_LEFT || msg.key == KEY_DOWN) {

        if (Bias_triangle == 0) {
            offset_h -= 2;
            offset_h = CLAMP(offset_h,-126,126);
            if (Is_Pause){Refresh_DSO_Waveform(DSO_SAMPLE_SHOW_BUFFER, offset_h, offset_w);}
        }
        else if (Bias_triangle == 1) {
            offset_w -= 2;
            offset_w = CLAMP(offset_w,-75,75);
            lcd_draw_rect(0,25,8,210,0x0000,1);
            lcd_draw_IsoscelesTriangle(7, 119 - offset_w, Triangle_RIGHT, 6, 0x057f);
            if (Is_Pause){Refresh_DSO_Waveform(DSO_SAMPLE_SHOW_BUFFER, offset_h, offset_w);}
        }
        else if (Bias_triangle == 2) {
            offset_v -= 2;
            offset_v = CLAMP(offset_v,-80,80);
            TriggerVoltage_Set(offset_v);
        }
    }
    else if (msg.event == ENCODER_EVENT_RIGHT || msg.key == KEY_UP) {
        if (Bias_triangle == 0) {
            offset_h += 2;
            offset_h = CLAMP(offset_h,-126,126);
            if (Is_Pause){Refresh_DSO_Waveform(DSO_SAMPLE_SHOW_BUFFER, offset_h, offset_w);}
        }
        if (Bias_triangle == 1) {
            offset_w += 2;
            offset_w = CLAMP(offset_w,-75,75);
            lcd_draw_rect(0,25,8,210,0x0000,1);
            lcd_draw_IsoscelesTriangle(7, 119 - offset_w, Triangle_RIGHT, 6, 0x057f);
            if (Is_Pause){Refresh_DSO_Waveform(DSO_SAMPLE_SHOW_BUFFER, offset_h, offset_w);}
        }
        if (Bias_triangle == 2) {
            offset_v += 2;
            offset_v = CLAMP(offset_v,-80,80);
            TriggerVoltage_Set(offset_v);
        }
    }
    else if (msg.key == KEY_MODE && msg.event == KEY_EVENT_LONG_PRESS) {
        dso_current_page = DSO_PAGE_SETTING;
        dso_current_setting = Setting_TimeBase;
        dso_past_setting = -1;
        lcd_draw_rect(7,2,57,16,0x1082,1);
        lcd_draw_string(7, 2, time_base[TimeBase].TimeBase_msg, &JetBrainsMono10x14, 0xf3c0, 0x1082, 0);
    }
    else if (msg.key == KEY_ENCODER && msg.event == KEY_EVENT_LONG_PRESS) {
        AppListType APP = APP_LVGL;
        StartBeezer(0);
        osMessageQueuePut(AppSwitchQueueHandle, &APP, 0, 10);
        return;
    }
    else return;
    StartBeezer(0);
}


static void dso_setting_page_handler(KeyEventMsg_t msg) {
    if (msg.event == ENCODER_EVENT_LEFT) {
        dso_past_setting = dso_current_setting;
        dso_current_setting = (dso_current_setting == 0) ? Setting_CoupleMode : (dso_current_setting - 1);
    }
    else if (msg.event == ENCODER_EVENT_RIGHT) {
        dso_past_setting = dso_current_setting;
        dso_current_setting = (dso_current_setting == Setting_CoupleMode) ? Setting_TimeBase : (dso_current_setting + 1);
    }
    else if (msg.key == KEY_MODE && msg.event == KEY_EVENT_LONG_PRESS) {
        dso_current_page = DSO_PAGE_MAIN;
        dso_past_setting = dso_current_setting;
    }
    else if (msg.key == KEY_UP ) {
        switch (dso_current_setting) {
            case Setting_TimeBase:
                TimeBase = (TimeBase < time_100ms) ? (TimeBase + 1) : TimeBase;     // 增加时基
                TimeBase_Set(TimeBase);
                break;
            case Setting_VoltageDiv:
                V_div = (V_div < div_5v) ? (V_div + 1) : V_div;
                Div_Set(V_div);
                break;
            case Setting_TriggerEdge:
                EdgeType = EDGE_RISING;
                Draw_TrigEdge(199, 2, EdgeType,0xf3c0);
                break;
            case Setting_TriggerMode:
                TriggerMode = TrigMode_Auto;
                HAL_TIM_Base_Start_IT(&htim11);       // 启动TIM11定时中断(定时触发)
                break;
            case Setting_CoupleMode:
                CoupleType = !CoupleType;
                OSC_Couple_Ctrl(CoupleType);
                break;
        }
    }
    else if (msg.key == KEY_DOWN) {
        switch (dso_current_setting) {
            case Setting_TimeBase:
                TimeBase = (TimeBase > time_5us) ? (TimeBase - 1) : TimeBase;     // 递减时基
                TimeBase_Set(TimeBase);
                break;
            case Setting_VoltageDiv:
                V_div = (V_div > div_5mv) ? (V_div - 1) : V_div;
                Div_Set(V_div);
                break;
            case Setting_TriggerEdge:
                EdgeType = EDGE_FALLING;
                Draw_TrigEdge(199, 2, EdgeType,0xf3c0);
                break;
            case Setting_TriggerMode:
                TriggerMode = TrigMode_Single;
                HAL_TIM_Base_Stop_IT(&htim11);       // 关闭TIM11定时中断(定时触发)
                break;
            case Setting_CoupleMode:
                CoupleType = !CoupleType;
                OSC_Couple_Ctrl(CoupleType);
                break;
        }
    }
    else if (msg.key == KEY_SET && msg.event == KEY_EVENT_CLICK) {
        Is_Pause = !Is_Pause;
        lcd_draw_rect(130,0,190,17,0x1082,1);
        if (!Is_Pause) {
            lcd_draw_string(141, 0, "RUN", &JetBrainsMono14x18, 0x07c4, 0x1082, -2);
        }
        else {
            lcd_draw_string(135,0,"STOP",&JetBrainsMono14x18,0xf000,0x1082,-2);
        }
    }
    else return;

    StartBeezer(0);

    // 绘制当前项目的选中状态,或者更新值
    switch (dso_current_setting) {
        case Setting_TimeBase:
            lcd_draw_rect(7,2,57,16,0x1082,1);
            lcd_draw_string(7, 2, time_base[TimeBase].TimeBase_msg, &JetBrainsMono10x14, 0xf3c0, 0x1082, 0);
            break;
        case Setting_VoltageDiv:
            lcd_draw_rect(70,2,120,16,0x1082,1);
            lcd_draw_string(70, 2, div_base[V_div].DivBase_msg, &JetBrainsMono10x14, 0xf3c0, 0x1082, 0);
            break;
        case Setting_TriggerEdge:
            lcd_draw_rect(195,0,210,16,0x1082,1);
            Draw_TrigEdge(199, 2, EdgeType,0xf3c0);
            break;
        case Setting_TriggerMode:
            lcd_draw_string(243, 0, TriggerMode_Msg[TriggerMode], &yahei16x16, 0xf3c0, 0x1082, 1);
            break;
        case Setting_CoupleMode:
            lcd_draw_string(290, 2, CoupleType_Msg[CoupleType], &JetBrainsMono10x14, 0xf3c0, 0x1082, 1);
            break;
    }
    // 清除上一个项目的选中状态，或者更新值
    switch (dso_past_setting) {
        case Setting_TimeBase:
            lcd_draw_rect(7,2,57,16,0x1082,1);
            lcd_draw_string(7, 2, time_base[TimeBase].TimeBase_msg, &JetBrainsMono10x14, 0xffff, 0x1082, 0);
            break;
        case Setting_VoltageDiv:
            lcd_draw_rect(70,2,120,16,0x1082,1);
            lcd_draw_string(70, 2, div_base[V_div].DivBase_msg, &JetBrainsMono10x14, 0xffff, 0x1082, 0);
            break;
        case Setting_TriggerEdge:
            lcd_draw_rect(195,0,210,16,0x1082,1);
            Draw_TrigEdge(199, 2, EdgeType,0xffff);
            break;
        case Setting_TriggerMode:
            lcd_draw_string(243, 0, TriggerMode_Msg[TriggerMode], &yahei16x16, 0xffff, 0x1082, 1);
            break;
        case Setting_CoupleMode:
            lcd_draw_string(290, 2, CoupleType_Msg[CoupleType], &JetBrainsMono10x14, 0xffff, 0x1082, 1);
            break;
    }
}