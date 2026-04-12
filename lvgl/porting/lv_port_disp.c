/**
 * @file lv_port_disp.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include <stdbool.h>

#include "cmsis_os2.h"
#include "os_handles.h"
#include "freertos_os2.h"
#include "UserTask.h"

/*********************
 *      DEFINES
 *********************/

#define MY_DISP_HOR_RES    320      // 屏幕长
#define MY_DISP_VER_RES    240      // 屏幕宽

/**********************
 *      TYPEDEFS
 **********************/
lv_disp_drv_t user_disp_drv;                   /*Descriptor of a display driver*/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*-----------------------------
     * Create a buffer for drawing
     *----------------------------*/

    static lv_disp_draw_buf_t draw_buf_dsc_2;
    static lv_color_t buf_2_1[MY_DISP_HOR_RES * 40] ;
    static lv_color_t buf_2_2[MY_DISP_HOR_RES * 40] ;
    lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * 40);

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/

    lv_disp_drv_init(&user_disp_drv);                     /*Basic initialization*/

    /*Set up the functions to access to your display*/

    /*Set the resolution of the display*/
    user_disp_drv.hor_res = MY_DISP_HOR_RES;
    user_disp_drv.ver_res = MY_DISP_VER_RES;

    /*Used to copy the buffer's content to the display*/
    user_disp_drv.flush_cb = disp_flush;

    /*Set a display buffer*/
    user_disp_drv.draw_buf = &draw_buf_dsc_2;

    /*Finally register the driver*/
    lv_disp_drv_register(&user_disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    /*You code here*/
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    if(disp_flush_enabled) {
        // 为当前刷新请求分配内存
        refresh_msg_t* msg = pvPortMalloc(sizeof(refresh_msg_t));
        if (msg == NULL) {
            lv_disp_flush_ready(disp_drv);
            return;
        }

        // 填充消息内容
        msg->RefreshType = REFRESH_TYPE_LVGL;   // 刷新类型为 LVGL
        msg->area = area;                       // 刷新区域指针
        msg->color_p = color_p;                 // 颜色数据指针
        msg->cmd_mem = NULL;                    // 手动刷新命令指针设为NULL
        msg->msg_mem = msg;                     // 消息自身内存指针指向自己，方便后续释放

        // 发送消息到 LCD 刷新队列
        if (osMessageQueuePut(LcdMsgQueueHandle, &msg, 0, 10) != osOK) {
            // 发送失败，释放分配的内存并通知 LVGL 刷新完成
            vPortFree(msg);
            lv_disp_flush_ready(disp_drv);
        }

    }else {
        // 如果刷新被禁用，直接通知 LVGL 刷新完成
        lv_disp_flush_ready(disp_drv);
    }
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif