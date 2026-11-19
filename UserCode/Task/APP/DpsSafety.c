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

/* 下调设定电压后的"回落窗口"：见 Update() 里的说明 */
static bool     s_ovp_blocked;
static uint32_t s_ovp_block_until;

/* ========================== 内部辅助 ========================== */

/* 清空三路计时（不改静默期与故障锁存） */
static void counters_clear(void)
{
    s_ovp_active = false;
    s_ocp_active = false;
    s_scp_active = false;
    s_ovp_blocked = false;
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

void DpsSafety_Disarm(void)
{
    counters_clear();
    s_ovp_blocked = false;
    s_has_last = false;             /* 下次开输出重新取目标快照并进入静默期 */
}

void DpsSafety_Reset(void)
{
    uint32_t now = osKernelGetTickCount();

    s_fault    = DPS_FAULT_NONE;
    s_has_last = false;
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
    float    limit;

    if (!output_on) {                       /* 输出关着：清计时、保留故障锁存 */
        DpsSafety_Disarm();
        return s_fault;
    }
    if (s_fault != DPS_FAULT_NONE) {         /* 已跳闸：保持锁存，等用户确认 */
        return s_fault;
    }

    now = osKernelGetTickCount();

    /* 目标值被改动：重新进入静默期，防止"把电压调低"被当成过压 */
    if ((!s_has_last) ||
        (fabsf(target_voltage - s_last_target_v) > 0.05f) ||
        (fabsf(target_current - s_last_target_c) > 0.02f)) {
        /* 下调设定电压时，输出只能靠负载或泄放电阻把电容放掉，
           轻载/空载下几百毫秒都落不到新设定值——这段时间里不能判过压，
           但要封顶（2s），否则真的失控就再也保护不了。
           一旦实测电压回落到新阈值以内，立刻恢复正常判定。 */
        if (s_has_last && (target_voltage < (s_last_target_v - 0.05f))) {
            s_ovp_blocked     = true;
            s_ovp_block_until = now + DPS_OVP_SETTLE_MAX_MS;
        }
        s_has_last       = true;
        s_last_target_v  = target_voltage;
        s_last_target_c  = target_current;
        rearm(now);
        return DPS_FAULT_NONE;
    }

    if ((int32_t) (now - s_arm_tick) < 0) {  /* 刚开输出：DAC 落位、输出电容充电过程中不判 */
        return DPS_FAULT_NONE;
    }

    /* ---------- OVP：电压超过目标 + max(0.4V, 8%) 持续 30ms ---------- */
    limit = target_voltage + margin_of(target_voltage, DPS_OVP_RATIO_MARGIN, DPS_OVP_ABS_MARGIN);
    if (s_ovp_blocked && ((voltage <= limit) || ((int32_t) (now - s_ovp_block_until) >= 0))) {
        s_ovp_blocked = false;          /* 已经回落 或 超出回落窗口，恢复过压判定 */
    }
    if (!s_ovp_blocked) {
        cond_track(voltage > limit, now, &s_ovp_active, &s_ovp_since);
        if (s_ovp_active && ((uint32_t) (now - s_ovp_since) >= DPS_OVP_DELAY_MS)) {
            s_fault = DPS_FAULT_OVP;
            return s_fault;
        }
    } else {
        s_ovp_active = false;
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
