#ifndef DPS_CALIBRATE_H
#define DPS_CALIBRATE_H

#include "UserTask.h"

void dps_cal_handler(KeyEventMsg_t msg);
void DPS_Calibrate_Enter(void);
void DPS_Calibrate_Refresh(void);   // 用于定时刷新显示

#endif