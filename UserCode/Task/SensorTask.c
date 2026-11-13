/**
******************************************************************************
  * @file           : SensorTask.c
  * @brief          : 传感器数据采集任务，包括读取INA226（电压/电流/功率）、
  *                   TMP102（DCDC模块温度），以及基于温度通过PWM占空比实现风扇转速自动调节。
  * @date           : 2026/2/21
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  *
  * This sensor data acquisition task module is independently developed by the author.
  * It is released under the CC-BY-NC-SA 4.0 open source license.
  * And the author's right of attribution is reserved.
  ******************************************************************************
  */

#include <string.h>
#include "INA226_Driver.h"
#include "TMP102.h"
#include "UserTask.h"
#include "freertos_os2.h"
#include "os_handles.h"
#include "tim.h"
#include "SwitchManager.h"
#include "UserDefineManage.h"

static void FAN_Regulation(void);
static void SleepingProcess(void);

// ===================== 设备句柄 ===================//

extern TMP102_HandleTypeDef htmp102_dcdc;

extern INA226_HandleTypeDef hina226_input;

// ===================== 定义 ===================//

volatile float DCDC_Temperature = 0.00f;    // DCDC 温度

volatile float Input_Voltage = 0.00f;    // 输入电压
volatile float Input_Current = 0.00f;    // 输入电流
volatile float Input_Power   = 0.00f;    // 输入功率

volatile uint8_t Fan_Duty_Cycle = 0;    // 风扇占空比

volatile bool IsSleeping = false;       // 是否处于睡眠状态

static uint32_t s_idle_start_tick = 0;  // 最近一次用户操作的系统节拍（休眠计时）
static bool s_fan_on = false;           // 风扇当前是否已启动（回差判断用）
// ===================== 任务函数 ===================//

void Start_SensorTask(void *argument){
    uint8_t counter = 0;
    for(;;)
    {
        counter++;
        if(counter > 8) {   // 每1s调控一次风扇,以及睡眠检测
            FAN_Regulation();
            SleepingProcess();
            counter = 0;
        }

        // INA226 与 TMP102 都挂在 I2C1 上，合并为一次加锁，避免反复申请/释放互斥锁
        osMutexAcquire(IIC1_MutexHandle, osWaitForever);

        // 电压与电流用一次自增读事务取回（原先两次独立事务，多花一个 I2C 传输时间）
        float input_voltage = 0.0f;
        float input_current = 0.0f;
        INA226_ReadBusVoltageAndCurrent(&hina226_input, &input_voltage, &input_current);
        Input_Voltage = input_voltage;
        Input_Current = input_current;
        Input_Power   = input_voltage * input_current;      // 输入功率 P = U × I
        DCDC_Temperature = TMP102_ReadTemperature(&htmp102_dcdc);

        osMutexRelease(IIC1_MutexHandle);

        osDelay(125);
    }

}


const uint8_t DUTY_MIN     = 20;        // 最低占空比
const uint8_t DUTY_MAX     = 100;       // 最高占空比

#define FAN_HYSTERESIS_C    2.0f        // 关断回差（℃），避免在启转温度附近反复启停
#define FAN_FULL_GAP_C      5.0f        // 满速温度至少比启转温度高出多少度

static uint8_t Fan_GetDutyCycle(float current_temp){
    if (UserParam.Fan_Enable == 0){      // 未启用风扇：清状态并返回 0
        s_fan_on = false;
        return 0;
    }

    float start_temp = (float)UserParam.Fan_StartTemperature;
    float full_temp  = (float)UserParam.Fan_FullTemperature;

    if (full_temp < (start_temp + FAN_FULL_GAP_C)) {   // 参数不合理时拉开间距，避免除零
        full_temp = start_temp + FAN_FULL_GAP_C;
    }

    if (!s_fan_on) {                     // 未启动：升到启转温度才开
        if (current_temp < start_temp) {
            return 0;
        }
        s_fan_on = true;
    } else if (current_temp <= (start_temp - FAN_HYSTERESIS_C)) {   // 已启动：降到回差以下才关
        s_fan_on = false;
        return 0;
    }

    if (current_temp <= start_temp) {    // 回差带内保持最低转速，不直接停
        return DUTY_MIN;
    }
    if (current_temp >= full_temp) {
        return DUTY_MAX;
    }

    float temp_range   = full_temp - start_temp;
    float duty_range   = (float)(DUTY_MAX - DUTY_MIN);
    float temp_percent = (current_temp - start_temp) / temp_range;

    uint8_t duty = (uint8_t)((float)DUTY_MIN + duty_range * temp_percent + 0.5f);

    if (duty > DUTY_MAX) duty = DUTY_MAX;
    if (duty < DUTY_MIN) duty = DUTY_MIN;
    return duty;
}

static void FAN_Regulation(void) {
    Fan_Duty_Cycle = Fan_GetDutyCycle(DCDC_Temperature);
    PWM_SetFanDuty(Fan_Duty_Cycle);     // 按 TIM1 实际 ARR 换算占空比
}

static void SleepingProcess(void) {
    uint32_t timeout_ms = (uint32_t)UserParam.Screen_Sleeptime * 1000U;

    if (IsSleeping || timeout_ms == 0U) {       // 已经在睡眠模式或关闭休眠，直接返回
        return;
    }
    if ((osKernelGetTickCount() - s_idle_start_tick) >= timeout_ms) {   // 按真实毫秒计时
        IsSleeping = true;
        PWM_SetBrightness(10);      // 将屏幕亮度设置为10 %
    }
}

/* 复位休眠计时，从"现在"重新开始算（InitTask 启动时、修改休眠时间时调用） */
void Sensor_ResetIdleTimer(void) {
    s_idle_start_tick = osKernelGetTickCount();
}

void WakeUp(void) {
    Sensor_ResetIdleTimer();        // 任何操作都重新计时

    if (!IsSleeping){return;}       // 不在睡眠模式，直接返回

    IsSleeping = false;
    PWM_SetBrightness((uint8_t)UserParam.Screen_Brightness);  // 恢复屏幕亮度
}


