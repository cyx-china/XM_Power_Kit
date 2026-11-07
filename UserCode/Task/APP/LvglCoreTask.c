/**
******************************************************************************
  * @file           : LvglCoreTask.c
  * @brief          : LVGL图形系统核心任务，包括将内存池分配至CCMRAM、LVGL初始化（显示/输入设备）、字体加载、UI向导配置，
  *                   以及任务挂起/恢复控制（含编码器状态重置、屏幕刷新管理和运行标志维护）。
  * @date           : 2026/2/21
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  *
  * This LVGL core task module is independently developed by the author.
  * It is released under the CC-BY-NC-SA 4.0 open source license.
  * And the author's right of attribution is reserved.
  ******************************************************************************
  */

#include "cmsis_os2.h"
#include "UserTask.h"
#include "ff.h"
#include "events_init.h"
#include "gui_guider.h"
#include "Encoder.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "os_handles.h"

// PS:Lvgl 的 memory pool 直接由 lv_conf.h 的 LV_MEM_ADR 固定指向 CCMRAM(0x10000000)，
//   在 lv_mem.c 的 lv_init 中通过 lv_tlsf_create_with_pool 建立，不需要在这里再定义数组
//   （原先的 lvgl_mem_pool 数组没有任何引用，链接时被 --gc-sections 丢弃，属于死代码）。
//   段布局见 STM32F407XX_FLASH.ld 的 .ccmram_lvgl_reserved。

volatile bool IsLvglRunning = false;        // 指示 LVGL核心任务 是否正在运行
volatile bool IsLvglJustResumed = false;    // 指示 LVGL核心任务 是否刚刚恢复

lv_ui guider_ui;
lv_font_t * acme_regular_24 = NULL;
lv_font_t * yahei_15 = NULL;
lv_font_t * yahei_18 = NULL;

void Start_LvglCoreTask(void *argument) {
    // 自挂启，等待app切换
    osThreadSuspend(LvglCoreTaskHandle);

    //lv_little_svg_init();           // 初始化svg库
    lv_init();                      // 初始化lvgl
    lv_port_disp_init();            // 初始化显示设备
    lv_port_indev_init();           // 初始化输入设备

    // 加载字体
    acme_regular_24 = lv_font_load("S:/font/acme_regular_24.bin");  // 我的建议是，不要加载大于24号的字体
    yahei_15 = lv_font_load("S:/font/yahei_15.bin");                // 否则内存占用巨大
    yahei_18 = lv_font_load("S:/font/yahei_18.bin");

    setup_ui(&guider_ui);           // 初始化gui-guider界面
    events_init(&guider_ui);        // 初始化回调事件

    for (;;) {
        if (IsLvglJustResumed == true) {
            // 刚刚恢复，重绘全屏
            lv_obj_invalidate(lv_scr_act());  // 标记全屏脏区
            IsLvglJustResumed = false;        // 重置标志位
        }

        lv_task_handler();                   // lvgl的主任务处理（内部已经调用 lv_timer_handler，不要再调一次）
        osDelay(10);

    }
}

void Suspend_LvglCoreTask(void) {
    osDelay(300);
    IsLvglRunning = false;                  // 将标志位设为 false，停止lv_tick_inc的调用 <- main.c末尾
    disp_disable_update();                  // 禁止屏幕刷新
    osThreadSuspend(LvglCoreTaskHandle);    // 挂起LVGL核心任务
    Suspend_IndevDetectTask();              // 挂起输入设备检测任务
}

void Resume_LvglCoreTask(void) {
    Suspend_IndevDetectTask();

    reset_encoder_state();                  // 重置编码器状态，避免恢复后出现突跳
    IsLvglJustResumed = true;               // 将标志位设为 true，告诉LVGL准备重绘全屏
    osThreadResume(LvglCoreTaskHandle);     // 恢复LVGL核心任务

    osDelay(500);                      // 等你放手..... 听到蜂鸣器就立马放，按久了会重新进入app

    disp_enable_update();                   // 允许屏幕刷新
    lv_obj_invalidate(lv_scr_act());        // 重绘全屏
    IsLvglRunning = true;                   // 将标志位设为 true，来恢复lv_tick_inc的调用
}
