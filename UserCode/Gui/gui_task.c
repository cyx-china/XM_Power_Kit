/**
  ******************************************************************************
  * @file           : gui_task.c
  * @brief          : GUI 任务：消费按键事件队列并驱动自研 GUI 框架
  * @date           : 2026/11/13
  ******************************************************************************
  * 任务入口先自挂起，由 PageSelectTask 唤醒；与其它 APP 一样通过
  * KeyEventQueue 接收按键/编码器事件，长按编码器由各 APP 自己处理（返回首页）。
  ******************************************************************************
  */

#include "cmsis_os2.h"
#include "gui.h"
#include "Keys.h"
#include "os_handles.h"
#include "UserTask.h"
#include "SwitchManager.h"

/* 把按键/编码器事件翻译成 GUI 事件 */
static GuiKey_t gui_from_event(const KeyEventMsg_t *msg) {
    switch (msg->key) {
        case KEY_UP:
            if (msg->event == KEY_EVENT_CLICK || msg->event == KEY_EVENT_LONG_HOLD) {
                return GUI_KEY_UP;
            }
            break;
        case KEY_DOWN:
            if (msg->event == KEY_EVENT_CLICK || msg->event == KEY_EVENT_LONG_HOLD) {
                return GUI_KEY_DOWN;
            }
            break;
        case KEY_SET:
            if (msg->event == KEY_EVENT_CLICK) {
                return GUI_KEY_ENTER;
            }
            break;
        case KEY_MODE:
            if (msg->event == KEY_EVENT_CLICK || msg->event == KEY_EVENT_LONG_PRESS) {
                return GUI_KEY_BACK;
            }
            break;
        case KEY_ENCODER:
            if (msg->event == KEY_EVENT_CLICK) {
                return GUI_KEY_ENTER;
            }
            if (msg->event == ENCODER_EVENT_LEFT || msg->event == ENCODER_EVENT_PRESS_LEFT) {
                return GUI_KEY_LEFT;
            }
            if (msg->event == ENCODER_EVENT_RIGHT || msg->event == ENCODER_EVENT_PRESS_RIGHT) {
                return GUI_KEY_RIGHT;
            }
            break;
        default:
            break;
    }
    return GUI_KEY_NONE;
}

void Start_GuiTask(void *argument) {
    osThreadSuspend(GuiTaskHandle);         // 自挂起，等待 PageSelectTask 唤醒

    Resume_IndevDetectTask();               // 上电后由 GUI 打开按键/编码器扫描（各 APP 会在自己的 Resume 里重新打开）
    Gui_Init();                             // 清屏并进入首页

    for (;;) {
        KeyEventMsg_t msg;

        if (osMessageQueueGet(KeyEventQueueHandle, &msg, NULL, 10U) == osOK) {
            GuiKey_t key = gui_from_event(&msg);
            if (key != GUI_KEY_NONE) {
                StartBeezer(0);             // 按键提示音
                WakeUp();                   // 唤醒屏幕
                Gui_Dispatch(key);
            }
        }
    }
}

void Suspend_GuiTask(void) {
    Suspend_IndevDetectTask();              // 输入扫描交给目标 APP
    osThreadSuspend(GuiTaskHandle);
}

void Resume_GuiTask(void) {
    KeyEventMsg_t stale;

    // 丢弃切换 APP 期间残留的按键事件，避免恢复后立刻误触发
    while (osMessageQueueGet(KeyEventQueueHandle, &stale, NULL, 0U) == osOK) {
    }

    Resume_IndevDetectTask();
    osThreadResume(GuiTaskHandle);
    Gui_Redraw();                           // 恢复后整屏重绘（此时 GUI 任务已恢复，绘制按队列顺序执行）
}
