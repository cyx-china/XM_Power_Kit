#ifndef __CH224A_H
#define __CH224A_H

#include "stm32f4xx_hal.h"
#include <stdint.h>


/************************** 基础宏定义 **************************/
// 1. I2C 核心参数（用户指定）
#define CH224A_ADDR         0x23        // CH224A 7位I2C地址
#define CH224A_IIC_HANDLE   &hi2c3      // I2C句柄

// 3. I2C 通信配置
#define CH224A_MEM_ADDR_SIZE    I2C_MEMADD_SIZE_8BIT  // 寄存器地址长度：8位
#define CH224A_IIC_TIMEOUT      100                   // 通信超时时间：100ms

// 4. CH224A 关键寄存器地址（手册表5-3）
#define CH224A_REG_STATUS       0x09    // 状态寄存器（只读：协议握手状态）
#define CH224A_REG_VOLT_CTRL    0x0A    // 电压控制寄存器（只写：切换电压档位/模式）
#define CH224A_REG_CURRENT      0x50    // 电流数据寄存器（只读：当前PD档位最大电流）
#define CH224A_REG_AVS_H        0x51    // AVS电压配置寄存器（高8位：只写）
#define CH224A_REG_AVS_L        0x52    // AVS电压配置寄存器（低8位：只写）
#define CH224A_REG_PPS          0x53    // PPS电压配置寄存器（只写）

/************************** 枚举与结构体定义 **************************/
// 1. 电压档位/模式枚举（对应手册表5-2及0x0A寄存器说明）
typedef enum {
    CH224A_VOLT_5V      = 0x00,  // 5V 固定档位
    CH224A_VOLT_9V      = 0x01,  // 9V 固定档位
    CH224A_VOLT_12V     = 0x02,  // 12V 固定档位
    CH224A_VOLT_15V     = 0x03,  // 15V 固定档位
    CH224A_VOLT_20V     = 0x04,  // 20V 固定档位
    CH224A_VOLT_28V     = 0x05,  // 28V 固定档位
    CH224A_MODE_PPS     = 0x06,  // PPS 动态调压模式
    CH224A_MODE_AVS     = 0x07   // AVS 动态调压模式
} CH224A_VoltModeTypeDef;

// 2. 协议状态结构体
typedef struct {
    uint8_t BC12_Active  : 1;  // BIT0：BC1.2协议握手成功（1=成功）
    uint8_t QC2_Active   : 1;  // BIT1：QC2.0协议握手成功（1=成功）
    uint8_t QC3_Active   : 1;  // BIT2：QC3.0协议握手成功（1=成功）
    uint8_t PD_Active    : 1;  // BIT3：PD协议握手成功（1=成功）
    uint8_t EPR_Active   : 1;  // BIT4：EPR模式激活（1=激活，仅28V档位有效）
    uint8_t Reserved     : 3;  // BIT5~BIT7：保留位（无意义）
} CH224A_ProtocolStatusTypeDef;


/************************** 函数声明 **************************/
// 1. 基础寄存器操作
HAL_StatusTypeDef CH224A_WriteReg(uint8_t reg_addr, uint8_t data);  // 单寄存器写1字节
HAL_StatusTypeDef CH224A_ReadReg(uint8_t reg_addr, uint8_t *data);   // 单寄存器读1字节
HAL_StatusTypeDef CH224A_WriteMultiReg(uint8_t start_reg, uint8_t *data_buf, uint8_t len);  // 多寄存器连续写

// 2. 核心功能操作
HAL_StatusTypeDef CH224A_SetVoltage(CH224A_VoltModeTypeDef volt_mode);  // 设置电压档位/模式
HAL_StatusTypeDef CH224A_GetProtocolStatus(CH224A_ProtocolStatusTypeDef *status);  // 读取协议状态
HAL_StatusTypeDef CH224A_GetMaxCurrent(uint16_t *current_ma);  // 读取当前PD档位最大电流（单位：mA）
HAL_StatusTypeDef CH224A_ConfigAVS(uint16_t avs_volt_mv);  // 配置AVS模式电压（单位：mV，5000~30000）
HAL_StatusTypeDef CH224A_ConfigPPS(uint16_t pps_volt_mv);  // 配置PPS模式电压（单位：mV，5000~30000）

#endif  // __CH224A_H