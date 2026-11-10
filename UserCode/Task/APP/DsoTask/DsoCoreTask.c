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
#include "format.h"
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
#include "ff.h"

/* ============================ 静态全局缓冲区定义 ============================ */
// ADC环形缓冲区：存储ADC采样的原始数据，采用环形结构循环存储
static uint8_t DSO_ADC_BUFFER[DSO_Buffer_Size] = {0};

// 帧内临时缓冲：raw_sorted_buf 与 grid_buffer 在同一帧内是先后使用、生命周期不重叠的
// （排序→插值/FFT 用 raw_sorted_buf；之后刷新波形用 grid_buffer），合并进同一个 union 省 2881 字节 RAM
typedef union {
    uint8_t  raw[SORTED_BUF_SIZE];      // 排序后的原始采样点，供插值与 FFT 使用
    uint16_t grid[1608];                // 波形刷新贴图（每列 8 像素 × 201 行）
} DsoFrameScratch_t;

static DsoFrameScratch_t dso_scratch;

// 排序后的原始采样点缓冲区
// [0..1439] 触发点前的1440个采样点 | [1440] 触发点 | [1441..2880] 触发点后的1440个采样点
#define raw_sorted_buf  (dso_scratch.raw)

// 时基拉伸/缩放处理后的缓冲区（高时基下通过插值扩展数据）
static uint8_t stretched_buf[SORTED_BUF_SIZE];

// 显示点缓冲区：存储最终用于LCD绘制的波形数据（经过峰值抽取处理）
static uint8_t DSO_SAMPLE_SHOW_BUFFER[DSO_ShowBuffer_Deep] = {0};

// 网格/波形刷新缓冲区（与 raw_sorted_buf 共用同一块内存，见上方 union 定义）
#define grid_buffer     (dso_scratch.grid)

/* ============================ 全局变量定义 ============================ */
// ---------------- 触发相关状态标志位 ----------------
volatile bool Is_Trig = false;                          // 触发标志：是否已触发
volatile bool Is_conv_finish = false;                   // ADC转换完成标志
volatile bool post_trigger_counting = false;            // 后触发计数标志

volatile bool Is_Pause = false;                         // 暂停标志：是否处于暂停状态

// 采集启动失败标记：HAL_ADC_Start_DMA 失败时置位，由主循环定期重试（避免静默定格）
static bool     capture_fault = false;
static uint32_t capture_retry_tick = 0;

// ---------------- 触发相关参数 ----------------
volatile uint32_t   trigger_ndtr_at_capture = 0;        // 捕获时刻的DMA剩余计数
volatile uint32_t   trigger_pos = 0;                    // 触发点在ADC缓冲区中的位置
volatile int8_t     trigger_offset = 0;                 // 触发电压偏移量
volatile int16_t    software_offset = 0;                // 软件触发微调偏移量

// ---------------- 示波器核心配置参数 ----------------
TimeBase_e           TimeBase = time_200us;             // 时基档位（默认200us/格）
Vdiv_e               V_div = div_500mv;                 // 电压档位（默认500mV/格）
CoupleTypeDef        CoupleType = Couple_DC;            // 耦合方式（默认直流）
AttenuationTypeDef   AttenuationType = Attenuation_1;   // 硬件衰减倍数
EdgeType_e           EdgeType = EDGE_RISING;            // 触发边沿类型（默认上升沿）
TriggerMode_e        TriggerMode = TrigMode_Auto;       // 触发模式（默认自动）

// ---------------- 显示偏移参数 ----------------
int16_t offset_h = 0;       // 水平偏移量
int8_t offset_w = 0;        // 垂直偏移量
int8_t offset_v = 0;        // 触发电压偏移量

// ---------------- 波形特征参数 ----------------
float Wave_Freq = 0.0f;     // 波形频率 (KHz)
float Wave_Duty = 0.0f;     // 波形占空比 (%)
float Wave_Vpp  = 0.0f;     // 波形峰峰值 (V)

// ---------------- 格式化字符缓冲区 ----------------
char Freq_buf[10] = {0};    // 频率显示字符串缓冲区
char Duty_buf[10] = {0};    // 占空比显示字符串缓冲区
char Vpp_buf[10]  = {0};    // 峰峰值显示字符串缓冲区

// ---------------- 参数字符串"影子"缓存 ----------------
// 记录上一次真正画到屏幕上的字符串，只有内容变化才重绘；清零表示下次必须刷新
static char Freq_buf_last[10] = {0};
static char Duty_buf_last[10] = {0};
static char Vpp_buf_last[10]  = {0};

// ---------------- 按键响应区 ----------------
KeyEventMsg_t DSO_Keymsg;                                   // 示波器按键事件消息
DSO_AppPage_t dso_current_page = DSO_PAGE_MAIN;             // 当前激活的功能页面
uint8_t Bias_triangle = 0;                                  // 偏移三角选中：0-左右偏移；1-上下偏移;2-调节触发
DSO_SettingMenu_t dso_current_setting = Setting_TimeBase;   // 当前激活的设置选项
DSO_SettingMenu_t dso_past_setting = -1;                    // 上一个激活的设置选项

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
static int16_t Current_Original_Offset(void);
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
void CB_DSO_ADC_ConvCplt(ADC_HandleTypeDef *hadc);

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
    /* 由硬件触发时，需要极快的速度读取到DMA的NDTR值，这样才能算到尽量准的触发点。
     * 因此我将此处的读取函数放在了 stm32f4xx_it.c 中，尽量避免函数层层调用带来的延时
     */

    //trigger_ndtr_at_capture = __HAL_DMA_GET_COUNTER(&hdma_adc1);
    TriggerHandler();
}

