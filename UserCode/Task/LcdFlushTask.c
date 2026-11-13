/**
******************************************************************************
  * @file           : LcdFlushTask.c
  * @brief          : LCD刷新任务，用于处理显示刷新请求，包括LVGL界面刷新（通过DMA将数据传输至LCD）和手动绘图
  *                   （像素、直线、矩形、圆形、图片、字符串等），并包含内存管理功能。
  * @date           : 2026/2/21
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  *
  * This LCD flush task module is independently developed by the author.
  * It is released under the CC-BY-NC-SA 4.0 open source license.
  * And the author's right of attribution is reserved.
  ******************************************************************************
  */

#include "UserTask.h"
#include "cmsis_os2.h"
#include "graphics.h"
#include "lcd_draw_api.h"
#include "os_handles.h"
#include "ST7789.h"
#include "freertos_os2.h"
/*
 * 这个任务负责处理 LCD 刷新请求：从消息队列取出绘图命令，按入队顺序调用 graphics.c
 * 的图元函数（窗口 + DMA），并在 DRAW_DATA 传输完成后释放完成信号量。
 * 所有界面（自研 GUI 与各个 APP）都通过同一套 lcd_draw_* 接口下发绘制命令。
 */

// DRAW_DATA 完成信号量：由本任务在 DMA 传输结束后释放，lcd_draw_data() 阻塞等待它
osSemaphoreId_t DrawData_Done_SemHandle = NULL;

void Start_LcdFlushTask(void *argument) {
    refresh_msg_t* msg = NULL;
    osStatus_t ret;

    // 计数信号量（最大1、初值0）：每次 DRAW_DATA 传输完成释放一次
    if (DrawData_Done_SemHandle == NULL) {
        DrawData_Done_SemHandle = osSemaphoreNew(1, 0, NULL);
    }

    for (;;) {
        // 20ms 超时：队列空闲时顺便处理"图形缓存失效"请求（上位机改动了字库/图片文件）
        ret = osMessageQueueGet(LcdMsgQueueHandle, &msg, NULL, 20U);
        if (ret != osOK || msg == NULL) {
            GFX_ServiceCacheInvalidate();
            continue;
        }

        {
            if (msg->draw_command == NULL) {
                goto MEM_FREE;
            }
            const GFX_DrawCommand_t* cmd = msg->draw_command;

            switch (cmd->flush_type) {
                    case CLEAR_SCREEN:
                        GFX_ClearScreen(cmd->color);
                        break;
                    case DRAW_PIXEL:
                        GFX_DrawPixel(cmd->start_point >> 16, cmd->start_point & 0xFFFF, cmd->color);
                        break;
                    case DRAW_LINE:
                        GFX_DrawLine(cmd->start_point >> 16, cmd->start_point & 0xFFFF,
                                     cmd->end_point >> 16, cmd->end_point & 0xFFFF, cmd->color);
                        break;
                    case DRAW_DOTTED_LINE:
                        GFX_DrawDottedLine(cmd->start_point >> 16, cmd->start_point & 0xFFFF,
                                           cmd->end_point >> 16, cmd->end_point & 0xFFFF,
                                           cmd->color, cmd->param1, cmd->param2);
                        break;
                    case DRAW_RECT:
                        GFX_DrawRect(cmd->start_point >> 16, cmd->start_point & 0xFFFF,
                                     cmd->end_point >> 16, cmd->end_point & 0xFFFF,
                                     cmd->color, cmd->is_filled);
                        break;
                    case DRAW_ROUND_RECT:
                        GFX_DrawRoundRect(cmd->start_point >> 16, cmd->start_point & 0xFFFF,
                                          cmd->end_point >> 16, cmd->end_point & 0xFFFF,
                                          cmd->param1, cmd->color, cmd->is_filled);
                        break;
                    case DRAW_CIRCLE:
                        GFX_DrawCircle(cmd->start_point >> 16, cmd->start_point & 0xFFFF,
                                       cmd->param1, cmd->color, cmd->is_filled);
                        break;
                    case DRAW_ELLIPSE:
                        GFX_DrawEllipse(cmd->start_point >> 16, cmd->start_point & 0xFFFF,
                                        cmd->param1, cmd->param2, cmd->color, cmd->is_filled);
                        break;
                    case DRAW_IMAGE:
                        if (cmd->image != NULL) {
                            GFX_DrawImage(cmd->image, cmd->start_point >> 16, cmd->start_point & 0xFFFF);
                        }
                        break;
                    case DRAW_STRING:
                        if (cmd->font != NULL && cmd->str != NULL) {
                            GFX_DrawString(cmd->start_point >> 16, cmd->start_point & 0xFFFF,
                                           cmd->str, cmd->font, cmd->color, cmd->param1, cmd->param2);
                        }
                        break;
                    case DRAW_DATA: {
                        LCD_SET_WINDOWS(cmd->start_point >> 16, cmd->start_point & 0xFFFF, cmd->end_point >> 16,
                                        cmd->end_point & 0xFFFF);
                        uint32_t ptr = ((uint32_t)cmd->param1 << 16) | cmd->param2;
                        LCD_FLUSH_DMA(ptr, cmd->color);     // 内部阻塞等待 DMA 完成
                        if (DrawData_Done_SemHandle != NULL) {
                            osSemaphoreRelease(DrawData_Done_SemHandle);   // 唤醒 lcd_draw_data()
                        }
                        break;
                    }
                    case DRAW_IsoscelesTriangle:
                        GFX_DrawIsoscelesTriangle(cmd->start_point >> 16, cmd->start_point & 0xFFFF,cmd->param2,
                            cmd->param1,cmd->color);
                        break;
                default:
                    break;
            }
        }

MEM_FREE:
        // 绘制命令块都是静态池块，画完还回池里即可
        LcdDraw_ReleaseMsg(msg);
        // 清空msg指针
        msg = NULL;
    }
}