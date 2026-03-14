#include "CH224A.h"

#include <stdio.h>
#include <string.h>

#include "i2c.h"
#include "usart.h"

/************************** 基础寄存器操作实现 **************************/

/**
 * @brief  向CH224A单个寄存器写1字节数据
 * @param  reg_addr 目标寄存器地址
 * @param  data 要写入的1字节数据
 * @retval HAL_StatusTypeDef：通信结果（HAL_OK=成功，其他=失败）
 */
HAL_StatusTypeDef CH224A_WriteReg(uint8_t reg_addr, uint8_t data) {
    return HAL_I2C_Mem_Write(
        CH224A_IIC_HANDLE,    // I2C句柄
        CH224A_ADDR << 1,    // 写地址
        reg_addr,             // 寄存器地址
        CH224A_MEM_ADDR_SIZE, // 寄存器地址长度（8位）
        &data,                // 写入数据缓冲区
        1,                    // 数据长度（1字节）
        CH224A_IIC_TIMEOUT    // 超时时间
    );
}

/**
 * @brief  从CH224A单个寄存器读1字节数据
 * @param  reg_addr 目标寄存器地址
 * @param  data 接收数据的缓冲区指针
 * @retval HAL_StatusTypeDef：通信结果（HAL_OK=成功，其他=失败）
 */
HAL_StatusTypeDef CH224A_ReadReg(uint8_t reg_addr, uint8_t *data) {
    return HAL_I2C_Mem_Read(
        CH224A_IIC_HANDLE,    // I2C句柄
        CH224A_ADDR << 1,     // 读地址
        reg_addr,             // 寄存器地址
        CH224A_MEM_ADDR_SIZE, // 寄存器地址长度（8位）
        data,                 // 接收数据缓冲区
        1,                    // 数据长度（1字节）
        CH224A_IIC_TIMEOUT    // 超时时间
    );
}

/**
 * @brief  向CH224A连续多个寄存器写数据
 * @param  start_reg 起始寄存器地址（如CH224A_REG_AVS_L）
 * @param  data_buf 多字节数据缓冲区指针
 * @param  len 数据长度（字节数，最大不超过16）
 * @retval HAL_StatusTypeDef：通信结果（HAL_OK=成功，其他=失败）
 */
HAL_StatusTypeDef CH224A_WriteMultiReg(uint8_t start_reg, uint8_t *data_buf, uint8_t len) {
    return HAL_I2C_Mem_Write(
        CH224A_IIC_HANDLE,    // I2C句柄
        CH224A_ADDR << 1,    // 写地址
        start_reg,            // 起始寄存器地址
        CH224A_MEM_ADDR_SIZE, // 寄存器地址长度（8位）
        data_buf,             // 多字节数据缓冲区
        len,                  // 数据长度
        CH224A_IIC_TIMEOUT    // 超时时间
    );
}

/************************** 核心功能操作实现 **************************/
/**
 * @brief  设置CH224A的电压档位或工作模式
 * @param volt_mode 电压档位/模式（CH224A_VoltModeTypeDef枚举）
 * @retval HAL_StatusTypeDef：配置结果（HAL_OK=成功，其他=失败）
 */
HAL_StatusTypeDef CH224A_SetVoltage(CH224A_VoltModeTypeDef volt_mode) {
    return CH224A_WriteReg(CH224A_REG_VOLT_CTRL, (uint8_t)volt_mode);
}

/**
 * @brief  读取CH224A当前协议握手状态
 * @param  status 协议状态结构体指针
 * @retval HAL_StatusTypeDef：读取结果（HAL_OK=成功，其他=失败）
 */
HAL_StatusTypeDef CH224A_GetProtocolStatus(CH224A_ProtocolStatusTypeDef *status) {
    uint8_t reg_data = 0;
    // 读取状态寄存器（0x09）
    if (CH224A_ReadReg(CH224A_REG_STATUS, &reg_data) != HAL_OK) {
        return HAL_ERROR;
    }

    // 解析寄存器位（对应手册表5-3）
    status->BC12_Active = (reg_data >> 0) & 0x01;
    status->QC2_Active  = (reg_data >> 1) & 0x01;
    status->QC3_Active  = (reg_data >> 2) & 0x01;
    status->PD_Active   = (reg_data >> 3) & 0x01;
    status->EPR_Active  = (reg_data >> 4) & 0x01;
    status->Reserved    = (reg_data >> 5) & 0x07;

    return HAL_OK;
}

