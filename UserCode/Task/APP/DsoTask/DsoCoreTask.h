#ifndef XM_POWER_KIT_DSOCORETASK_H
#define XM_POWER_KIT_DSOCORETASK_H

#include <stdint.h>
#include "SwitchManager.h"

// 环形缓冲区大小
#define DSO_Buffer_Size   5632
// 显示缓冲区点深度
#define DSO_ShowBuffer_Deep 601
// 排序后缓冲区大小：前1440 + 触发点 + 后1440 = 2881
#define SORTED_BUF_SIZE   2881
// 前触发点数
#define PRE_TRIGGER_POINTS  1440
// 后触发点数
#define POST_TRIGGER_POINTS 1440

#define WAVE_COLOR  0x367F
#define GRID_COLOR  0x3a09
#define BG_COLOR    0x0000
#define CLAMP(val, low, high) ((val) < (low) ? (low) : ((val) > (high) ? (high) : (val)))

extern const uint8_t grid_data[201][38];
extern const uint8_t y_lookup[256];

typedef enum {
    time_5us = 0,
    time_10us,
    time_20us,
    time_50us,
    time_100us,
    time_200us,
    time_500us,
    time_1ms,
    time_2ms,
    time_5ms,
    time_10ms,
    time_20ms,
    time_50ms,
    time_100ms,
    timebase_num
} TimeBase_e;

typedef enum {
    X1 = 0,
    X10 = 1
} ProbeAttenuation_e;

typedef struct {
    TimeBase_e time_base;
    uint32_t sample_rate;
    uint16_t trigger_timer_psc;
    uint16_t trigger_timer_arr;
    uint16_t refresh_timer_psc;
    uint16_t refresh_timer_arr;
    uint16_t refresh_time;
    char TimeBase_msg[8];
} TimeBase_t;

extern const TimeBase_t time_base[timebase_num];

typedef enum {
    div_5mv = 0,
    div_10mv,
    div_20mv,
    div_50mv,
    div_100mv,
    div_200mv,
    div_500mv,
    div_1v,
    div_2v,
    div_num
} Vdiv_e;

typedef struct {
    Vdiv_e div_type;
    float div_value;
    AttenuationTypeDef attenuation;
    MagnifyTypeDef magnify;
    float target_gain;
    float zoom_factor;
    char DivBase_msg[8];
} DivBase_t;

extern DivBase_t div_base[div_num];

typedef enum {
    EDGE_RISING,
    EDGE_FALLING,
} EdgeType_e;

typedef enum {
    TrigMode_Auto = 0,
    TrigMode_Single,
}TriggerMode_e;

extern const char *const TriggerMode_Msg[2];
extern const char *const CoupleType_Msg[2];

typedef enum {
    DSO_PAGE_MAIN = 0,          // 主界面
    DSO_PAGE_SETTING,           // 设置界面
    DSO_PAGE_NUM                // 页面总数（用于边界检查）
} DSO_AppPage_t;

typedef enum {
    Setting_TimeBase = 0,       // 时间基准
    Setting_VoltageDiv,         // 电压基准
    Setting_TriggerEdge,        // 触发边沿
    Setting_TriggerMode,        // 触发模式
    Setting_CoupleMode  ,       // 耦合模式
}DSO_SettingMenu_t;



#endif // XM_POWER_KIT_DSOCORETASK_H