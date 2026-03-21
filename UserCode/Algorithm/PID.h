#ifndef XM_POWER_KIT_PID_H
#define XM_POWER_KIT_PID_H
#include <stdint.h>


#define MAX_OUTPUT_VOLTAGE    36.0f         // 最大输出电压
#define MIN_OUTPUT_VOLTAGE    1.0f          // 最小输出电压

#define MAX_OUTPUT_CURRENT    10.0f         // 最大输出电流
#define MIN_OUTPUT_CURRENT    0.1f          // 最小输出电流

#define CV_VOLTAGE_TOLERANCE  0.01f         // 精度阈值（±0.01V）
#define CV_MAX_ADJUST_ERROR   0.15f         // 最大调节误差（±0.15V）
#define CV_ADJUST_INTERVAL    0             // 调节间隔（中断次数）：调试参数，不用调

typedef struct {
    uint8_t mode;                           // 0 = CV, 1 = CC
    float hysteresis;                       // 电流回差（A）
    float integral_max;                     // CC 积分限幅正向
    float integral_min;                     // CC 积分限幅负向
    float result;                           // 本次计算的增量（仅 CC 使用）
    uint16_t current_dac;                   // 当前 DAC 值

    float prev_error;                       // 上一次电流误差（CC）
    float prev_prev_error;                  // 上上一次电流误差（CC）
} PID;

extern PID pid_controller;
extern volatile float Target_Voltage;       // 目标电压
extern volatile float Target_Current;       // 目标电流



void PID_Init(void);                                                        // 初始化PID参数
uint16_t PID_Calculate(float measured_current, float measured_voltage);     // PID计算

void SetTargetVoltage(float Voltage);                                       // 更新目标电压
void SetTargetCurrent(float Current);                                       // 更新目标电流



#endif //XM_POWER_KIT_PID_H