/**
 * @brief  TIM11更新中断回调函数（定时触发）
 * @param  htim: 定时器句柄
 * @retval None
 */
void CB_DSO_TIM11_UpdateCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance != TIM11) return;
    trigger_ndtr_at_capture = __HAL_DMA_GET_COUNTER(&hdma_adc1);
    TriggerHandler();
}

/**
 * @brief  ADC半转换完成回调函数
 * @param  hadc: ADC句柄
 * @retval None
 */
void CB_ADC_HalfConvCplt(ADC_HandleTypeDef *hadc) {
    if (!post_trigger_counting) return;     // 如果没进入后触发状态，直接返回

    uint32_t current_ndtr = __HAL_DMA_GET_COUNTER(&hdma_adc1);      // 读取当DMA的NDTR值
    uint32_t post = (trigger_ndtr_at_capture - current_ndtr + DSO_Buffer_Size) % DSO_Buffer_Size;  // 计算后触发已经采集的点数

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
void CB_DSO_ADC_ConvCplt(ADC_HandleTypeDef *hadc) {
    if (!post_trigger_counting) return;

    uint32_t current_ndtr = __HAL_DMA_GET_COUNTER(&hdma_adc1);
    uint32_t post = (trigger_ndtr_at_capture - current_ndtr + DSO_Buffer_Size) % DSO_Buffer_Size;

    if (post >= POST_TRIGGER_POINTS) {
        AdcTim_OFF();
        Is_conv_finish = true;
        post_trigger_counting = false;
    }
}

/* ============================ 采集启动/重启 ============================ */
/**
 * @brief  启动 ADC + DMA 采集（先停再启，保证状态机与 NDTR 都确定）
 * @retval true: 启动成功；false: 两次尝试都失败
 * @note   HAL_ADC_Start_DMA 在 ADC 不是 READY 状态时直接返回 HAL_BUSY 且什么都不做，
 *         原来不判返回值，一旦流/状态机出错就会静默定格在最后一帧，看起来像死机。
 */
static bool Dso_StartCapture(void) {
    (void)HAL_ADCEx_MultiModeStop_DMA(&hadc1);                 // 本来没在采也允许，返回值忽略
    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_EOC | ADC_FLAG_OVR); // 清掉上一次可能残留的标志
    if (HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t *)DSO_ADC_BUFFER, DSO_Buffer_Size) == HAL_OK) {
        return true;
    }
    (void)HAL_ADCEx_MultiModeStop_DMA(&hadc1);                 // 再兜一次：先停干净再重试
    return HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t *)DSO_ADC_BUFFER, DSO_Buffer_Size) == HAL_OK;
}

/**
 * @brief  重新武装一整轮采集（ADC/DMA + TIM8 触发 + 触发中断）
 * @param  wait_half_period: 是否等待半个刷新周期让缓冲区先填满
 * @retval true: 成功；false: ADC 启动失败（调用方置 capture_fault，主循环重试）
 */
static bool Dso_RestartAcquisition(bool wait_half_period) {
    if (!Dso_StartCapture()) {
        return false;
    }
    Is_Trig = false;                                            // 新一轮采集：清掉上一次的触发标志
    AdcTim_ON();
    if (wait_half_period) {
        osDelay(time_base[TimeBase].refresh_time / 2);          // 先让ADC采集满半个周期
    }
    if (TriggerMode != TrigMode_Single) {
        HAL_TIM_Base_Start_IT(&htim11);                         // 重启定时触发
    }
    __HAL_TIM_ENABLE_IT(&htim3, TIM_IT_CC3);                    // 重启硬件边沿捕获
    post_trigger_counting = false;
    return true;
}

/**
 * @brief  从"暂停"回到"运行"时重新武装采集
 * @note   暂停期间 ADC、定时触发、边沿捕获都已停下（最后一帧的回调里 AdcTim_OFF 且触发中断被关），
 *         如果只把 Is_Pause 清零，界面会一直定格在最后一帧。
 *         这里丢掉暂停期间残留的那一帧，重新启动采集；
 *         单次触发模式下再补一次定时触发，相当于"按一次恢复键采一帧"。
 */
