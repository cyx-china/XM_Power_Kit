/******************************************************************************
* @file    INA226_Driver.c
* @author  Xuemeng
* @version V1.3.0
* @date    2025/5/13
* @brief   INA226 电流/电压监测芯片驱动文件（核心功能版）
******************************************************************************/

#include "INA226_Driver.h"
#include "format.h"
#include <stdio.h>
#include <string.h>
#include "i2c.h"

#define INA225_Debug_Mode 0         // 0: 关闭调试，1：开启调试

/**
 * @brief  计算INA226校准参数
 * @param  dev: 设备句柄
 * @param  max_current: 最大电流（A）
 * @param  shunt_resistance: 分流电阻（Ω）
 */
static void INA226_CalculateCalibration(INA226_HandleTypeDef* dev, float max_current, float shunt_resistance) {
    dev->current_lsb = max_current / 32768.0f;                              // 计算电流lsb
    float cal_value = 0.00512f / (dev->current_lsb * shunt_resistance);     // 计算校准寄存器值

    cal_value = (float)((int)cal_value);
    if (cal_value < 1.0f) cal_value = 1.0f;
    if (cal_value > 65535.0f) cal_value = 65535.0f;
    dev->calibration = (uint16_t)cal_value;

    dev->power_lsb = 25.0f * dev->current_lsb;                              // 功率lsb
}

/**
 * @brief  初始化INA226芯片
 * @param  hi2c: I2C句柄
 * @param  address: 设备地址
 * @param  max_current: 最大电流（A）
 * @param  shunt_resistance: 分流电阻（Ω）
 * @retval 0（固定返回成功）
 */
HAL_StatusTypeDef INA226_Init(INA226_HandleTypeDef* dev,I2C_HandleTypeDef* hi2c, uint8_t address, float max_current, float shunt_resistance) {
    dev->hi2c = hi2c;
    dev->address = address;

    // 检查INA226有没有回应
    if (HAL_I2C_IsDeviceReady(hi2c,address << 1,3,100) != HAL_OK) {
        return HAL_ERROR;  // 设备无响应
    }

    INA226_CalculateCalibration(dev, max_current, shunt_resistance);

    // 配置寄存器写入
    uint8_t config_data_bytes[2] = {
        (Conf_REG_Data >> 8) & 0xFF,
        Conf_REG_Data & 0xFF
    };
    HAL_I2C_Mem_Write(
        dev->hi2c, dev->address << 1, INA226_CONFIG_REG,
        I2C_MEMADD_SIZE_8BIT, config_data_bytes, 2, 100
    );

    // 校准寄存器写入
    uint8_t calibration_data_bytes[2] = {
        (dev->calibration >> 8) & 0xFF,
        dev->calibration & 0xFF
    };
    HAL_I2C_Mem_Write(
        dev->hi2c, dev->address << 1, INA226_CALIB_REG,
        I2C_MEMADD_SIZE_8BIT, calibration_data_bytes, 2, 100
    );

    return HAL_OK;
}

/**
 * @brief  读取总线电压
 * @param  dev: 设备句柄
 * @retval 总线电压（V）
 */
float INA226_ReadBusVoltage(INA226_HandleTypeDef* dev) {
    uint8_t data[2];
    HAL_I2C_Mem_Read(
        dev->hi2c, dev->address << 1, INA226_BUS_VOLTAGE_REG,
        I2C_MEMADD_SIZE_8BIT, data, 2, 100
    );

    uint16_t raw_value = (data[0] << 8) | data[1];
    return (float)raw_value * 0.00125f;  // 电压转换系数
}

/**
 * @brief  一次事务读取总线电压和电流
 * @note   INA226 支持寄存器自增读，总线电压(0x02)/功率(0x03)/电流(0x04) 是连续地址，
 *         因此 6 字节一次读回即可同时得到电压和电流，省掉一次独立的 I2C 事务。
 * @param  dev: 设备句柄
 * @param  voltage: 输出总线电压（V）
 * @param  current: 输出电流（A）
 * @retval 无（读失败时输出 0）
 */
void INA226_ReadBusVoltageAndCurrent(INA226_HandleTypeDef* dev, float* voltage, float* current) {
    uint8_t data[6];

    if (HAL_I2C_Mem_Read(dev->hi2c, dev->address << 1, INA226_BUS_VOLTAGE_REG,
                         I2C_MEMADD_SIZE_8BIT, data, 6, 100) != HAL_OK) {
        *voltage = 0.0f;
        *current = 0.0f;
        return;
    }

    uint16_t raw_voltage = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);   // 0x02 总线电压
    // data[2]/data[3] 是 0x03 功率寄存器，本函数不需要
    int16_t  raw_current = (int16_t)(((uint16_t)data[4] << 8) | data[5]);    // 0x04 电流

    *voltage = (float)raw_voltage * 0.00125f;
    *current = (float)raw_current * dev->current_lsb;
}

/**
 * @brief  读取分流电压
 * @param  dev: 设备句柄
 * @retval 分流电压（V）
 */
float INA226_ReadShuntVoltage(INA226_HandleTypeDef* dev) {
    uint8_t data[2];
    HAL_I2C_Mem_Read(
        dev->hi2c, dev->address << 1, INA226_SHUNT_VOLTAGE_REG,
        I2C_MEMADD_SIZE_8BIT, data, 2, 100
    );

    int16_t raw_value = (int16_t)((data[0] << 8) | data[1]);
    return (float)raw_value * 0.0000025f;  // 分流电压转换系数
}

/**
 * @brief  读取电流值
 * @param  dev: 设备句柄
 * @retval 电流（A）
 */
