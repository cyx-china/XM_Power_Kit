#include <inttypes.h>
#include <stdio.h>
#include "lcd_draw_api.h"
#include "UserTask.h"
#include "os_handles.h"

// 这个任务是空闲的，你可以在这里编写读书与你的功能，它对应着主界面第二排第一个应用。
// 你可以参考其他APP的结构，下面我只保留最基础的结构。
// 此任务的栈大小为2K，如需要拓展，可以自行修改

// 如果要启用，请将UserTask.h末尾的 #define USE_USER_APP 0 改成1

void Start_UserCoreTask(void *argument){
    osThreadSuspend(UserCoreTaskHandle);    // 初始挂起本任务


    for (;;) {

    }
}









// 挂起本APP需要执行的内容
void Suspend_UserCoreTask(void) {




    Suspend_IndevDetectTask();                   // 挂起输入设备检测任务
    osThreadSuspend(UserCoreTaskHandle);         // 挂起核心任务
}

// 进入本APP需要执行的内容
void Resume_UserCoreTask(void) {




    osThreadResume(UserCoreTaskHandle);         // 恢复核心任务
    Resume_IndevDetectTask();                   // 恢复输入设备检测任务
}