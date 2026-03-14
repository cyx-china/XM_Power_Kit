/**
 ******************************************************************************
 * @file    wave_generator.h
 * @brief   波形发生器驱动头文件
 *          支持正弦波、方波、三角波、噪声等多种波形的DAC输出
 * @note    所有波形生成函数接口统一为 (freq, amplitude, bias, duty_cycle)
 ******************************************************************************
 */

#ifndef WAVE_GENERATOR_H
#define WAVE_GENERATOR_H

#include "stm32f4xx_hal.h"
#include <math.h>

// ------------------- 公共配置参数 -------------------
#define MAX_SAMPLE       256     // 最大采样点数 (2^7)，RAM占用 256 * 2 = 512 Bytes (12-bit aligned)
#define DAC_MAX          4095    // 12位DAC最大值 (2^12 - 1)
#define VREF             3.3f    // DAC参考电压 (V)
#define MAX_AMPLITUDE    4.95f   // 最大输出振幅 (V)，受后端运放轨到轨限制
#define MIN_AMPLITUDE    0.0f    // 最小输出振幅 (V)
#define MAX_BIAS         4.95f   // 最大输出偏置 (V)
#define MIN_BIAS         (-4.95f)// 最小输出偏置 (V)
#define TRIG_MAX_FREQ    1000000 // DAC最大触发频率 (Hz)
#define TIM_TRGO_HANDLE  htim7   // 用于触发DAC的定时器句柄 (TIM7 TRGO)
#define TIM_FREQ         72000000.0f // 定时器时钟频率 (Hz)，APB1 or APB2

// ------------------- 函数声明 -------------------

/**
 * @brief  生成正弦波 (Sine Wave)
 * @param  freq:       频率 (Hz, Range: 1 ~ 50000)
 * @param  amplitude:  振幅 (V, Range: 0 ~ MAX_AMPLITUDE)
 * @param  bias:       直流偏置 (V, Range: MIN_BIAS ~ MAX_BIAS)
 * @param  duty_cycle: 保留参数，未使用
 * @return HAL状态码
 */
HAL_StatusTypeDef WaveGen_Sin(float freq, float amplitude, float bias, float duty_cycle);

/**
 * @brief  生成矩形波/方波 (Square Wave)
 * @param  freq:       频率 (Hz, Range: 1 ~ 50000)
 * @param  amplitude:  振幅 (V, Range: 0 ~ MAX_AMPLITUDE)
 * @param  bias:       直流偏置 (V, Range: MIN_BIAS ~ MAX_BIAS)
 * @param  duty_cycle: 占空比 (Range: 0.0 ~ 1.0, e.g., 0.5 = 50%)
 * @return HAL状态码
 */
HAL_StatusTypeDef WaveGen_Square(float freq, float amplitude, float bias, float duty_cycle);

/**
 * @brief  生成锯齿波/三角波 (Sawtooth/Triangle Wave)
 * @param  freq:       频率 (Hz, Range: 1 ~ 50000)
 * @param  amplitude:  振幅 (V, Range: 0 ~ MAX_AMPLITUDE)
 * @param  bias:       直流偏置 (V, Range: MIN_BIAS ~ MAX_BIAS)
 * @param  duty_cycle: 占空比 (Range: 0.0 ~ 1.0). 0.0=上升锯齿, 0.5=三角波, 1.0=下降锯齿
 * @return HAL状态码
 */
HAL_StatusTypeDef WaveGen_Sawtooth(float freq, float amplitude, float bias, float duty_cycle);

/**
 * @brief  生成半波整流波形 (Half-Wave Rectified Sine)
 * @param  freq:       频率 (Hz, Range: 1 ~ 50000)
 * @param  amplitude:  振幅 (V, Range: 0 ~ MAX_AMPLITUDE)
 * @param  bias:       直流偏置 (V, Range: MIN_BIAS ~ MAX_BIAS)
 * @param  duty_cycle: 保留参数，未使用
 * @return HAL状态码
 */
HAL_StatusTypeDef WaveGen_HalfWaveRect(float freq, float amplitude, float bias, float duty_cycle);

/**
 * @brief  生成全波整流波形 (Full-Wave Rectified Sine / Absolute Sine)
 * @param  freq:       频率 (Hz, Range: 1 ~ 50000)
 * @param  amplitude:  振幅 (V, Range: 0 ~ MAX_AMPLITUDE)
 * @param  bias:       直流偏置 (V, Range: MIN_BIAS ~ MAX_BIAS)
 * @param  duty_cycle: 保留参数，未使用
 * @return HAL状态码
 */
HAL_StatusTypeDef WaveGen_FullWaveRect(float freq, float amplitude, float bias, float duty_cycle);

/**
 * @brief  生成8级正阶梯波 (8-Level Upward Staircase Wave)
 * @param  freq:       频率 (Hz, Range: 1 ~ 50000)
 * @param  amplitude:  振幅 (V, Range: 0 ~ MAX_AMPLITUDE)
 * @param  bias:       直流偏置 (V, Range: MIN_BIAS ~ MAX_BIAS)
 * @param  duty_cycle: 保留参数，未使用
 * @return HAL状态码
 */