float INA226_ReadCurrent(INA226_HandleTypeDef* dev) {
    uint8_t data[2];
    HAL_I2C_Mem_Read(
        dev->hi2c, dev->address << 1, INA226_CURRENT_REG,
        I2C_MEMADD_SIZE_8BIT, data, 2, 100
    );

    int16_t raw_value = (int16_t)((data[0] << 8) | data[1]);
    return (float)raw_value * dev->current_lsb;  // 电流转换（基于LSB）
}

/**
 * @brief  读取功率值
 * @param  dev: 设备句柄
 * @retval 功率（W）
 */
float INA226_ReadPower(INA226_HandleTypeDef* dev) {
    uint8_t data[2];
    HAL_I2C_Mem_Read(
        dev->hi2c, dev->address << 1, INA226_POWER_REG,
        I2C_MEMADD_SIZE_8BIT, data, 2, 100
    );

    uint16_t raw_value = (data[0] << 8) | data[1];
    return (float)raw_value * dev->power_lsb;  // 功率转换（基于LSB）
}

/**
 * @brief  检查溢出状态
 * @param  dev: 设备句柄
 * @retval 1=溢出，0=正常
 */
uint8_t INA226_CheckOverflow(INA226_HandleTypeDef* dev) {
    uint8_t data[2];
    HAL_I2C_Mem_Read(
        dev->hi2c, dev->address << 1, INA226_MASK_ENABLE_REG,
        I2C_MEMADD_SIZE_8BIT, data, 2, 100
    );

    return (data[0] & 0x80) ? 1 : 0;  // 溢出标志位判断
}

/**
 * @brief  读取寄存器原始值
 * @param  dev: 设备句柄
 * @param  REG_ADDR: 寄存器地址
 * @retval 寄存器原始值
 */
uint16_t INA226_ReadRegister(INA226_HandleTypeDef* dev , uint16_t REG_ADDR) {
    uint8_t data[2];
    HAL_I2C_Mem_Read(
        dev->hi2c, dev->address << 1, REG_ADDR,
        I2C_MEMADD_SIZE_8BIT, data, 2, 100
    );

    return (data[0] << 8) | data[1];
}

#if INA226_Debug_Mode

/**
 * @brief  打印INA226数据到串口
 * @param  dev: 设备句柄
 * @param  huart: 串口句柄
 */
void PrintINA226Data(INA226_HandleTypeDef* dev, UART_HandleTypeDef* huart) {
    char buffer[512];

    float bus_voltage = INA226_ReadBusVoltage(dev);
    float shunt_voltage = INA226_ReadShuntVoltage(dev);
    float current = INA226_ReadCurrent(dev);
    float power = INA226_ReadPower(dev);
    uint8_t overflow = INA226_CheckOverflow(dev);

    uint16_t raw_bus_voltage = INA226_ReadRegister(dev, INA226_BUS_VOLTAGE_REG);
    uint16_t raw_shunt_voltage = INA226_ReadRegister(dev, INA226_SHUNT_VOLTAGE_REG);
    uint16_t raw_current = INA226_ReadRegister(dev, INA226_CURRENT_REG);
    uint16_t raw_power = INA226_ReadRegister(dev, INA226_POWER_REG);
    uint16_t raw_config = INA226_ReadRegister(dev, INA226_CONFIG_REG);
    uint16_t raw_calibration = INA226_ReadRegister(dev, INA226_CALIB_REG);
    uint16_t raw_mask_enable = INA226_ReadRegister(dev, INA226_MASK_ENABLE_REG);

    // 定点格式化（不用 %f，避免引入 _printf_float 整套浮点打印代码）
    char bus_v_str[12], shunt_v_str[12], cur_str[12], pwr_str[12];
    fmt_fixed(bus_v_str, sizeof(bus_v_str), bus_voltage, 3, 0, 0);
    fmt_fixed(shunt_v_str, sizeof(shunt_v_str), shunt_voltage, 6, 0, 0);
    fmt_fixed(cur_str, sizeof(cur_str), current, 3, 0, 0);
    fmt_fixed(pwr_str, sizeof(pwr_str), power, 3, 0, 0);

    sprintf(buffer,
        "INA226 Data:\r\n"
        "--------------------------------\r\n"
        "Bus Voltage:    %s V  (Raw: 0x%04X @ 0x%02X)\r\n"
        "Shunt Voltage:  %s V  (Raw: 0x%04X @ 0x%02X)\r\n"
        "Current:        %s A  (Raw: 0x%04X @ 0x%02X)\r\n"
        "Power:          %s W  (Raw: 0x%04X @ 0x%02X)\r\n"
        "Overflow:       %s\r\n"
        "\r\n"
        "Configuration:  0x%04X @ 0x%02X\r\n"
        "Calibration:    0x%04X @ 0x%02X\r\n"
        "Mask/Enable:    0x%04X @ 0x%02X\r\n"
        "--------------------------------\r\n\r\n",

        bus_v_str,       raw_bus_voltage,     INA226_BUS_VOLTAGE_REG,
        shunt_v_str,     raw_shunt_voltage,   INA226_SHUNT_VOLTAGE_REG,
        cur_str,         raw_current,         INA226_CURRENT_REG,
        pwr_str,         raw_power,           INA226_POWER_REG,
        overflow ? "YES" : "NO",
        raw_config,      INA226_CONFIG_REG,
        raw_calibration, INA226_CALIB_REG,
        raw_mask_enable, INA226_MASK_ENABLE_REG
    );

    HAL_UART_Transmit(huart, (uint8_t*)buffer, strlen(buffer), 1000);
}
#endif
