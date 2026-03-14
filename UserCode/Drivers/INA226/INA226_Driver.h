/******************************************************************************
* @file    INA226_Driver.h
* @author  Xuemeng
* @version V1.0.0
* @date    2025/4/9
* @brief   INA226 电流/电压监测芯片驱动头文件
******************************************************************************/
#ifndef INA226_DRIVER_H
#define INA226_DRIVER_H

#include <stdint.h>
#include "usart.h"

// 宏定义寄存器地址
#define INPUT_ADDR                  0x40	// 输入端 A0=GND，A1=GND

#define INA226_CONFIG_REG     		0x00  	// 配置寄存器
#define INA226_SHUNT_VOLTAGE_REG    0x01 	// 分流电压寄存器
#define INA226_BUS_VOLTAGE_REG     	0x02 	// 总线电压寄存器
#define INA226_POWER_REG            0x03 	// 功率寄存器
#define INA226_CURRENT_REG          0x04 	// 电流寄存器
#define INA226_CALIB_REG      		0x05  	// 校准寄存器
#define INA226_MASK_ENABLE_REG      0x06  	// 掩码/使能寄存器
#define INA226_ALERT_REG      		0x07 	// 告警寄存器
#define INA226_MANUF_ID_REG   		0xFE 	// 制造商ID寄存器
#define INA226_DIE_ID_REG     		0xFF 	// 芯片ID寄存器

// 配置寄存器定义
#define Conf_REG_Data         0x4497  // 配置寄存器值 (1.1ms转换时间，16次平均，连续模式)

// 寄存器计算结构体定义
typedef struct {
    I2C_HandleTypeDef* hi2c;    // I2C总线
    uint8_t address;            // 设备地址
    float current_lsb;          // 电流LSB
    float power_lsb;            // 功率LSB
    uint16_t calibration;       // 校准寄存器值
} INA226_HandleTypeDef;

// 函数声明
HAL_StatusTypeDef INA226_Init(INA226_HandleTypeDef* dev,I2C_HandleTypeDef* hi2c, uint8_t address, float max_current, float shunt_resistance);
float INA226_ReadBusVoltage(INA226_HandleTypeDef* dev);
float INA226_ReadShuntVoltage(INA226_HandleTypeDef* dev);
float INA226_ReadCurrent(INA226_HandleTypeDef* dev);
float INA226_ReadPower(INA226_HandleTypeDef* dev);
uint8_t INA226_CheckOverflow(INA226_HandleTypeDef* dev);
void PrintINA226Data(INA226_HandleTypeDef* dev, UART_HandleTypeDef* huart);
uint16_t INA226_ReadRegister(INA226_HandleTypeDef* dev , uint16_t REG_ADDR);

#endif