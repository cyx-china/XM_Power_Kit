/**
******************************************************************************
  * @file           : PageSelectTask.c
  * @brief          : 多应用系统的应用切换任务，包括挂起/恢复不同应用（LVGL、电源、示波器、任意波形发生器、数字万用表）的任务，
  *                   通过消息队列处理应用切换请求，并管理当前应用的运行状态。
  * @date           : 2026/2/21
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  *
  * This App switch task module is independently developed by the author.
  * It is released under the CC-BY-NC-SA 4.0 open source license.
  * And the author's right of attribution is reserved.
  ******************************************************************************
  */

#include "cmsis_os2.h"
#include "UserTask.h"
#include "os_handles.h"
#include "ST7789.h"
#include "wave_generator.h"

static void SuspendTask(AppListType APP);
static void ResumeTask(AppListType APP);

AppListType RunningApp = APP_NONE;

static AppListType TargetApp;        // 最近一次从队列取到的目标 APP（仅本文件使用）


void Start_PageSelectTask(void *argument) {
    // 自挂启，等待app切换
    osThreadSuspend(PageSelectTaskHandle);

    //上电首先让LVGL的App选择界面运行
    osThreadResume(LvglCoreTaskHandle);         // 恢复LVGL任务
    IsLvglRunning = true;                       // 设置标志位，允许lv_tick_inc被调用
    RunningApp = APP_LVGL;                      // 标记当前运行的APP为LVGL


    for (;;) {
        if (osMessageQueueGet(AppSwitchQueueHandle,&TargetApp,NULL,osWaitForever) != osOK) {
            TargetApp = APP_NONE;
            continue;
        }

        // 切换前先校验目标 APP：
        // 1) 越界或 APP_NONE 直接丢弃，避免 ResumeTask 查表落空；
        // 2) 若先把当前 APP 挂起、再发现目标无法运行，就会出现"所有 APP 都挂起"的
        //    死局（只能复位），因此必须先确认目标合法再挂起当前 APP。
        if (TargetApp <= APP_NONE || TargetApp > APP_CAL) {
            TargetApp = APP_NONE;
            continue;
        }
        if (TargetApp == RunningApp) {
            TargetApp = APP_NONE;
            continue;
        }

        SuspendTask(RunningApp);
        RunningApp = APP_NONE;

        ResumeTask(TargetApp);
        RunningApp = TargetApp;
        TargetApp = APP_NONE;
    }
}

// 挂起指定App任务
static void SuspendTask(AppListType APP) {
    switch (APP) {
        case APP_LVGL:      Suspend_LvglCoreTask();     break;
        case APP_POWER:     Suspend_DpsCoreTask();      break;
        case APP_DSO:       Suspend_DsoCoreTask();      break;
        case APP_AWG:       Suspend_AwgTask();          break;
        case APP_DMM:       Suspend_DmmTask();          break;
        case APP_CAL:       Suspend_CalibrateTask();    break;
        case APP_NONE:                                  break;
    }
}

// 恢复指定任务
static void ResumeTask(AppListType APP) {
    switch (APP) {
        case APP_LVGL:      Resume_LvglCoreTask();      break;
        case APP_POWER:     Resume_DpsCoreTask();       break;
        case APP_DSO:       Resume_DsoCoreTask();       break;
        case APP_AWG:       Resume_AwgTask();           break;
        case APP_DMM:       Resume_DmmTask();           break;
        case APP_CAL:       Resume_CalibrateTask();     break;
        case APP_NONE:                                  break;
    }
}