static void Dso_ResumeRunning(void) {
    Is_conv_finish = false;                                     // 丢掉暂停期间残留的帧
    if (!Dso_RestartAcquisition(false)) {
        capture_fault = true;                                   // 交给主循环看门狗重试
        return;
    }
    if (TriggerMode == TrigMode_Single) {
        HAL_TIM_Base_Start_IT(&htim11);                         // 单次模式：补一次触发
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

    for (;;) {
        // 处理按键消息
        if (osMessageQueueGet(KeyEventQueueHandle, &DSO_Keymsg, NULL, 0) == osOK) {
            page_handlers[dso_current_page](DSO_Keymsg);    // 按键操作映射
        }
        // ADC转换完成后处理波形数据
        if (Is_conv_finish && !Is_Pause) { // 波形转换完成且未暂停

            if (TriggerMode == TrigMode_Single) {   // 如果是单次触发模式，那么直接暂停函数
                Is_Pause = true;                    // 这里必然处于运行态，直接置暂停（原来写成取反，容易误读）
                lcd_draw_rect(130,0,190,17,0x1082,1);
                lcd_draw_string(135,0,"STOP",&JetBrainsMono14x18,0xf000,0x1082,-2);
            }

            // 按触发点排序原始采样数据
            SortRawSamplesToBuffer(trigger_pos);
            // 应用时基拉伸/插值处理
            ApplyStretchToBuffer();
            // 构建显示缓冲区
            DSO_BuildShowBuffer();
            // 波形参数统计必须放在刷新波形之前：网格缓冲与原始缓冲共用一块内存，
            // Refresh_DSO_Waveform 会把 raw_sorted_buf 覆盖掉，之后再做 FFT 就没有数据了
            Calculate_WaveParas();
            // 软件触发微调
            software_offset = Software_Trigger_FineTune(DSO_SAMPLE_SHOW_BUFFER, EdgeType);
            // 刷新LCD波形显示
            Refresh_DSO_Waveform(DSO_SAMPLE_SHOW_BUFFER, offset_h, offset_w);
            // 参数一变就更新（Refresh_WaveInfo 内部按字符串比较，没变化不会重画）
            Refresh_WaveInfo();

            // 重置状态标志
            Is_conv_finish = false;
            Is_Trig = false;

            // 重置定时器计数器
            __HAL_TIM_SET_COUNTER(&htim11, 0);
            __HAL_TIM_SET_COUNTER(&htim8, 0);

            // 重启ADC采样与触发中断
            if (!Dso_RestartAcquisition(true)) {
                capture_fault = true;                           // 交给主循环看门狗重试
            }
        }
        // 采集看门狗：启动失败（或运行中出错）时每 200ms 重新武装一次，不再静默定格
        if (capture_fault && !Is_Pause && (osKernelGetTickCount() - capture_retry_tick >= 200U)) {
            capture_retry_tick = osKernelGetTickCount();
            if (Dso_RestartAcquisition(false)) {
                capture_fault = false;
            }
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
 * @note   当然可以先画完背景网格，再连线成波形，这样确实方便，但是会导致严重的屏闪。
 *         所以这里创建一个缓冲区，同时绘制背景和波形，随后刷到屏幕上，即可避免屏闪，但是复杂度很高。
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

    // 绘制最后5列(301 // 8 = 5)
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
    // 只有内容变化才重绘：三个参数都是慢变量，原来每 110ms 无条件刷屏一次
    // （影子缓存 Freq_buf_last 等在 Resume_DsoCoreTask 里清零，表示"必须刷新"）
    format_frequency(Wave_Freq, Freq_buf, sizeof(Freq_buf));
    format_percentage(Wave_Duty, Duty_buf, sizeof(Duty_buf));
    format_voltage(Wave_Vpp, Vpp_buf, sizeof(Vpp_buf));

    if (strcmp(Freq_buf, Freq_buf_last) != 0) {
        memcpy(Freq_buf_last, Freq_buf, sizeof(Freq_buf));
        lcd_draw_rect(40,224,120,236,0x1082,1);
        lcd_draw_string(40, 224, Freq_buf, &MapleMono9x12, 0xffff, 0x1082, 0);
    }
    if (strcmp(Duty_buf, Duty_buf_last) != 0) {
        memcpy(Duty_buf_last, Duty_buf, sizeof(Duty_buf));
        lcd_draw_rect(153,224,195,236,0x1082,1);
        lcd_draw_string(153, 224, Duty_buf, &MapleMono9x12, 0xffff, 0x1082, 0);
    }
    if (strcmp(Vpp_buf, Vpp_buf_last) != 0) {
        memcpy(Vpp_buf_last, Vpp_buf, sizeof(Vpp_buf));
        lcd_draw_rect(253,224,319,236,0x1082,1);
        lcd_draw_string(253, 224, Vpp_buf, &MapleMono9x12, 0xffff, 0x1082, 0);
    }
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
 * @brief  设置触发电压
 * @param  offset: 触发电压偏移量（-100 ~ 100）
 * @retval None
 */
void TriggerVoltage_Set(int8_t offset) {
    // 限制偏移量范围
    int8_t offset_safe = CLAMP(offset, -100, 100);
    // 转换为DAC值（0~4095）
    uint16_t dac_value = (uint16_t)(((int32_t)(offset_safe + 100) * 4095 + 100) / 200);
    // 写入DAC（MCP4725_OSC 挂在 I2C1 上，与 SensorTask 读 INA226/TMP102 共用总线，必须加锁）
    if (osMutexAcquire(IIC1_MutexHandle, osWaitForever) == osOK) {
        MCP4725_WriteFast(&hmcp4725_OSC, dac_value, MCP4725_MODE_NORMAL);
        osMutexRelease(IIC1_MutexHandle);
    }

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
    const int16_t search_range = 125;    // 搜索范围(±)
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

        // 判断是否为过阈值边沿 (专门用来判断像矩形波这种快边沿)
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
 * @note   软件进入时计算一次缩放因子，匹配实际
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
    HAL_ADCEx_MultiModeStop_DMA(&hadc1);                // 关闭ADC & DMA

    // 配置TIM8（ADC采样触发源）参数
    __HAL_TIM_SET_PRESCALER(&htim8, time_base[TimeBase].trigger_timer_psc);
    __HAL_TIM_SET_AUTORELOAD(&htim8, time_base[TimeBase].trigger_timer_arr);

    // 配置TIM11（定时触发）参数
    __HAL_TIM_SET_PRESCALER(&htim11, time_base[TimeBase].refresh_timer_psc);
    __HAL_TIM_SET_AUTORELOAD(&htim11, time_base[TimeBase].refresh_timer_arr);

    // 启动ADC DMA采样（失败时置故障标记，由主循环看门狗重试）
    if (!Dso_StartCapture()) {
        capture_fault = true;
    }

    // 启动定时器和中断
    AdcTim_ON();                          // 启动TIM8
    // 自动模式：定时触发一直开着；单次模式：只有处于运行态才补一次触发。
    // 否则在单次模式下暂停着改时基，会自己触发一帧、帧处理又把 Is_Pause 置位，
    // 看起来就像"按一下下键就把波形暂停了"。
    if (TriggerMode != TrigMode_Single || !Is_Pause) {
        HAL_TIM_Base_Start_IT(&htim11);
    }
    HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_3); // 启动TIM3捕获中断
}


/**
 * @brief  将环形缓冲区数据按触发点排序到raw_sorted_buf
 * @param  trigger_pos: 触发点在ADC缓冲区中的位置
 * @note   环形缓冲区，触发点随机，触发点前1440 + 后1440个点，如果经过起始点，就需要根据索引重排序
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
 * @brief  Catmull-Rom插值算法（用于波形拉伸）仅仅适用于时基很酷的情况
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
 * @brief  取当前电压档位对应的原点偏置校正量
 * @retval 校正量（定义：UserParam.OSC_Original_Xx = 理想值128 - 实际空载ADC值）
 * @note   显示码值 = 128 + (原始码值 + 校正量 - 128) * zoom，统计波形参数时也要反算回去
 */
static int16_t Current_Original_Offset(void) {
    switch (V_div) {
        case div_10mv:  return UserParam.OSC_Original_X32;
        case div_20mv:  return UserParam.OSC_Original_X16;
        case div_50mv:  return UserParam.OSC_Original_X8;
        case div_100mv: return UserParam.OSC_Original_X4;
        case div_200mv: return UserParam.OSC_Original_X2;
        case div_500mv: return UserParam.OSC_Original_X4;
        case div_1v:    return UserParam.OSC_Original_X2;
        case div_2v:    return UserParam.OSC_Original;
        default:        return 0;
    }
}

/**
 * @brief  对采样数据应用时基拉伸/插值处理 / 原点偏移校准及电压缩放
 * @retval None
 */
static void ApplyStretchToBuffer(void) {

    // 拉伸因子直接取自时基表：三重交替把真实采样率提高了 3 倍，
    // 同样显示窗口内的真实点也多了 3 倍，所以原来靠插值撑出来的几档倍率降到了 1/3
    const float stretch_factor = time_base[TimeBase].stretch;

    // 时基拉伸处理
    if (stretch_factor <= 1.1f) {
        // 拉伸因子≤1.1时直接复制（避免精度原因取1.1f）
        memcpy(stretched_buf, raw_sorted_buf, SORTED_BUF_SIZE);
    } else {
        // 拉伸因子>1.1时进行Catmull-Rom插值
        // 优化：1/stretch_factor 只算一次（原来是每个点做一次除法）；
        //       抽头由 13 个减到 5 个，覆盖范围不变（仍是 ±0.9 个样本），
        //       5µs 档每帧插值次数 37k → 14k，耗时约 8~10ms 降到 3~4ms。
        const float inv_stretch = 1.0f / stretch_factor;
        const float tap_step    = 0.45f;
        const float pos_max     = (float)(SORTED_BUF_SIZE - 2);   // 保证 idx+1 不越界
        for (int i = 0; i < SORTED_BUF_SIZE; i++) {
            float raw_pos = (float)i * inv_stretch;
            uint8_t max_val = 0;
            // 多组插值取最大值（提升波形清晰度）
            for (int k = -2; k <= 2; k++) {
                float pos = raw_pos + (float)k * tap_step;
                if (pos < 0.0f) pos = 0.0f;                     // 端点钳位，避免末尾出现黑点
                if (pos > pos_max) pos = pos_max;
                uint32_t idx = (uint32_t)pos;
                float t = pos - (float)idx;
                // 处理边界点
                uint8_t p0 = raw_sorted_buf[(idx == 0) ? 0 : idx - 1];
                uint8_t p1 = raw_sorted_buf[idx];
                uint8_t p2 = raw_sorted_buf[idx + 1];
                uint8_t p3 = raw_sorted_buf[(idx + 2 >= SORTED_BUF_SIZE) ? SORTED_BUF_SIZE - 1 : idx + 2];
                // 插值计算
                uint8_t interp_val = CatmullRomInterp(p0, p1, p2, p3, t);
                if (interp_val > max_val) max_val = interp_val;
            }
            stretched_buf[i] = max_val;
        }
    }

    // 根据电压档位获取对应的偏置校正量（统计函数里也要用，抽成公用函数）
    int16_t offset_calib = Current_Original_Offset();

    // 5. 应用偏置校正 + 围绕理想原点的电压缩放
    for (int i = 0; i < SORTED_BUF_SIZE; i++) {
        const int16_t IDEAL_ORIGIN = 128;
        float code = (float)stretched_buf[i];

        // 偏置校正
        float calibrated_code = code + (float)offset_calib;
        // 围绕理想原点(128)进行电压档位缩放
        float final_code = (float)IDEAL_ORIGIN + (calibrated_code - (float)IDEAL_ORIGIN) * div_base[V_div].zoom_factor;
        // 钳位到8bit显示范围(0-255)
        stretched_buf[i] = (uint8_t) CLAMP(final_code, 0.0f, 255.0f);
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
    // 统计对象换成"原始采样"：显示缓冲 DSO_SAMPLE_SHOW_BUFFER 已经被钳位到 0~255，
    // 超量程的波形会被压平导致读数偏小；而且它只有屏幕上的 301 列（由 601 点抽取）；
    // 2881 点原始数据也才能给 FFT 用。
    // 注意：本函数必须在 Refresh_DSO_Waveform 之前调用——网格缓冲与原始缓冲共用内存，
    // 刷新波形会把 raw_sorted_buf 覆盖掉。
    const uint8_t *raw = raw_sorted_buf;
    uint8_t raw_max = raw[0];                        // 波形最大值
    uint8_t raw_min = raw[0];                        // 波形最小值

    // 遍历原始缓冲区统计最大/最小值
    for (uint16_t i = 1; i < SORTED_BUF_SIZE; i++) {
        uint8_t val = raw[i];
        if (val > raw_max) raw_max = val;
        if (val < raw_min) raw_min = val;
    }

    // 峰峰值：原始码差 → 屏幕码差要乘电压档位的软件缩放因子（见 ApplyStretchToBuffer），
    // 满 256 码对应 div_value*8 伏（一屏 8 格）
    float zoom = div_base[V_div].zoom_factor;
    if (zoom < 0.01f) zoom = 0.01f;                  // 防除零（理论上不会发生）
    Wave_Vpp = (float)(raw_max - raw_min) * zoom / 256.0f * (div_base[V_div].div_value * 8.0f);

    // 占空比：触发阈值是"显示码值"(127+trigger_offset)，按
    // 显示 = 128 + (原始 + 校正量 - 128)*zoom 反算回原始码值，再统计高电平点数
    float raw_threshold = 128.0f + ((float)(127 + trigger_offset) - 128.0f) / zoom
                          - (float)Current_Original_Offset();
    uint16_t high_cnt = 0;                           // 高电平计数
    for (uint16_t i = 0; i < SORTED_BUF_SIZE; i++) {
        if ((float)raw[i] >= raw_threshold) high_cnt++;
    }
    Wave_Duty = (float)high_cnt / (float)SORTED_BUF_SIZE * 100.0f;                 // 占空比 (%)

    // 频率：对同一次采样的原始数据做 FFT
    Wave_Freq = Get_Main_AC_Freq(raw, time_base[TimeBase].sample_rate) / 1000.0f;  // FFT计算频率 (KHz)
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
        fmt_fixed_unit(buffer, buffer_size, rounded, 3, 0, 0, "KHz");
        return;
    }

    // 10~100KHz：保留2位小数，整数补零到2位
    if (freq_khz < 100.0f) {
        rounded = roundf(freq_khz * 100.0f) / 100.0f;
        fmt_fixed_unit(buffer, buffer_size, rounded, 2, 5, FMT_ZERO_PAD, "KHz");
        return;
    }

    // ≥100KHz：保留1位小数，整数补零到3位
    rounded = roundf(freq_khz * 10.0f) / 10.0f;
    fmt_fixed_unit(buffer, buffer_size, rounded, 1, 5, FMT_ZERO_PAD, "KHz");
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
        fmt_fixed_unit(buffer, buffer_size, rounded, 2, 0, 0, "%");
        return;
    }

    // ≥10%：保留1位小数，整数补零到2位
    rounded = roundf(value * 10.0f) / 10.0f;

    if (rounded >= 100.0f - 1e-6f) {
        snprintf(buffer, buffer_size, "100%%");
    } else {
        fmt_fixed_unit(buffer, buffer_size, rounded, 1, 4, FMT_ZERO_PAD, "%");
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
            fmt_fixed_unit(buffer, buffer_size, rounded, 3, 0, 0, "mV");
            return;
        }

        // 10~100mV：保留2位小数，整数补零到2位
        if (value < 100.0f) {
            rounded = roundf(value * 100.0f) / 100.0f;
            fmt_fixed_unit(buffer, buffer_size, rounded, 2, 5, FMT_ZERO_PAD, "mV");
            return;
        }

        // 100~1000mV：保留1位小数，整数补零到3位
        rounded = roundf(value * 10.0f) / 10.0f;
        if (rounded < 1000.0f) {
            fmt_fixed_unit(buffer, buffer_size, rounded, 1, 5, FMT_ZERO_PAD, "mV");
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
        fmt_fixed_unit(buffer, buffer_size, rounded, 3, 0, 0, "V");
        return;
    }

    // ≥10V：保留2位小数，整数补零到2位
    rounded = roundf(value * 100.0f) / 100.0f;
    fmt_fixed_unit(buffer, buffer_size, rounded, 2, 5, FMT_ZERO_PAD, "V");
}

/* ============================ 三重交替采样（ADC1+ADC2+ADC3） ============================ */
/*
 * 三个 ADC 采同一个引脚 PA3（ADC123_IN3），由主机 ADC1 的外部触发（TIM8 TRGO）统一发起；
 * 采样时刻由 ADC_CCR.DELAY 错开，DMA（模式1）每个序列按 ADC1→ADC2→ADC3 传 3 个字节，
 * 所以合成采样率是单 ADC 的 3 倍：2.4 MSa/s → 7.2 MSa/s。
 */
static ADC_HandleTypeDef s_dso_hadc2;                  // 从机 ADC2
static ADC_HandleTypeDef s_dso_hadc3;                  // 从机 ADC3
static bool              s_adc_multi = true;           // true: 三重交替；false: 只用 ADC1（校准流程）

void DSO_ADC_SetMulti(bool on) {
    s_adc_multi = on;
}

/**
 * @brief  配置一个参与多模式的 ADC（主机/从机参数略有差别）
 * @param  h: ADC 句柄
 * @param  inst: ADC1/ADC2/ADC3
 * @param  is_master: 主机(ADC1)负责外部触发与 DMA 请求，从机只跟着多模式硬件走
 */
static void dso_adc_config(ADC_HandleTypeDef *h, ADC_TypeDef *inst, bool is_master) {
    ADC_ChannelConfTypeDef ch = {0};

    h->Instance = inst;
    h->Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV2;   // ADCCLK = 72/2 = 36 MHz
    h->Init.Resolution            = ADC_RESOLUTION_8B;          // 8 位（与 8 位采样缓冲一致）
    h->Init.ScanConvMode          = DISABLE;                    // 单通道
    h->Init.ContinuousConvMode    = DISABLE;                    // 不连续转换，由 TIM8 触发
    h->Init.DiscontinuousConvMode = DISABLE;
    h->Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    h->Init.NbrOfConversion       = 1;
    h->Init.EOCSelection          = ADC_EOC_SEQ_CONV;
    if (is_master) {
        h->Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
        h->Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T8_TRGO;
        h->Init.DMAContinuousRequests = ENABLE;
    } else {
        // 从机的触发由多模式硬件统一调度，自己的触发源与 DMA 请求都不参与
        h->Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
        h->Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T8_TRGO;
        h->Init.DMAContinuousRequests = DISABLE;
    }
    if (HAL_ADC_Init(h) != HAL_OK) {
        Error_Handler();
    }

    ch.Channel      = ADC_CHANNEL_3;                 // PA3 = ADC123_IN3
    ch.Rank         = 1;
    ch.SamplingTime = ADC_SAMPLETIME_3CYCLES;        // 最短采样时间（15 个 ADCCLK ≈ 417 ns）
    if (HAL_ADC_ConfigChannel(h, &ch) != HAL_OK) {
        Error_Handler();
    }

    if (!is_master) {
        // HAL_ADCEx_MultiModeStart_DMA 只会打开主机，从机必须自己置 ADON（配置通道之后才能打开）
        SET_BIT(inst->CR2, ADC_CR2_ADON);
    }
}

/* ============================ 硬件初始化/反初始化函数 ============================ */
/**
 * @brief  初始化DSO的ADC采集相关硬件
 * @note   配置ADC、GPIO、DMA，设置ADC触发源为TIM8 TRGO
 * @retval None
 */
void DSO_ADC_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* ---------- 三重交替采样：ADC1(主机) + ADC2/ADC3(从机) ---------- */
    dso_adc_config(&hadc1, ADC1, true);
    __HAL_RCC_ADC2_CLK_ENABLE();
    __HAL_RCC_ADC3_CLK_ENABLE();
    if (s_adc_multi) {
        dso_adc_config(&s_dso_hadc2, ADC2, false);
        dso_adc_config(&s_dso_hadc3, ADC3, false);

        // 多模式必须在三个 ADC 都配置好之后再设（HAL_ADC_Init 会清掉 CCR 的 MULTI 位）
        ADC_MultiModeTypeDef mm = {0};
        mm.Mode             = ADC_TRIPLEMODE_INTERL;          // 三重交替
        mm.DMAAccessMode    = ADC_DMAACCESSMODE_1;            // DMA 模式1：CDR 里的数据按 1→2→3 逐个搬（宽度由 DMA 决定）
        mm.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_5CYCLES;   // 错开 5 个 ADCCLK ≈ 139 ns
        if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &mm) != HAL_OK) {
            Error_Handler();
        }
    } else {
        // 校准流程要求"一个触发一个样本"：强制回到独立模式
        ADC->CCR &= ~(ADC_CCR_MULTI | ADC_CCR_DMA | ADC_CCR_DDS);
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
    HAL_ADCEx_MultiModeStop_DMA(&hadc1);
    HAL_ADC_Stop(&hadc1);
    HAL_DMA_PollForTransfer(&hdma_adc1, HAL_DMA_FULL_TRANSFER, 100);

    // 退出多重模式：ADC1 恢复独立工作（DPS 等其它 APP 会重新初始化 ADC1）
    ADC->CCR &= ~(ADC_CCR_MULTI | ADC_CCR_DMA | ADC_CCR_DDS);
    __HAL_ADC_DISABLE(&s_dso_hadc2);                       // 从机 ADON 也一起关掉
    __HAL_ADC_DISABLE(&s_dso_hadc3);

    // 反初始化ADC、GPIO、DMA
    HAL_ADC_DeInit(&hadc1);
    __HAL_RCC_ADC1_CLK_DISABLE();
    __HAL_RCC_ADC2_CLK_DISABLE();
    __HAL_RCC_ADC3_CLK_DISABLE();
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
    HAL_ADCEx_MultiModeStop_DMA(&hadc1);                // 关闭ADC & DMA
    DSO_ADC_DeInit();

    OSC_Couple_Ctrl(Couple_AC);                         // 耦合方式：AC
    Div_Set(div_2v);                           // 最大化前级
    Suspend_IndevDetectTask();                          // 挂起输入设备检测任务
    osThreadSuspend(DsoCoreTaskHandle);                 // 挂起DSO核心任务
}

