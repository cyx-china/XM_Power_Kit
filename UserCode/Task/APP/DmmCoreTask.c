#include <stdio.h>
#include <stdlib.h>

#include "adc.h"
#include "lcd_draw_api.h"
#include "stm32f4xx_hal.h"
#include "UserTask.h"
#include "os_handles.h"
#include "SwitchManager.h"
#include "tim.h"
#include "UserDefineManage.h"

static void DrawDmmMainBasicElement(void);
volatile uint16_t dmm_adc_raw_buf[600] = {0};   // 三个通道，每个通道200个点

volatile float DMM_Voltage = 0.0f;                  // 电压表_实际值
volatile float DMM_Current = 0.0f;                  // 电流表_实际值
volatile float DMM_Resistance_Voltage = 0.0f;       // 电阻表_原始电压值

char msg1[12] = {0};
char msg2[12] = {0};
char msg3[12] = {0};

void Start_DmmCoreTask(void *argument) {
    // 自挂启
    osThreadSuspend(DmmCoreTaskHandle);
    DrawDmmMainBasicElement();

    for (;;) {

            // sprintf(msg1, "%.5fV", DMM_Voltage);
            // lcd_draw_string(10, 100, msg1,&yahei16x16,0x0000, 0xFFFF,0);
            //
            // sprintf(msg2, "%.5fA", DMM_Current);
            // lcd_draw_string(10, 130, msg2,&yahei16x16, 0x0000, 0xFFFF,0);
            //
            // sprintf(msg3, "%.5fV", DMM_Resistance_Voltage);
            // lcd_draw_string(10, 150, msg3, &yahei16x16,0x0000, 0xFFFF,0);

        osDelay(125);

    }
}



static void DrawDmmMainBasicElement(void){
    //背景
    lcd_draw_rect(0, 0, 319, 31, 0x1908, 1);             // 顶栏背景色
    lcd_draw_rect(0, 31, 319, 239, 0x18c6, 1);           // 主体背景色
    lcd_draw_round_rect(12,66,308,154,8,0x1908,1);     // 数据区背景
    for (uint8_t i = 0;i < 4;i ++) {
        lcd_draw_round_rect(12 + 76 * i,166,80 + 76 * i,209,8,0x1908,1);
        lcd_draw_round_rect(12 + 76 * i,166,80 + 76 * i,209,8,0x21aa,0);
    }

    // 顶栏
    lcd_draw_string(127, 6, "万用表", &yahei20x20, 0x24be, 0x1908, 3);
    lcd_draw_line(0, 31, 319, 31, 0x11ac);                      // 顶栏下划线

    // body




}



















/**
 * @brief  DMM系统ADC初始化函数
 * @note   配置ADC1采集通道2/1/0（对应GPIOA2/GPIOA1/GPIOA0），用于模拟量采集
 *         - 触发源：定时器8 TRGO事件上升沿
 *         - 采集模式：扫描模式+DMA环形模式，三通道连续采集
 *         - 分辨率：12位，采样时间480周期（保证采集精度）
 * @retval 无
 */
