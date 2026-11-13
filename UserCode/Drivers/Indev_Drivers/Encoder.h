#ifndef XM_POWER_KIT_INDEV_DRIVER_H
#define XM_POWER_KIT_INDEV_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "Keys.h"


// 通用读取
bool get_encoder_data(int32_t *diff, bool *pressed);

// 重置状态
void reset_encoder_state(void);
void Encoder_Init(void);

void Encoder_Scan(void);

#endif //XM_POWER_KIT_INDEV_DRIVER_H