/* ================================================================================
 * 【临时】M1：三重交替采样的数据通路验证 —— 生成 0:/dump.bin 供上位机下载核对
 * -------------------------------------------------------------------------------
 * 先"单脉冲触发一次"数清一个序列里发生了多少次 DMA 传输（预期 3，即 ADC1/2/3 各一个），
 * 再整段采满 2048 个样本存盘，用来核对数据排布与各 ADC 的偏置差异。
 * 确认无误后把这一段（含 DSO_M1_ENABLE）整段删除即可。
 * ==============================================================================*/
#define DSO_M1_ENABLE  1
#define DSO_M1_FILE    "0:/dump.bin"
#define DSO_M1_LEN     2048U

#if DSO_M1_ENABLE
static uint8_t s_m1_done = 0;

typedef struct {
    uint32_t magic;      // 'M1D0'
    uint32_t moved;      // 一次 TIM8 触发内发生的 DMA 传输次数（预期 3）
    uint32_t ndtr;       // 整段采完后的剩余 DMA 计数
    uint32_t ccr;        // ADC->CCR 实测值
    uint32_t len;        // 后面跟的样本数
    uint32_t rsrv[3];
} m1_head_t;

static void dso_m1_dump(void) {
    m1_head_t head;
    ADC_MultiModeTypeDef mm = {0};
    FIL  fil;
    UINT bw = 0;

    memset(&head, 0, sizeof(head));

    __HAL_RCC_ADC2_CLK_ENABLE();
    __HAL_RCC_ADC3_CLK_ENABLE();
    dso_adc_config(&hadc1, ADC1, true);
    dso_adc_config(&s_dso_hadc2, ADC2, false);
    dso_adc_config(&s_dso_hadc3, ADC3, false);

    mm.Mode             = ADC_TRIPLEMODE_INTERL;
    mm.DMAAccessMode    = ADC_DMAACCESSMODE_1;
    mm.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_5CYCLES;
    if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &mm) != HAL_OK) {
        return;
    }

    // DMA 用普通模式：传满就停，方便数"一次触发传了几次"
    hdma_adc1.Instance                 = DMA2_Stream4;
    hdma_adc1.Init.Channel             = DMA_CHANNEL_0;
    hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_adc1.Init.Mode                = DMA_NORMAL;
    hdma_adc1.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_adc1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) {
        return;
    }
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

    __HAL_TIM_SET_PRESCALER(&htim8, 0);
    __HAL_TIM_SET_AUTORELOAD(&htim8, 59);          // 每 ADC 2.4 MSa/s → 合成 7.2 MSa/s
    __HAL_TIM_SET_COUNTER(&htim8, 0);

    /* ① 单脉冲：只发一次 TRGO */
    memset(DSO_ADC_BUFFER, 0, sizeof(DSO_ADC_BUFFER));
    if (HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t *)DSO_ADC_BUFFER, DSO_M1_LEN) == HAL_OK) {
        htim8.Instance->CR1 |= TIM_CR1_OPM;        // 单脉冲模式
        AdcTim_ON();
        for (volatile uint32_t i = 0; i < 40000U; i++) {
            __NOP();
        }
        head.moved = DSO_M1_LEN - __HAL_DMA_GET_COUNTER(&hdma_adc1);
        htim8.Instance->CR1 &= ~TIM_CR1_OPM;
        AdcTim_OFF();
        HAL_ADCEx_MultiModeStop_DMA(&hadc1);
    } else {
        head.moved = 0xFFFFFFFFU;
    }

    /* ② 连续采一段，把样本填满 */
    memset(DSO_ADC_BUFFER, 0, sizeof(DSO_ADC_BUFFER));
    if (HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t *)DSO_ADC_BUFFER, DSO_M1_LEN) == HAL_OK) {
        AdcTim_ON();
        uint32_t t0 = HAL_GetTick();
        while ((__HAL_DMA_GET_COUNTER(&hdma_adc1) > 0U) && ((HAL_GetTick() - t0) < 100U)) {
        }
        head.ndtr = __HAL_DMA_GET_COUNTER(&hdma_adc1);
        AdcTim_OFF();
        HAL_ADCEx_MultiModeStop_DMA(&hadc1);
    } else {
        head.ndtr = 0xFFFFFFFFU;
    }

    head.magic = 0x3044314DU;                      // "M1D0"
    head.ccr   = ADC->CCR;
    head.len   = DSO_M1_LEN;

    if (f_open(&fil, DSO_M1_FILE, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
        (void)f_write(&fil, &head, sizeof(head), &bw);
        (void)f_write(&fil, DSO_ADC_BUFFER, DSO_M1_LEN, &bw);
        (void)f_close(&fil);
    }
}
#else
static uint8_t s_m1_done = 1;
#endif