void DMM_ADC_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};           // ADC通道配置结构体初始化
    GPIO_InitTypeDef GPIO_InitStruct = {0};         // GPIO配置结构体初始化

    // -------------------------- ADC1 基础配置 --------------------------
    hadc1.Instance = ADC1;                                              // 选择ADC1外设
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;               // ADC时钟分频：PCLK/2
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;                         // 12位分辨率
    hadc1.Init.ScanConvMode = ENABLE;                                   // 使能扫描模式（支持多通道采集）
    hadc1.Init.ContinuousConvMode = DISABLE;                            // 关闭连续转换（由外部触发控制）
    hadc1.Init.DiscontinuousConvMode = DISABLE;                         // 关闭间断模式
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;  // 外部触发边沿：上升沿
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T8_TRGO;         // 触发源：定时器8 TRGO事件
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;                         // 数据右对齐
    hadc1.Init.NbrOfConversion = 3;                                     // 转换通道数：3个
    hadc1.Init.DMAContinuousRequests = ENABLE;                          // 使能DMA连续请求（环形模式必需）
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;                         // 序列转换完成后触发EOC中断
    if (HAL_ADC_Init(&hadc1) != HAL_OK)                                 // ADC初始化
    {
        Error_Handler();
    }

    // -------------------------- ADC通道配置 --------------------------
    // 通道2（GPIOA2）：优先级1
    sConfig.Channel = ADC_CHANNEL_2;                            // 选择通道2
    sConfig.Rank = 1;                                           // 优先级1
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;            // 采样时间480周期
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    // 通道1（GPIOA1）：优先级2
    sConfig.Channel = ADC_CHANNEL_1;                            // 选择通道1
    sConfig.Rank = 2;                                           // 优先级2
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;            // 采样时间480周期（保持与上一通道一致）
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    // 通道0（GPIOA0）：优先级3
    sConfig.Channel = ADC_CHANNEL_0;                            // 选择通道0
    sConfig.Rank = 3;                                           // 优先级3
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;            // 采样时间480周期
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    // -------------------------- GPIO 初始化 --------------------------
    __HAL_RCC_ADC1_CLK_ENABLE();                                // 使能ADC1时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();                               // 使能GPIOA时钟
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2; // ADC通道0/1/2对应引脚
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;                    // 模拟输入模式
    GPIO_InitStruct.Pull = GPIO_NOPULL;                         // 无上下拉
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // -------------------------- DMA 配置 --------------------------
    hdma_adc1.Instance = DMA2_Stream4;                              // DMA2流4（ADC1默认DMA通道）
    hdma_adc1.Init.Channel = DMA_CHANNEL_0;                         // DMA通道0
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;                // 数据方向：外设到内存
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;                    // 外设地址不递增
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;                        // 内存地址递增
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;   // 外设数据对齐：半字
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;      // 内存数据对齐：半字
    hdma_adc1.Init.Mode = DMA_CIRCULAR;                             // 环形模式
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;                    // 高优先级
    hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;                 // 关闭FIFO模式
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) {
        Error_Handler();
    }

    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);   // 关联ADC1和DMA句柄
}

/**
 * @brief  DMM系统ADC反初始化函数
 * @note   安全停止ADC采集，释放ADC、GPIO、DMA相关资源
 * @retval 无
 */
void DMM_ADC_DeInit(void) {
    HAL_ADC_Stop(&hadc1);                               // 停止ADC转换
    HAL_ADC_Stop_DMA(&hadc1);                           // 停止ADC DMA传输
    // 等待DMA传输完成（超时100ms），避免数据传输中断导致异常
    HAL_DMA_PollForTransfer(&hdma_adc1, HAL_DMA_FULL_TRANSFER, 100);

    HAL_ADC_DeInit(&hadc1);                                     // ADC外设反初始化
    __HAL_RCC_ADC1_CLK_DISABLE();                               // 关闭ADC1时钟
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2); // 释放ADC对应GPIO引脚
    HAL_DMA_DeInit(&hdma_adc1);                                 // DMA反初始化
}



// ================= IIR滤波相关变量 =================
#define IIR_ALPHA_VOLTAGE    0.08f         // 电压滤波系数（0~1，越小越平滑）
#define IIR_ALPHA_CURRENT    0.1f          // 电流滤波系数
#define IIR_ALPHA_RESISTANCE 0.1f          // 电阻滤波系数
static float iir_prev_voltage = 0.0f;       // 电压上一次滤波值
static float iir_prev_current = 0.0f;       // 电流上一次滤波值
static float iir_prev_resistance = 0.0f;    // 电阻上一次滤波值


static float iir_filter(float current_val, float prev_val, float alpha) {
    return alpha * current_val + (1.0f - alpha) * prev_val;
}

// 比较函数
int compare_uint16_ternary(const void *a, const void *b) {
    const uint16_t num1 = *(const uint16_t *)a;
    const uint16_t num2 = *(const uint16_t *)b;

    return (num1 < num2) ? -1 : (num1 > num2) ? 1 : 0;
}


