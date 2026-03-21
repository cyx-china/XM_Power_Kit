#include "PID.h"
#include <math.h>
#include <stdint.h>

#include "cmsis_os2.h"
#include "UserDefineManage.h"
#include "UserTask.h"

volatile float Target_Voltage = 5.0f;       // 目标电压
volatile float Target_Current = 2.0f;       // 目标电流

// 静态计数器：防抖+调节限速
static uint8_t cv_adjust_cnt = 0;          // 防抖计数器
static uint8_t cv_adjust_timer = 0;        // 调节限速计数器（记录中断次数）

void SetTargetVoltage(float Voltage) {
    // 目标电压限幅
    Voltage = fmaxf(fminf(Voltage, MAX_OUTPUT_VOLTAGE), MIN_OUTPUT_VOLTAGE);
    Target_Voltage = Voltage;

    // CV模式下立即重置基准DAC
    if (pid_controller.mode == 0) {
        float base_dac = UserParam.DPS_Voltage_DAC_Coefficient * Voltage + UserParam.DPS_Voltage_DAC_Constant;      // 根据校准数据计算基础DAC值
        pid_controller.current_dac = (uint16_t)roundf(base_dac);
        // 硬件限幅
        pid_controller.current_dac = (pid_controller.current_dac > 4095) ? 4095 : pid_controller.current_dac;
        pid_controller.current_dac = (pid_controller.current_dac < 0)    ? 0    : pid_controller.current_dac;
        cv_adjust_cnt = 0;        // 重置防抖计数
        cv_adjust_timer = 0;      // 重置限速计数
    }
    osDelay(100);
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
    pid_controller.result        = 0.0f;        // CC模式PID增量
    // 初始化基准DAC（5V）
    pid_controller.current_dac   = (uint16_t)(UserParam.DPS_Voltage_DAC_Coefficient * 5.0f +
                                              UserParam.DPS_Voltage_DAC_Constant);
    pid_controller.prev_error     = 0.0f;
    pid_controller.prev_prev_error = 0.0f;
    // 初始化计数器
    cv_adjust_cnt = 0;
    cv_adjust_timer = 0;
}

uint16_t PID_Calculate(float measured_current, float measured_voltage) {
    // 模式切换逻辑
    float cc_enter_threshold = Target_Current + pid_controller.hysteresis;  // 进入CC模式的电流 ： 目标电流 + 死区电流
    float cc_exit_threshold  = Target_Current - pid_controller.hysteresis;  // 退出CC模式的电流 ： 目标电流 - 死区电流
    cc_enter_threshold = fminf(cc_enter_threshold, MAX_OUTPUT_CURRENT);     // 限幅
    cc_exit_threshold  = fmaxf(cc_exit_threshold, MIN_OUTPUT_CURRENT);

    if (pid_controller.mode == 0) { // CV→CC
        if (measured_current >= cc_enter_threshold) {
            pid_controller.mode = 1;
            pid_controller.prev_error = 0.0f;
            pid_controller.prev_prev_error = 0.0f;
            pid_controller.result = 0.0f;
            PowerMode = true;
            cv_adjust_cnt = 0;
            cv_adjust_timer = 0;
        }
    } else { // CC→CV
        if (measured_current <= cc_exit_threshold) {
            pid_controller.mode = 0;
            PowerMode = false;
            cv_adjust_cnt = 0;
            cv_adjust_timer = 0;
            // 切回CV时初始化基准DAC
            float compensated_voltage = fmaxf(fminf(Target_Voltage, MAX_OUTPUT_VOLTAGE), MIN_OUTPUT_VOLTAGE);
            pid_controller.current_dac = (uint16_t)roundf(
                UserParam.DPS_Voltage_DAC_Coefficient * compensated_voltage + UserParam.DPS_Voltage_DAC_Constant
            );
        }
    }

    // PID计算逻辑
    uint16_t target_dac;

    if (pid_controller.mode == 0) {   // CV模式 —— 直接微调DAC
        // 直接基于上一次存储的current_dac微调
        target_dac = pid_controller.current_dac;

        // 计算电压误差（实测 - 目标）
        float voltage_error = measured_voltage - Target_Voltage;

        // 误差分层判断
        if (fabs(voltage_error) > CV_MAX_ADJUST_ERROR) {
            // 情况1：误差超过±0.15V → 直接重置为基准值
            float base_dac = UserParam.DPS_Voltage_DAC_Coefficient * Target_Voltage + UserParam.DPS_Voltage_DAC_Constant;
            target_dac = (uint16_t)roundf(base_dac);
            target_dac = (target_dac > 4095) ? 4095 : target_dac;
            target_dac = (target_dac < 0)    ? 0    : target_dac;
            cv_adjust_cnt = 0;        // 重置防抖计数
            cv_adjust_timer = 0;      // 重置限速计数
        } else if (fabs(voltage_error) > CV_VOLTAGE_TOLERANCE) {
            // 情况2：误差在±0.02~±0.15V之间 → 先防抖，再限速调节
            cv_adjust_cnt++;
            if (cv_adjust_cnt >= 2) { // 防抖计数达标
                // 限速计数器累加（1ms/次，累计到 CV_ADJUST_INTERVAL 才调节）
                cv_adjust_timer++;
                if (cv_adjust_timer >= CV_ADJUST_INTERVAL) {    // 这里暂定为0，如果你有过冲/震荡，可以将CV_ADJUST_INTERVAL调大
                    // 执行±1微调
                    if (voltage_error < -CV_VOLTAGE_TOLERANCE) {    //(DAC与输出电压呈现负相关)
                        target_dac -= 1; // 电压偏低，DAC - 1
                    } else if (voltage_error > CV_VOLTAGE_TOLERANCE) {
                        target_dac += 1; // 电压偏高，DAC + 1
                    }
                    cv_adjust_cnt = 0;        // 重置防抖计数
                    cv_adjust_timer = 0;      // 重置限速计数
                }
            }
        } else {
            // 情况3：误差≤±0.02V → 不调节，重置所有计数器
            cv_adjust_cnt = 0;
            cv_adjust_timer = 0;
        }

        // DAC硬件限幅
        target_dac = (target_dac > 4095) ? 4095 : target_dac;
        target_dac = (target_dac < 0)    ? 0    : target_dac;

    } else { // CC模式 ——  增量式PID计算
        float error = measured_current - Target_Current + 0.01f;
        float increment = UserParam.DPS_Loop_P * (error - pid_controller.prev_error)
                        + UserParam.DPS_Loop_I * error
                        + UserParam.DPS_Loop_D * (error - 2.0f * pid_controller.prev_error + pid_controller.prev_prev_error);
        increment = fmaxf(fminf(increment, pid_controller.integral_max), pid_controller.integral_min);
        pid_controller.result = increment;

        int32_t dac_temp = (int32_t)pid_controller.current_dac + (int32_t)roundf(increment);
        dac_temp = (dac_temp > 4095) ? 4095 : dac_temp;
        dac_temp = (dac_temp < 0)    ? 0    : dac_temp;
        target_dac = (uint16_t)dac_temp;

        pid_controller.prev_prev_error = pid_controller.prev_error;
        pid_controller.prev_error = error;
    }

    // 存储本次DAC值，供下一次计算使用
    pid_controller.current_dac = target_dac;
    return target_dac;
}