/**
 * @brief  读取当前PD档位的最大可用电流
 * @param  current_ma 最大电流值（单位：mA，仅PD协议握手成功后有效）
 * @retval HAL_StatusTypeDef：读取结果（HAL_OK=成功，其他=失败）
 */
HAL_StatusTypeDef CH224A_GetMaxCurrent(uint16_t *current_ma) {
    uint8_t reg_data = 0;
    CH224A_ProtocolStatusTypeDef status;

    // 先检查PD协议是否握手成功（仅PD模式下电流寄存器有效，手册表5-3）
    if (CH224A_GetProtocolStatus(&status) != HAL_OK || status.PD_Active == 0) {
        *current_ma = 0;  // PD未激活时，电流值无效，设为0
        return HAL_ERROR;
    }

    // 读取电流数据寄存器（0x50）
    if (CH224A_ReadReg(CH224A_REG_CURRENT, &reg_data) != HAL_OK) {
        return HAL_ERROR;
    }

    // 计算实际电流（手册说明：寄存器值×50mA）
    *current_ma = reg_data * 50;

    return HAL_OK;
}

/**
 * @brief  配置AVS模式的请求电压（动态调压）
 * @param  avs_volt_mv AVS请求电压（单位：mV，范围5000~30000，精度100mV）
 * @retval HAL_StatusTypeDef：配置结果（HAL_OK=成功，其他=失败）
 */
HAL_StatusTypeDef CH224A_ConfigAVS(uint16_t avs_volt_mv) {
    uint16_t volt_code = 0;  // AVS电压编码（单位：100mV）
    uint8_t avs_data[2] = {0};

    // 1. 检查电压范围合法性（5V~30V）
    if (avs_volt_mv < 5000 || avs_volt_mv > 30000) {
        return HAL_ERROR;
    }

    // 2. 转换电压编码（100mV/步：如18000mV → 180 编码）
    volt_code = avs_volt_mv / 100;

    // 3. 拆分高低位（手册说明：先写低8位，再写高8位+使能位）
    avs_data[0] = volt_code & 0xFF;                    // 低8位（写入0x52寄存器）
    avs_data[1] = ((volt_code >> 8) & 0x7F) | 0x80;    // 高7位 + 使能位（BIT7=1，写入0x51寄存器）

    // 4. 写入AVS配置寄存器（先低8位，后高8位）
    if (CH224A_WriteReg(CH224A_REG_AVS_L, avs_data[0]) != HAL_OK) {
        return HAL_ERROR;
    }
    if (CH224A_WriteReg(CH224A_REG_AVS_H, avs_data[1]) != HAL_OK) {
        return HAL_ERROR;
    }

    // 5. 切换到AVS模式（写入电压控制寄存器）
    return CH224A_SetVoltage(CH224A_MODE_AVS);
}

/**
 * @brief  配置PPS模式的请求电压（动态调压）
 * @param  pps_volt_mv PPS请求电压（单位：mV，范围5000~30000，精度100mV）
 * @retval HAL_StatusTypeDef：配置结果（HAL_OK=成功，其他=失败）
 */
HAL_StatusTypeDef CH224A_ConfigPPS(uint16_t pps_volt_mv) {
    uint8_t pps_code = 0;  // PPS电压编码（单位：100mV）

    // 1. 检查电压范围合法性（5V~30V）
    if (pps_volt_mv < 5000 || pps_volt_mv > 30000) {
        return HAL_ERROR;
    }

    // 2. 转换电压编码（100mV/步：如12000mV → 120 编码）
    pps_code = (uint8_t)(pps_volt_mv / 100);

    // 3. 写入PPS配置寄存器（0x53）
    if (CH224A_WriteReg(CH224A_REG_PPS, pps_code) != HAL_OK) {
        return HAL_ERROR;
    }

    // 4. 切换到PPS模式（写入电压控制寄存器）
    return CH224A_SetVoltage(CH224A_MODE_PPS);
}




