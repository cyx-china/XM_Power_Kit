/**
******************************************************************************
  * @file           : DpsSafety.c
  * @brief          : 数控电源的断电保护（OVP / OCP / SCP）判定实现
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * 判据都要连续成立一段时间才认，计时用系统节拍（1 tick = 1ms），
  * 不依赖调用频率，PID 任务偶发被抢占也不会把防抖时间算短。
  ******************************************************************************
  */

#include "DpsSafety.h"

#include <math.h>

#include "cmsis_os2.h"

/* ========================== 内部状态 ========================== */
static DpsFault_t s_fault = DPS_FAULT_NONE;     /* 锁存的故障码，只有 ClearFault/Reset 能清 */

static uint32_t s_arm_tick;                     /* OVP/OCP 静默期结束时刻 */
static uint32_t s_scp_arm_tick;                 /* SCP 静默期结束时刻 */

static bool  s_has_last;                        /* 是否已有目标值快照 */
static float s_last_target_v;
static float s_last_target_c;

static bool     s_ovp_active;
static uint32_t s_ovp_since;
static bool     s_ocp_active;
static uint32_t s_ocp_since;
static bool     s_scp_active;
static uint32_t s_scp_since;

/* 参考电平跟随：下调设定电压后输出靠负载/漏电慢慢回落，
   这期间只允许电压保持在"过程最低电平"附近，不允许再往上爬。 */
static bool  s_settling;
static float s_v_trough;                        /* 进入回落态以来的最低实测电压 */

static bool  s_scan_mode;                       /* 诊断扫描期间彻底关闭保护 */

/* ========================== 内部辅助 ========================== */

/* 清空三路计时（不改静默期、不改回落态、不改故障锁存） */
static void counters_clear(void)
{
    s_ovp_active = false;
    s_ocp_active = false;
    s_scp_active = false;
    s_ovp_since  = 0U;
    s_ocp_since  = 0U;
    s_scp_since  = 0U;
}

/* 目标值变化超过一个 DAC 台阶就重启静默期：改设定瞬间实测会短暂"高于新目标" */
static void rearm(uint32_t now)
{
    counters_clear();
    s_arm_tick     = now + DPS_ARM_MS;
    s_scp_arm_tick = now + DPS_SCP_ARM_MS;
}

/* 条件连续成立计时：条件消失立即清零，重新成立重新开始计时 */
static void cond_track(bool cond, uint32_t now, bool *active, uint32_t *since)
{
    if (!cond) {
        *active = false;
        return;
    }
    if (!*active) {
        *active = true;
        *since  = now;
    }
}

/* 绝对余量与相对余量取大者 */
static float margin_of(float base, float ratio, float absolute)
{
    float value = base * ratio;

    return (value > absolute) ? value : absolute;
}

/* ========================== 对外接口 ========================== */

void DpsSafety_SetScanMode(bool on)
{
    s_scan_mode = on;
    if (on) {                       /* 扫描期间不判任何保护，也不能残留计时 */
        counters_clear();
        s_settling = false;
    }
}

void DpsSafety_Disarm(void)
{
    counters_clear();
    s_settling = false;
    s_has_last = false;             /* 下次开输出重新取目标快照并进入静默期 */
}

void DpsSafety_Reset(void)
{
    uint32_t now = osKernelGetTickCount();

    s_fault    = DPS_FAULT_NONE;
    s_has_last = false;
    s_settling = false;
    rearm(now);
}

void DpsSafety_ClearFault(void)
{
    DpsSafety_Reset();
}

DpsFault_t DpsSafety_GetFault(void)
{
    return s_fault;
}

const char *DpsSafety_CodeText(DpsFault_t fault)
{
    switch (fault) {
        case DPS_FAULT_OVP: return "OVP";
        case DPS_FAULT_OCP: return "OCP";
        case DPS_FAULT_SCP: return "SCP";
        default:            return "---";
    }
}

