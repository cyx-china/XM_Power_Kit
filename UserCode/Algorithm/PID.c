#include "PID.h"

#include <math.h>
#include <stdint.h>

#include "UserDefineManage.h"
#include "UserTask.h"


volatile float Target_Voltage = 5.0f;       // 目标电压
volatile float Target_Current = 2.0f;       // 目标电流

void SetTargetVoltage(float Voltage) {
    Target_Voltage = Voltage;
}

void SetTargetCurrent(float Current) {
    Target_Current = Current;
}

PID pid_controller = {0};

void PID_Init(void) {
    pid_controller.mode          = 0;           // 初始为 CV模式
    pid_controller.hysteresis    = 0.02f;       // 死区电流
    pid_controller.integral_max  = 200.0f;      // 积分项最大值
    pid_controller.integral_min  = -200.0f;     // 积分项最小值
    pid_controller.result        = 0.0f;        // 计算出的PID增量

    pid_controller.current_dac   = (uint16_t)(UserParam.DPS_Voltage_DAC_Coefficient * 5.0f +
                                              UserParam.DPS_Voltage_DAC_Constant);  // 默认5V
    pid_controller.prev_error     = 0.0f;
    pid_controller.prev_prev_error = 0.0f;
}


// PID计算，并返回最新的DAC值，直接写4725
uint16_t PID_Calculate(float measured_current) {
    // 计算限流阈值
    float cc_enter_threshold = Target_Current + pid_controller.hysteresis;  // CV进入CC阈值电流
    float cc_exit_threshold  = Target_Current - pid_controller.hysteresis;  // CC退回CV阈值电流
    // 阈值限制
    cc_enter_threshold = fminf(cc_enter_threshold, MAX_OUTPUT_CURRENT);
    cc_exit_threshold  = fmaxf(cc_exit_threshold, MIN_OUTPUT_CURRENT);

    // 模式切换逻辑
    if (pid_controller.mode == 0) { // 当前CV模式
        if (measured_current >= cc_enter_threshold) {   // 如果 当前电流 >= CV进入CC阈值电流
            pid_controller.mode = 1;                    // 进入CC模式
            // 清除CC模式历史
            pid_controller.prev_error = 0.0f;           // 上一次误差
            pid_controller.prev_prev_error = 0.0f;      // 上上次误差
            pid_controller.result = 0.0f;               // 计算出的PID增量
            // 更新mode标志
            PowerMode = true;
        }
    }else { // 当前CC模式
        if (measured_current <= cc_exit_threshold) {    // 如果 当前电流 <= CC退回CV阈值电流
            pid_controller.mode = 0;                    // 回到CV模式
            // 更新mode标志
            PowerMode = false;
        }
    }

    // 开始PID计算
    uint16_t target_dac;

    if (pid_controller.mode == 0) {   // 如果当前是CV模式
        float compensated_voltage = Target_Voltage;
        // 限幅一下DAC值，虽然可以去掉，但以防万一 ~
        compensated_voltage = fmaxf(fminf(compensated_voltage, MAX_OUTPUT_VOLTAGE), MIN_OUTPUT_VOLTAGE);
        // 根据公式反推出目标DAC值
        target_dac = (uint16_t)roundf(UserParam.DPS_Voltage_DAC_Coefficient * compensated_voltage +
            UserParam.DPS_Voltage_DAC_Constant);

    }else { // 当前是CC模式，进行增量式PID计算
        // 计算误差
        float error = measured_current - Target_Current + 0.01f;    // 误差 = 目标电流 - 当前电流

        // PID 计算出增量
        float increment = UserParam.DPS_Loop_P * (error - pid_controller.prev_error)
                + UserParam.DPS_Loop_I * error
                + UserParam.DPS_Loop_D * (error - 2.0f * pid_controller.prev_error + pid_controller.prev_prev_error);


        // 增量限幅
        increment = fmaxf(fminf(increment, pid_controller.integral_max), pid_controller.integral_min);

        // 更新结果到结构体
        pid_controller.result = increment;

        // 计算新dac值
        int32_t dac_temp = (int32_t)pid_controller.current_dac + (int32_t)roundf(increment);

        // 限幅
        dac_temp = (dac_temp > 4095) ? 4095 : dac_temp;
        dac_temp = (dac_temp < 0)    ? 0    : dac_temp;

        target_dac = (uint16_t)dac_temp;

        // 更新误差历史
        pid_controller.prev_prev_error = pid_controller.prev_error;
        pid_controller.prev_error = error;
    }
    // 计算完成，更新状态
    pid_controller.current_dac = target_dac;
    return target_dac;
}





























