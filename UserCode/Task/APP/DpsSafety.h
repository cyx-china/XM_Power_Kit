/**
******************************************************************************
  * @file           : DpsSafety.h
  * @brief          : 数控电源的断电保护（OVP 过压 / OCP 过流 / SCP 短路）
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * 这一层只负责"判定"，不做任何界面与 I2C 动作：
  *   由 1ms 的 PID 任务周期调用 DpsSafety_Update()，返回非 NONE 时由调用方切断输出。
  *
  * 阈值全部写死（不占用户参数），三档判据都带毫秒级防抖，避免负载阶跃误触发：
  *   OVP: 实测电压 > 目标 + max(0.4V, 8%)，持续 30ms
  *   OCP: 实测电流 > 目标 + max(0.3A, 10%)，持续 100ms
  *   SCP: 处于恒流（限流）状态且实测电压 < 0.9V，持续 300ms
  *
  * 硬阈值必须高于控制层 PID 的过压回退余量 max(0.2V, 5%)，否则正常回调就会被误切断。
  ******************************************************************************
  */

#ifndef XM_POWER_KIT_DPS_SAFETY_H
#define XM_POWER_KIT_DPS_SAFETY_H

#include <stdbool.h>
#include <stdint.h>

/* ========================== 保护阈值 ========================== */
#define DPS_OVP_ABS_MARGIN      0.4f        /* 过压绝对余量（V） */
#define DPS_OVP_RATIO_MARGIN    0.08f       /* 过压相对余量（目标电压的 8%） */
#define DPS_OVP_DELAY_MS        30U         /* 过压持续时间 */
#define DPS_OCP_ABS_MARGIN      0.3f        /* 过流绝对余量（A） */
#define DPS_OCP_RATIO_MARGIN    0.10f       /* 过流相对余量（目标电流的 10%） */
#define DPS_OCP_DELAY_MS        100U        /* 过流持续时间 */
#define DPS_SCP_VOLTAGE_V       0.9f        /* 短路判据的电压门限（V） */
#define DPS_SCP_DELAY_MS        300U        /* 短路持续时间 */

#define DPS_ARM_MS              300U        /* 开输出/改设定后的静默期（OVP/OCP） */
#define DPS_SCP_ARM_MS          800U        /* 短路保护静默期更长：容性负载上电充电 */
#define DPS_OVP_SETTLE_MAX_MS   2000U       /* 下调设定电压后等输出回落的最长时间（见 DpsSafety.c） */

/* ========================== 类型 ========================== */
typedef enum {
    DPS_FAULT_NONE = 0,
    DPS_FAULT_OVP,
    DPS_FAULT_OCP,
    DPS_FAULT_SCP
} DpsFault_t;

/* ========================== 接口 ========================== */
void DpsSafety_Reset(void);                 /* 清故障锁存 + 进入静默期（开输出/进入 APP 时调用） */
void DpsSafety_Disarm(void);                /* 关输出：清计时与目标快照，保留故障锁存 */
void DpsSafety_ClearFault(void);            /* 用户确认后清故障（等同于 Reset） */

/**
 * @brief  每 1ms 调用一次的保护判定
 * @param  voltage:        实测输出电压（V，用 PID 滤波后的快速值）
 * @param  current:        实测输出电流（A，用 PID 滤波后的快速值）
 * @param  target_voltage: 目标电压（V）
 * @param  target_current: 目标电流（A）
 * @param  cc_mode:        当前是否处于恒流（限流）模式
 * @param  output_on:      输出是否开启；false 时自动 Disarm
 * @retval 当前锁存的故障码（NONE 表示正常）
 */
DpsFault_t DpsSafety_Update(float voltage, float current,
                            float target_voltage, float target_current,
                            bool cc_mode, bool output_on);

DpsFault_t DpsSafety_GetFault(void);        /* 读取锁存的故障码 */
const char *DpsSafety_CodeText(DpsFault_t fault);   /* "OVP"/"OCP"/"SCP"/"---" */

#endif /* XM_POWER_KIT_DPS_SAFETY_H */
