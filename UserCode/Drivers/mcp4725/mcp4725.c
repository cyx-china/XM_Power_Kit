/**
******************************************************************************
  * @file    MCP4725.c
  * @author  Xuemeng
  * @version V1.4.0
  * @date    2026/02/04
  * @brief   MCP4725 DAC 驱动源文件
  ******************************************************************************
  */

#include "cmsis_os2.h"
#include "MCP4725.h"

/**
  * @brief  检查 MCP4725 是否就绪
  */
HAL_StatusTypeDef MCP4725_Init(MCP4725_HandleTypeDef *hdev, I2C_HandleTypeDef *hi2c,uint16_t address)
{
    if (hdev == NULL)
        return HAL_ERROR;
    hdev->hi2c = hi2c;
    hdev->dev_addr = address;

    // 尝试 3 次，超时 100ms
    return HAL_I2C_IsDeviceReady(hdev->hi2c, (hdev->dev_addr << 1), 3, 100);
}

/**
  * @brief  快速写入 DAC 值
  */
HAL_StatusTypeDef MCP4725_WriteFast(MCP4725_HandleTypeDef *hdev, uint16_t data, MCP4725_PowerDownModeTypeDef mode)
{
    if (hdev == NULL || hdev->hi2c == NULL)
        return HAL_ERROR;

    if (data > 4095)
        return HAL_ERROR;

    uint8_t tx_buf[2];

    // 命令字节：bit7-6 = 00, bit5-4 = PD1/PD0, bit3-0 = D11~D8
    tx_buf[0] = (mode << 4) | ((data >> 8) & 0x0F);
    tx_buf[1] = data & 0xFF;

    return HAL_I2C_Master_Transmit(hdev->hi2c, (hdev->dev_addr << 1), tx_buf, 2, 50);
}

/**
  * @brief  写入 DAC 并保存到 EEPROM（3 字节命令）
  */
HAL_StatusTypeDef MCP4725_WriteEEPROM(MCP4725_HandleTypeDef *hdev, uint16_t data, MCP4725_PowerDownModeTypeDef mode)
{
    if (hdev == NULL || hdev->hi2c == NULL)
        return HAL_ERROR;

    if (data > 4095)
        return HAL_ERROR;

    uint8_t tx_buf[3];

    // 第一字节：0x60 | (PD1 PD0 << 3)  →  011 PD1 PD0 0 0 0
    tx_buf[0] = 0x60 | ((mode & 0x03) << 3);
    tx_buf[1] = (data >> 4) & 0xFF;         // D11~D4
    tx_buf[2] = (data << 4) & 0xF0;         // D3~D0 在高 4 位，低 4 位为 0

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(hdev->hi2c, (hdev->dev_addr << 1), tx_buf, 3, 50);

    if (status == HAL_OK)
    {
        osDelay(25);  // 等待 EEPROM 写入完成（典型 25ms）
    }

    return status;
}