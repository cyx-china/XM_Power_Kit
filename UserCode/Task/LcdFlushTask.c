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
#include "lv_hal_disp.h"
#include "lv_port_disp.h"
#include "os_handles.h"
#include "ST7789.h"
#include "freertos_os2.h"
/*
 * 这个函数是处理LCD刷新请求的，它分为如下两个部分：
 * 1. 适配LVGL的刷新请求，直接将lvgl缓冲区的数据通过DMA传输到LCD显示器上
 * 2. 适配手动调用刷新屏幕的需求，根据消息，调用对应的绘制函数
 */

// 高速内存拷贝函数保留（手动刷新逻辑可能潜在用到，若确认不用可删除）
// static void FastMemCopy(lv_color_t *restrict dst,const lv_color_t *restrict src,uint32_t pixel_count){
//     uint32_t *d = (uint32_t *)dst;
//     const uint32_t *s = (const uint32_t *)src;
//     uint32_t pairs = pixel_count >> 1;   // 多少组（每组 2 个像素 = 4 字节）
//
//     while (pairs >= 8)
//     {
//         d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
//         d[4] = s[4]; d[5] = s[5]; d[6] = s[6]; d[7] = s[7];
//         d += 8;
//         s += 8;
//         pairs -= 8;
//     }
//     while (pairs--)
//     {
//         *d++ = *s++;
//     }
//     // 处理奇数像素
//     if (pixel_count & 1)
//     {
//         *(uint16_t *)d = *(const uint16_t *)s;
//     }
// }

void Start_LcdFlushTask(void *argument) {
    refresh_msg_t* msg = NULL;
    osStatus_t ret;

    for (;;) {
        ret = osMessageQueueGet(LcdMsgQueueHandle, &msg, NULL, osWaitForever);
        if (ret != osOK || msg == NULL) {
            continue;
        }

        switch (msg->RefreshType) {
            case REFRESH_TYPE_LVGL:
                if (msg->area == NULL || msg->color_p == NULL) {
                    lv_disp_flush_ready(&user_disp_drv);
                    goto MEM_FREE;
                }

                uint32_t pixel_count = (msg->area->x2 - msg->area->x1 + 1) * (msg->area->y2 - msg->area->y1 + 1);
                LCD_SET_WINDOWS(msg->area->x1, msg->area->y1, msg->area->x2, msg->area->y2);

                LCD_FLUSH_DMA((uint32_t)msg->color_p, pixel_count);

                lv_disp_flush_ready(&user_disp_drv);
                break;

            case REFRESH_TYPE_MANUAL:
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
                    default:
                        break;
                }
                break;
            default:
                break;
        }

MEM_FREE:
        // 释放绘图命令内存（GFX_DrawCommand_t）
        if (msg->cmd_mem != NULL) {
            vPortFree((void*)msg->cmd_mem);
            msg->cmd_mem = NULL;
        }
        // 释放消息自身内存（refresh_msg_t）
        if (msg->msg_mem != NULL) {
            vPortFree((void*)msg->msg_mem);
            msg->msg_mem = NULL;
        }
        // 清空msg指针
        msg = NULL;
    }
}