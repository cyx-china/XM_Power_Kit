/**
******************************************************************************
  * @file    MCP4725.h
  * @author  Xuemeng
  * @version V1.4.0
  * @date    2026/2/4
  * @brief   MCP4725 DAC 驱动头文件（精简版，支持多设备）
  ******************************************************************************
  */

#ifndef MCP4725_H
#define MCP4725_H

#include "stm32f4xx_hal.h"

#define MCP4725_ADDR 0x60

// 掉电模式定义（与数据手册对应）
typedef enum {
    MCP4725_MODE_NORMAL = 0x00,     // 正常模式（00）
    MCP4725_MODE_1K     = 0x01,     // 掉电模式：1kΩ 到地（01）
    MCP4725_MODE_100K   = 0x02,     // 掉电模式：100kΩ 到地（10）
    MCP4725_MODE_500K   = 0x03      // 掉电模式：500kΩ 到地（11）
} MCP4725_PowerDownModeTypeDef;

// MCP4725 设备句柄（支持多设备）
typedef struct {
    I2C_HandleTypeDef *hi2c;        // I2C 句柄
    uint16_t           dev_addr;    // 7位 I2C 从机地址
} MCP4725_HandleTypeDef;

/**
  * @brief  检查 MCP4725 是否就绪（初始化/存在性检测）
  * @param  hdev: MCP4725 设备句柄
  * @param hi2c: 此设备所在的 I2C总线
  * @param address: 七位地址，无需左移
  * @retval HAL_StatusTypeDef
  */
HAL_StatusTypeDef MCP4725_Init(MCP4725_HandleTypeDef *hdev, I2C_HandleTypeDef *hi2c,uint16_t address);

/**
  * @brief  快速写入 DAC 值（仅更新 DAC 寄存器，不写入 EEPROM）
  * @param  hdev:  MCP4725 设备句柄
  * @param  data:  12 位 DAC 值（0 ~ 4095）
  * @param  mode:  掉电模式
  * @retval HAL_StatusTypeDef
  */
HAL_StatusTypeDef MCP4725_WriteFast(MCP4725_HandleTypeDef *hdev, uint16_t data, MCP4725_PowerDownModeTypeDef mode);

/**
  * @brief  写入 DAC 值并保存到 EEPROM（断电后保持）
  * @param  hdev:  MCP4725 设备句柄
  * @param  data:  12 位 DAC 值（0 ~ 4095）
  * @param  mode:  掉电模式
  * @retval HAL_StatusTypeDef
  * @note   EEPROM 写入典型需要 25ms，已在函数内延时等待
  */
HAL_StatusTypeDef MCP4725_WriteEEPROM(MCP4725_HandleTypeDef *hdev, uint16_t data, MCP4725_PowerDownModeTypeDef mode);

#endif /* MCP4725_H */