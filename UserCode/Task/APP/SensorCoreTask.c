#include <inttypes.h>
#include <stdio.h>
#include "lcd_draw_api.h"
#include "UserTask.h"
#include "os_handles.h"


void Star_tSensorCoreTask(void *argument) {
    osThreadSuspend(SensorCoreTaskHandle);
}


void Suspend_SensorTask(void) {

}


void Resume_SensorTask(void) {

}