void Resume_DsoCoreTask(void) {
#if DSO_M1_ENABLE
    if (!s_m1_done) {
        s_m1_done = 1;
        dso_m1_dump();      // 【临时】生成 0:/dump.bin，用于核对三重交替的数据排布
    }
#endif
    DSO_ADC_SetMulti(true);        // 正常使用时都用三重交替（校准流程会临时关掉）

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
    // 字符串缓冲区清零（含影子缓存：清零后第一次刷新必定重画）
    memset(Freq_buf, 0, sizeof(Freq_buf));
    memset(Duty_buf, 0, sizeof(Duty_buf));
    memset(Vpp_buf,  0, sizeof(Vpp_buf));
    memset(Freq_buf_last, 0, sizeof(Freq_buf_last));
    memset(Duty_buf_last, 0, sizeof(Duty_buf_last));
    memset(Vpp_buf_last,  0, sizeof(Vpp_buf_last));
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
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_COMPLETE_CB_ID, CB_DSO_ADC_ConvCplt);
    HAL_TIM_RegisterCallback(&htim3, HAL_TIM_IC_CAPTURE_CB_ID, CB_Trigger_CaptureCallback);
    HAL_TIM_RegisterCallback(&htim11, HAL_TIM_PERIOD_ELAPSED_CB_ID, CB_DSO_TIM11_UpdateCallback);

    // 配置定时器参数
    __HAL_TIM_SET_PRESCALER(&htim8, time_base[TimeBase].trigger_timer_psc);
    __HAL_TIM_SET_AUTORELOAD(&htim8, time_base[TimeBase].trigger_timer_arr);
    __HAL_TIM_SET_PRESCALER(&htim11, time_base[TimeBase].refresh_timer_psc);
    __HAL_TIM_SET_AUTORELOAD(&htim11, time_base[TimeBase].refresh_timer_arr);

    // 启动ADC DMA采样（失败时置故障标记，由主循环看门狗重试）
    capture_fault = false;
    if (!Dso_StartCapture()) {
        capture_fault = true;
    }

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