HAL_StatusTypeDef WaveGen_UpStep(float freq, float amplitude, float bias, float duty_cycle);

/**
 * @brief  生成8级反阶梯波 (8-Level Downward Staircase Wave)
 * @param  freq:       频率 (Hz, Range: 1 ~ 50000)
 * @param  amplitude:  振幅 (V, Range: 0 ~ MAX_AMPLITUDE)
 * @param  bias:       直流偏置 (V, Range: MIN_BIAS ~ MAX_BIAS)
 * @param  duty_cycle: 保留参数，未使用
 * @return HAL状态码
 */
HAL_StatusTypeDef WaveGen_DownStep(float freq, float amplitude, float bias, float duty_cycle);

/**
 * @brief  生成指数上升波形 (Exponential Rise / Charge Curve)
 * @param  freq:       频率 (Hz, Range: 1 ~ 50000)
 * @param  amplitude:  振幅 (V, Range: 0 ~ MAX_AMPLITUDE)
 * @param  bias:       直流偏置 (V, Range: MIN_BIAS ~ MAX_BIAS)
 * @param  duty_cycle: 保留参数，未使用
 * @return HAL状态码
 */
HAL_StatusTypeDef WaveGen_ExpRise(float freq, float amplitude, float bias, float duty_cycle);

/**
 * @brief  生成指数下降波形 (Exponential Decay / Discharge Curve)
 * @param  freq:       频率 (Hz, Range: 1 ~ 50000)
 * @param  amplitude:  振幅 (V, Range: 0 ~ MAX_AMPLITUDE)
 * @param  bias:       直流偏置 (V, Range: MIN_BIAS ~ MAX_BIAS)
 * @param  duty_cycle: 保留参数，未使用
 * @return HAL状态码
 */
HAL_StatusTypeDef WaveGen_ExpDecay(float freq, float amplitude, float bias, float duty_cycle);

/**
 * @brief  生成直流信号 (DC Output)
 * @note   为了统一函数指针接口，保留了前两个无用参数
 * @param  freq:       保留参数，未使用
 * @param  amplitude:  保留参数，未使用
 * @param  bias:       直流输出电压 (V, Range: MIN_BIAS ~ MAX_BIAS)
 * @param  duty_cycle: 保留参数，未使用
 * @return HAL状态码
 */
HAL_StatusTypeDef WaveGen_DC(float freq, float amplitude, float bias, float duty_cycle);

/**
 * @brief  生成多音频信号 (Multi-tone / Harmonic Signal)
 * @note   包含基频、2倍频、3倍频成分 (1:0.5:0.3 幅度比)
 * @param  freq:       基波频率 (Hz, Range: 1 ~ 50000)
 * @param  amplitude:  总振幅 (V, Range: 0 ~ MAX_AMPLITUDE)
 * @param  bias:       直流偏置 (V, Range: MIN_BIAS ~ MAX_BIAS)
 * @param  duty_cycle: 保留参数，未使用
 * @return HAL状态码
 */
HAL_StatusTypeDef WaveGen_MultiTone(float freq, float amplitude, float bias, float duty_cycle);

/**
 * @brief  生成辛克脉冲 (Sinc Pulse / sin(x)/x)
 * @note   主瓣宽度由频率决定
 * @param  freq:       频率 (Hz, Range: 1 ~ 50000)
 * @param  amplitude:  振幅 (V, Range: 0 ~ MAX_AMPLITUDE)
 * @param  bias:       直流偏置 (V, Range: MIN_BIAS ~ MAX_BIAS)
 * @param  duty_cycle: 保留参数，未使用
 * @return HAL状态码
 */
HAL_StatusTypeDef WaveGen_Sinc(float freq, float amplitude, float bias, float duty_cycle);

/**
 * @brief  生成洛伦兹波形 (Lorenz / Cauchy Pulse)
 * @note   具有较宽的频谱和拖尾
 * @param  freq:       频率 (Hz, Range: 1 ~ 50000)
 * @param  amplitude:  振幅 (V, Range: 0 ~ MAX_AMPLITUDE)
 * @param  bias:       直流偏置 (V, Range: MIN_BIAS ~ MAX_BIAS)
 * @param  duty_cycle: 保留参数，未使用
 * @return HAL状态码
 */
HAL_StatusTypeDef WaveGen_Lorenz(float freq, float amplitude, float bias, float duty_cycle);

/**
 * @brief  生成高斯白噪声 (White Noise / Random Noise)
 * @note   使用伪随机数生成器 (PRNG)
 * @param  freq:       保留参数，未使用 (噪声带宽由采样率决定)
 * @param  amplitude:  噪声幅度 (V, Range: 0 ~ MAX_AMPLITUDE, 峰峰值)
 * @param  bias:       直流偏置 (V, Range: MIN_BIAS ~ MAX_BIAS)
 * @param  duty_cycle: 保留参数，未使用
 * @return HAL状态码
 */
HAL_StatusTypeDef WaveGen_Noise(float freq, float amplitude, float bias, float duty_cycle);

void WaveGen_Stop(void);

#endif