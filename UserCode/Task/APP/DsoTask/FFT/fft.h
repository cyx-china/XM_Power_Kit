#pragma once
#include <stdint.h>

// ==================== 配置参数 ====================
#define RAW_BUF_SIZE    2881U   // 原始排序缓冲区长度（2881点，来自SortRawSamplesToBuffer）
#define FFT_N           1024U   // FFT运算点数（基2，中部截取，升级为1024点）

// ==================== 对外接口函数 ====================
/**
 * @brief  从2881点原始排序数据中截取中部1024点做FFT，获取能量最大的交流分量频率
 * @param  raw_sorted_buf: 原始排序数据指针(uint8_t[2881])，必须是SortRawSamplesToBuffer的直接输出
 * @param  sample_rate: 采样率(Hz)，最大支持2.4MHz
 * @retval 主交流频率值(Hz)，浮点数（带二次插值精度优化）
 * @note   禁止传入经过峰值抽取、缩放、插值的显示数据（如DSO_SAMPLE_SHOW_BUFFER）
 */
float Get_Main_AC_Freq(const uint8_t *raw_sorted_buf, uint32_t sample_rate);