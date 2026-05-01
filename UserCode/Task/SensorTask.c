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
#include "lv_label.h"
#include "os_handles.h"
#include "tim.h"
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
volatile int32_t SleepCounter = 0;      // 睡眠计数器
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

        osMutexAcquire(IIC1_MutexHandle, osWaitForever);

        Input_Voltage = INA226_ReadBusVoltage(&hina226_input);
        Input_Current = INA226_ReadCurrent(&hina226_input);
        Input_Power   = Input_Current * Input_Power;

        osMutexRelease(IIC1_MutexHandle);

        osMutexAcquire(IIC1_MutexHandle, osWaitForever);
        DCDC_Temperature = TMP102_ReadTemperature(&htmp102_dcdc);
        osMutexRelease(IIC1_MutexHandle);

        osDelay(125);
    }

}


const float TEMP_FULL      = 60.0f;     // 达到100%占空比的温度（℃）
const uint8_t DUTY_MIN     = 20;        // 最低占空比
const uint8_t DUTY_MAX     = 100;       // 最高占空比

static uint8_t Fan_GetDutyCycle(float current_temp){
    if (UserParam.Fan_Enable == 0){return 0;}       // 未启用风扇，占空比返回0
    
    if (current_temp <= (float)UserParam.Fan_StartTemperture) {return 0;}
    if (current_temp >= TEMP_FULL)  {return DUTY_MAX;}

    float temp_range   = TEMP_FULL - (float)UserParam.Fan_StartTemperture;
    float duty_range   = (float)(DUTY_MAX - DUTY_MIN);
    float temp_percent = (current_temp - (float)UserParam.Fan_StartTemperture) / temp_range;

    uint8_t duty = (uint8_t)((float)DUTY_MIN + duty_range * temp_percent + 0.5f);

    if (duty > DUTY_MAX) duty = DUTY_MAX;
    if (duty < DUTY_MIN) duty = DUTY_MIN;
    return duty;
}

static void FAN_Regulation(void) {
    Fan_Duty_Cycle = Fan_GetDutyCycle(DCDC_Temperature);
    uint16_t ARR = 4999 * Fan_Duty_Cycle / 100 ;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, ARR);
}

static void SleepingProcess(void) {
    if (IsSleeping || UserParam.Screen_Sleeptime == 0){return;}    // 已经在睡眠模式或关闭休眠，直接返回

    SleepCounter --;            // 睡眠计数器减 1
    if (SleepCounter <= 0){     // 睡眠计数器小于等于0，进入睡眠模式
        IsSleeping = true;
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 4999 * 10 / 100);  // 将屏幕亮度设置为10 %
        SleepCounter = UserParam.Screen_Sleeptime;  //  重置睡眠计数器
    }
}

void WakeUp(void) {
    if (!IsSleeping){return;}    // 不在睡眠模式，直接返回

    IsSleeping = false;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 4999 * UserParam.Screen_Brightness / 100);  // 恢复屏幕亮度
}