DpsFault_t DpsSafety_Update(float voltage, float current,
                            float target_voltage, float target_current,
                            bool cc_mode, bool output_on)
{
    uint32_t now;
    float    normal_margin;
    float    normal_limit;
    float    limit;

    if (!output_on) {                       /* 输出关着：清计时、保留故障锁存 */
        DpsSafety_Disarm();
        return s_fault;
    }
    if (s_scan_mode) {                      /* 诊断扫描：保护全部让位 */
        counters_clear();
        return s_fault;
    }
    if (s_fault != DPS_FAULT_NONE) {         /* 已跳闸：保持锁存，等用户确认 */
        return s_fault;
    }

    now = osKernelGetTickCount();

    normal_margin = margin_of(target_voltage, DPS_OVP_RATIO_MARGIN, DPS_OVP_ABS_MARGIN);
    normal_limit  = target_voltage + normal_margin;

    /* 目标值被改动：重新进入静默期，防止"把电压调低"被当成过压 */
    if ((!s_has_last) ||
        (fabsf(target_voltage - s_last_target_v) > 0.05f) ||
        (fabsf(target_current - s_last_target_c) > 0.02f)) {
        /* 只有"新目标低于当前实测电压"才需要回落态：输出电容没有泄放电阻，
           轻载/空载下电压只能靠漏电慢慢掉，这期间物理上做不到立即到达目标。
           抬升设定时电压就在目标以下，不进入回落态。 */
        if (voltage > normal_limit) {
            if (!s_settling) {
                s_settling  = true;
                s_v_trough  = voltage;      /* 以进入时的电平为基准 */
            }
            /* 已在回落态里再次下调：不抬高基准，继续跟随已有的最低电平 */
        } else {
            s_settling = false;
        }

        s_has_last      = true;
        s_last_target_v = target_voltage;
        s_last_target_c = target_current;
        rearm(now);
        return DPS_FAULT_NONE;
    }

    if ((int32_t) (now - s_arm_tick) < 0) {  /* 刚开输出：DAC 落位、输出电容充电过程中不判 */
        return DPS_FAULT_NONE;
    }

    /* ---------- 回落态：基准跟着电压一路往下走 ---------- */
    if (s_settling) {
        if (voltage < s_v_trough) {
            s_v_trough = voltage;
        }
        /* 落回正常阈值以内（留一半余量的缓冲）就退出回落态，恢复以目标为基准 */
        if (voltage <= (target_voltage + normal_margin * 0.5f)) {
            s_settling = false;
        }
    }

    /* ---------- OVP：电压超过限值持续 30ms ---------- */
    limit = normal_limit;
    if (s_settling) {
        float rise         = margin_of(s_v_trough, DPS_OVP_SETTLE_RISE_RATIO, DPS_OVP_SETTLE_RISE_ABS);
        float settle_limit = s_v_trough + rise;

        if (settle_limit > limit) {          /* 回落期间只允许电压在"最低点 + 回升余量"以内 */
            limit = settle_limit;
        }
    }

    cond_track(voltage > limit, now, &s_ovp_active, &s_ovp_since);
    if (s_ovp_active && ((uint32_t) (now - s_ovp_since) >= DPS_OVP_DELAY_MS)) {
        s_fault = DPS_FAULT_OVP;
        return s_fault;
    }

    /* ---------- OCP：电流超过目标 + max(0.3A, 10%) 持续 100ms ---------- */
    limit = target_current + margin_of(target_current, DPS_OCP_RATIO_MARGIN, DPS_OCP_ABS_MARGIN);
    cond_track(current > limit, now, &s_ocp_active, &s_ocp_since);
    if (s_ocp_active && ((uint32_t) (now - s_ocp_since) >= DPS_OCP_DELAY_MS)) {
        s_fault = DPS_FAULT_OCP;
        return s_fault;
    }

    /* ---------- SCP：限流状态 + 电压 < 0.9V 持续 300ms ---------- */
    if ((int32_t) (now - s_scp_arm_tick) >= 0) {
        cond_track(cc_mode && (voltage < DPS_SCP_VOLTAGE_V), now, &s_scp_active, &s_scp_since);
        if (s_scp_active && ((uint32_t) (now - s_scp_since) >= DPS_SCP_DELAY_MS)) {
            s_fault = DPS_FAULT_SCP;
            return s_fault;
        }
    }

    return DPS_FAULT_NONE;
}