// 按键事件分发
static void (*page_handlers[DSO_PAGE_NUM])(KeyEventMsg_t) = {
    [DSO_PAGE_MAIN]    = dso_main_page_handler,
    [DSO_PAGE_SETTING] = dso_setting_page_handler,
};

// 主界面响应逻辑
static void dso_main_page_handler(KeyEventMsg_t msg) {
    // 如果set键单击，则在暂停/继续中切换
    if (msg.key == KEY_SET && msg.event == KEY_EVENT_CLICK) {
        Is_Pause = !Is_Pause;
        lcd_draw_rect(130,0,190,17,0x1082,1);
        if (!Is_Pause) {
            lcd_draw_string(141, 0, "RUN", &JetBrainsMono14x18, 0x07c4, 0x1082, -2);
            Dso_ResumeRunning();    // 暂停期间采集与触发都停了，恢复时必须重新武装
        }
        else {
            lcd_draw_string(135,0,"STOP",&JetBrainsMono14x18,0xf000,0x1082,-2);
        }
    }
    // 如果mode键单击，在“左右平移”“上下平移”“触发调节”之间切换
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
    // 如果编码器单击，则归零“左右平移”“上下平移”“触发调节”的值
    else if (msg.key == KEY_ENCODER && msg.event == KEY_EVENT_CLICK) {
        offset_h = 0;
        offset_w = 0;
        offset_v = 0;
        if (Is_Pause){Refresh_DSO_Waveform(DSO_SAMPLE_SHOW_BUFFER, offset_h, offset_w);}
        lcd_draw_rect(0,25,8,210,0x0000,1);
        lcd_draw_IsoscelesTriangle(7, 119, Triangle_RIGHT, 6, 0x057f);
        TriggerVoltage_Set(0);
    }
    // 如果左转旋转编码器 或 下键单击，减少“左右平移”/“上下平移”/“触发调节”的值
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
    // 如果右转旋转编码器 或 上建单击，增加“左右平移”/“上下平移”/“触发调节”的值
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
    // 如果mode键长按，则进入设置界面
    else if (msg.key == KEY_MODE && msg.event == KEY_EVENT_LONG_PRESS) {
        dso_current_page = DSO_PAGE_SETTING;
        dso_current_setting = Setting_TimeBase;
        dso_past_setting = -1;
        lcd_draw_rect(7,2,57,16,0x1082,1);
        lcd_draw_string(7, 2, time_base[TimeBase].TimeBase_msg, &JetBrainsMono10x14, 0xf3c0, 0x1082, 0);
    }
    // 如果长按编码器，返回LVGL界面
    else if (msg.key == KEY_ENCODER && msg.event == KEY_EVENT_LONG_PRESS) {
        AppListType APP = APP_LVGL;
        StartBeezer(0);
        osMessageQueuePut(AppSwitchQueueHandle, &APP, 0, 10);
        return;
    }
    else return;
    StartBeezer(0);
}

