/**
 * @file    DpsGraph.h
 * @brief   DPS 曲线显示页：10 秒电压/电流/功率趋势图（采样缓冲 + 自动量程 + 整帧渲染）
 *
 * 设计说明：
 *   1. 采样由 DPS 主循环按固定节拍（约 38ms）推送，与本模块解耦，切换页面不清空缓冲；
 *   2. 一列像素对应一个采样点（266 列 = 266 点），曲线滚动时只整像素平移，
 *      不回采样、不插值，因此历史曲线的形状是固定的，只有坐标轴缩放会让整条曲线重新映射；
 *   3. 三个小图各自独立自动量程（1/2/5 × 10^n 取整），单位在 V/mV、A/mA、W/mW 间自适应；
 *   4. 渲染采用"整帧局部重绘"：把绘图区按 8 列一条分块填进静态缓冲，
 *      每块一次 lcd_draw_data() 发送，一帧 34 次 DMA，避免占用命令池。
 */
#ifndef XM_POWER_KIT_DPSGRAPH_H
#define XM_POWER_KIT_DPSGRAPH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 采样点数：与绘图区列数相同（一列一点，266 列 = 266 点），曲线滚动才是整像素平移 */
#define DPS_GRAPH_SAMPLE_NUM        266
/* 采样周期（毫秒）：266 × 38ms ≈ 10 秒（实际受主循环 10ms 节拍量化，会略长一点） */
#define DPS_GRAPH_SAMPLE_PERIOD_MS  38

/**
 * @brief  清空采样缓冲并使能刻度重绘（切换进 DPS APP 时调用）
 */
void DpsGraph_Reset(void);

/**
 * @brief  压入一个采样点（由 DPS 主循环按约 38ms 周期调用，一次一列像素）
 * @param  voltage 输出电压（V）
 * @param  current 输出电流（A）
 * @param  power   输出功率（W）
 */
void DpsGraph_Sample(float voltage, float current, float power);

/**
 * @brief  绘制曲线区静态底图（清空绘图区 + 刻度失效，随后由 DrawFrame 填充）
 */
void DpsGraph_DrawChrome(void);

/**
 * @brief  重绘一整帧曲线（含刻度标签）
 * @param  target_voltage 目标电压（电源开启时作为虚线参考并入量程）
 * @param  target_current 目标电流（同上）
 * @param  power_on       电源是否开启：0-关闭，1-开启
 */
void DpsGraph_DrawFrame(float target_voltage, float target_current, uint8_t power_on);

#ifdef __cplusplus
}
#endif

#endif /* XM_POWER_KIT_DPSGRAPH_H */
