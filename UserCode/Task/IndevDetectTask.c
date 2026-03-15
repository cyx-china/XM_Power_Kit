/**
******************************************************************************
  * @file           : IndevDetectTask.c
  * @brief          : 输入设备检测任务，包括按键/编码器扫描、通过消息队列实现蜂鸣器PWM控制，并提供输入扫描的挂起/恢复接口
  *                   （仅由非LVGL应用启用）。
  * @date           : 2026/2/21
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  *
  * This input device detection task module is independently developed by the author.
  * It is released under the CC-BY-NC-SA 4.0 open source license.
  * And the author's right of attribution is reserved.
  ******************************************************************************
  */

#include "cmsis_os2.h"
#include "Encoder.h"
#include "UserTask.h"
#include "Keys.h"
#include "os_handles.h"
#include "tim.h"
#include "UserDefineManage.h"


bool IsIndevDetectTaskRunnin = false;


void Start_IndevDetectTask(void *argument) {


    Key_Init();
    reset_encoder_state();

    uint16_t BeezerTime = 0;
    uint16_t BeezerRemainTicks = 0;
    uint8_t BeezerIsActive = 0;


    for (;;) {
        if (osMessageQueueGet(BeezerQueueHandle, &BeezerTime, NULL, 0) == osOK) {
            BeezerRemainTicks = UserParam.Beezer_Time + 1;  // 先赋值默认值
            if (BeezerTime != 0){BeezerRemainTicks = BeezerTime / 10 + 1;}  // 如果入参不是0，那么就计算时间的时间
            BeezerIsActive = 1;
            HAL_TIM_PWM_Start(&htim9, TIM_CHANNEL_1);
        }

        if (BeezerIsActive) {
            BeezerRemainTicks--;
            if (BeezerRemainTicks == 0) {
                HAL_TIM_PWM_Stop(&htim9, TIM_CHANNEL_1);
                BeezerIsActive = 0;
            }
        }

        if (IsIndevDetectTaskRunnin == true) {  // 仅启动了此任务，才会进行输入扫描 (由除了lvgl外的app开启)
            Encoder_Scan();
            Key_Scan();
        }

        osDelay(10);
    }
}

// 传入0则使用设置里的时间，否则按传入值计算
void StartBeezer(uint16_t time_ms) {
    osMessageQueuePut(BeezerQueueHandle,&time_ms,0,0);
}



void Suspend_IndevDetectTask(void) {
    Key_Init();                             // 重置按键状态
    Encoder_Init();                         // 重置旋转编码器状态
    IsIndevDetectTaskRunnin = false;
}

void Resume_IndevDetectTask(void) {
    Key_Init();                             // 重置按键状态
    Encoder_Init();                         // 重置旋转编码器状态
    IsIndevDetectTaskRunnin = true;
}





