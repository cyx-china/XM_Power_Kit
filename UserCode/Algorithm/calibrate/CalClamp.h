/**
 * @file    CalClamp.h
 * @brief   校准参数的步进限幅
 *
 * 校准页的所有编辑都是"按一下 ±1 / ±0.01"的累加，原实现没有任何上下限：
 * 编码器卡住、连按、或者误触都会把参数推到物理上不可能的值（例如系数变成
 * 负数或 0、零点推到 3000），一旦保存，下次上电就完全没法用。
 *
 * 这里只做"明显不合理"的兜底，范围留得足够宽，正常校准碰不到边界：
 *   零点 / 偏置类（int16，单位 LSB）：±2000
 *   系数类（float）：0.1 ~ 200
 */
#ifndef XM_POWER_KIT_CALCLAMP_H
#define XM_POWER_KIT_CALCLAMP_H

#include <stdint.h>

#define CAL_OFFSET_MIN   (-2000)
#define CAL_OFFSET_MAX   (2000)
#define CAL_FACTOR_MIN   (0.1f)
#define CAL_FACTOR_MAX   (200.0f)

/* 零点/偏置：先按 int32 计算再限幅，避免 int16 在边界处回绕 */
static inline int16_t cal_clamp_i16(int32_t value, int32_t min, int32_t max)
{
    if (value < min) return (int16_t) min;
    if (value > max) return (int16_t) max;
    return (int16_t) value;
}

static inline float cal_clamp_f(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

#endif /* XM_POWER_KIT_CALCLAMP_H */