// 设置界面响应逻辑
static void dso_setting_page_handler(KeyEventMsg_t msg) {
    // 左右旋转编码器，在参数中选择
    if (msg.event == ENCODER_EVENT_LEFT) {
        dso_past_setting = dso_current_setting;
        dso_current_setting = (dso_current_setting == 0) ? Setting_CoupleMode : (dso_current_setting - 1);
    }
    else if (msg.event == ENCODER_EVENT_RIGHT) {
        dso_past_setting = dso_current_setting;
        dso_current_setting = (dso_current_setting == Setting_CoupleMode) ? Setting_TimeBase : (dso_current_setting + 1);
    }
    // 长按mode键，回到主界面
    else if (msg.key == KEY_MODE && msg.event == KEY_EVENT_LONG_PRESS) {
        dso_current_page = DSO_PAGE_MAIN;
        dso_past_setting = dso_current_setting;
    }
    // 单击上建，增加选中项目的值
    else if (msg.key == KEY_UP ) {
        switch (dso_current_setting) {
            case Setting_TimeBase:
                TimeBase = (TimeBase < time_100ms) ? (TimeBase + 1) : TimeBase;     // 增加时基
                TimeBase_Set(TimeBase);
                break;
            case Setting_VoltageDiv:
                V_div = (V_div < div_2v) ? (V_div + 1) : V_div;
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
                CoupleType = Couple_DC;                 // 上键选直流（原来上下键都是取反，动作重复）
                OSC_Couple_Ctrl(CoupleType);
                break;
        }
    }
    // 单击下键，减少选中项目的值
    else if (msg.key == KEY_DOWN) {
        switch (dso_current_setting) {
            case Setting_TimeBase:
                TimeBase = (TimeBase > time_5us) ? (TimeBase - 1) : TimeBase;     // 递减时基
                TimeBase_Set(TimeBase);
                break;
            case Setting_VoltageDiv:
                V_div = (V_div > div_10mv) ? (V_div - 1) : V_div;
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
                CoupleType = Couple_AC;                 // 下键选交流
                OSC_Couple_Ctrl(CoupleType);
                break;
        }
    }
    // set键单击，暂停/继续采集波形
    else if (msg.key == KEY_SET && msg.event == KEY_EVENT_CLICK) {
        Is_Pause = !Is_Pause;
        lcd_draw_rect(130,0,190,17,0x1082,1);
        if (!Is_Pause) {
            lcd_draw_string(141, 0, "RUN", &JetBrainsMono14x18, 0x07c4, 0x1082, -2);
            Dso_ResumeRunning();    // 暂停期间采集与触发都停了，恢复时必须重新武装
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