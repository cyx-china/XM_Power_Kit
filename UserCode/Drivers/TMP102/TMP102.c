#include "TMP102.h"
#include <math.h>

// 读寄存器
static HAL_StatusTypeDef TMP102_ReadRegs(TMP102_HandleTypeDef *hdev, uint8_t reg, uint8_t *buf, uint8_t len)
{
    return HAL_I2C_Mem_Read(hdev->hi2c,
                             hdev->dev_addr,
                             reg,
                             I2C_MEMADD_SIZE_8BIT,
                             buf,
                             len,
                             80);
}

// 写寄存器
static HAL_StatusTypeDef TMP102_WriteRegs(TMP102_HandleTypeDef *hdev, uint8_t reg, const uint8_t *data, uint8_t len)
{
    return HAL_I2C_Mem_Write(hdev->hi2c,
                              hdev->dev_addr,
                              reg,
                              I2C_MEMADD_SIZE_8BIT,
                              (uint8_t*)data,
                              len,
                              80);
}

// 检查设备是否响应
HAL_StatusTypeDef TMP102_IsReady(TMP102_HandleTypeDef *hdev)
{
    return HAL_I2C_IsDeviceReady(hdev->hi2c, hdev->dev_addr, 2, 50);
}

// 初始化：强制写入配置 0x60C0（8 conv/s，连续模式）
HAL_StatusTypeDef TMP102_Init(TMP102_HandleTypeDef *hdev)
{
    // 先确认设备存在
    if (TMP102_IsReady(hdev) != HAL_OK)
    {
        return HAL_ERROR;
    }

    // 直接写入 0x60C0
    uint8_t config[2] = {0x60, 0xC0};
    if (TMP102_WriteRegs(hdev, TMP102_REG_CONFIG, config, 2) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

// 读取当前温度（℃）
float TMP102_ReadTemperature(TMP102_HandleTypeDef *hdev)
{
    uint8_t buf[2];

    if (TMP102_ReadRegs(hdev, TMP102_REG_TEMP, buf, 2) != HAL_OK)
    {
        return 0.0f;
    }

    int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
    raw >>= 4;

    return (float)raw * 0.0625f;
}

