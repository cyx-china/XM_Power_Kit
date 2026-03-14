#ifndef XM_POWER_KIT_TMP102_H
#define XM_POWER_KIT_TMP102_H


// ============== 头文件引入 =============== //
#include "stm32f4xx_hal.h"


// ================ 定义 ================== //
typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint16_t dev_addr;      // 写地址，例如 (0x48 << 1) = 0x90
} TMP102_HandleTypeDef;

// 寄存器地址
#define TMP102_REG_TEMP     0x00
#define TMP102_REG_CONFIG   0x01

// 函数声明
HAL_StatusTypeDef TMP102_Init(TMP102_HandleTypeDef *hdev);
HAL_StatusTypeDef TMP102_IsReady(TMP102_HandleTypeDef *hdev);
float             TMP102_ReadTemperature(TMP102_HandleTypeDef *hdev);

#endif //XM_POWER_KIT_TMP102_H