/**
 * @brief  万用表ADC转换完成回调函数
 * @note   无
 * @retval 无
 */
void CB_DMM_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        AdcTim_OFF();                                   // 关闭ADC触发
        // 解交错
        uint16_t ch1_data[200];
        uint16_t ch2_data[200];
        uint16_t ch3_data[200];
        int idx = 0;
        for(int i = 0; i < 600; i += 3) {
            ch1_data[idx] = dmm_adc_raw_buf[i];          // 电压数据
            ch2_data[idx] = dmm_adc_raw_buf[i+1];        // 电流数据
            ch3_data[idx] = dmm_adc_raw_buf[i+2];        // 电阻数据
            idx++;
        }
        // 排序
        qsort(ch1_data, 200, sizeof(uint16_t), compare_uint16_ternary);
        qsort(ch2_data, 200, sizeof(uint16_t), compare_uint16_ternary);
        qsort(ch3_data, 200, sizeof(uint16_t), compare_uint16_ternary);
        // 加和
        uint32_t ch1_sum = 0, ch2_sum = 0, ch3_sum = 0;
        for (uint8_t i = 4;i < 196; i++) {          // 去掉前4个和后4个毛刺数据
            ch1_sum += ch1_data[i];
            ch2_sum += ch2_data[i];
            ch3_sum += ch3_data[i];
        }
        // 过采样到14bit + 均值滤波
        uint16_t ch1_14bit = 0, ch2_14bit = 0, ch3_14bit = 0;
        ch1_14bit = (ch1_sum >> 2) / 12;
        ch2_14bit = (ch2_sum >> 2) / 12;
        ch3_14bit = (ch3_sum >> 2) / 12;

        // 更新数据
        // 电压
        int32_t temp1 = (int32_t)ch1_14bit - (8192 - 17 ); //UserParam.DMM_Voltage_Original);
        float raw_voltage = (float) temp1 * (3.3f / 16383.0f) * (temp1 >= 0
                                                               ? UserParam.DMM_Voltage_Factor_B
                                                               : UserParam.DMM_Voltage_Factor_R);
        int32_t temp2 = (int32_t)ch2_14bit - (8192 + UserParam.DMM_Current_Original);
        float raw_current = (float) temp2 * (3.3f / 16383.0f) * UserParam.DMM_Current_Factor;
        float raw_resistance_voltage = (float)ch3_14bit * (3.3f / 16383.0f);

        // ================= IIR滤波 =================
        DMM_Voltage = iir_filter(raw_voltage, iir_prev_voltage, IIR_ALPHA_VOLTAGE);
        DMM_Current = iir_filter(raw_current, iir_prev_current, IIR_ALPHA_CURRENT);
        DMM_Resistance_Voltage = iir_filter(raw_resistance_voltage, iir_prev_resistance, IIR_ALPHA_RESISTANCE);

        // 更新上一次的滤波值
        iir_prev_voltage = DMM_Voltage;
        iir_prev_current = DMM_Current;
        iir_prev_resistance = DMM_Resistance_Voltage;

        AdcTim_ON();
    }
}


void Resume_DmmTask(void) {
    DMM_ADC_Init();
    osDelay(100);
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_COMPLETE_CB_ID, CB_DMM_ADC_ConvCpltCallback);
    AdcTim_OFF();

    HAL_ADC_Start_DMA(&hadc1, (uint32_t *) dmm_adc_raw_buf, 600);
    __HAL_TIM_SET_PRESCALER(&htim8, 0);                 // 20Khz触发
    __HAL_TIM_SET_AUTORELOAD(&htim8, 7200 - 1);         // 每s能有100次有效数据
    AdcTim_ON();


    HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_12);

    // HAL_GPIO_TogglePin(GPIOE,GPIO_PIN_2);
    // HAL_GPIO_TogglePin(GPIOE,GPIO_PIN_0);
    // HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_7);

    osThreadResume(DmmCoreTaskHandle);          // 恢复DMM核心任务
}




