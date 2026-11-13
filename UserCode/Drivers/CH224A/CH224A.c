/**
  ******************************************************************************
  * @file           : CH224A.c
  * @brief          : CH224A（USB PD 受电端协议芯片）I2C 驱动实现
  * @date           : 2026/11/14
  ******************************************************************************
  * 寄存器与位定义见 CH224A.h 头部注释（依据 CH224 Datasheet V2.0）。
  * 所有对外的读写都会先取 IIC3_MutexHandle（I2C3 与 MCP4725 共用），
  * 内部实现拆成不带锁的 static 版本，避免递归加锁。
  ******************************************************************************
  */

#include "CH224A.h"
#include "i2c.h"
#include "cmsis_os2.h"
#include "os_handles.h"

/* 已探测到的从机地址（0 表示未探测） */
static uint8_t s_addr = 0;

/*=============================== 内部实现（不加锁） ===============================*/
static HAL_StatusTypeDef ch_write_reg(uint8_t addr, uint8_t reg, uint8_t value) {
    return HAL_I2C_Mem_Write(CH224A_IIC_HANDLE, (uint16_t) (addr << 1), reg,
                             CH224A_MEM_ADDR_SIZE, &value, 1, CH224A_IIC_TIMEOUT);
}

static HAL_StatusTypeDef ch_read_reg(uint8_t addr, uint8_t reg, uint8_t *value) {
    return HAL_I2C_Mem_Read(CH224A_IIC_HANDLE, (uint16_t) (addr << 1), reg,
                            CH224A_MEM_ADDR_SIZE, value, 1, CH224A_IIC_TIMEOUT);
}

/* 读一次 0x09：既能拿到状态，也能当作"芯片是否在线"的探针 */
static HAL_StatusTypeDef ch_read_status_raw(uint8_t addr, uint8_t *raw) {
    return ch_read_reg(addr, CH224A_REG_STATUS, raw);
}

static void ch_parse_status(uint8_t raw, CH224A_Status_t *status) {
    status->raw        = raw;
    status->bc12       = (raw & CH224A_ST_BC12)       ? 1U : 0U;
    status->qc2        = (raw & CH224A_ST_QC2)        ? 1U : 0U;
    status->qc3        = (raw & CH224A_ST_QC3)        ? 1U : 0U;
    status->pd         = (raw & CH224A_ST_PD)         ? 1U : 0U;
    status->epr_active = (raw & CH224A_ST_EPR_ACTIVE) ? 1U : 0U;
    status->epr_exist  = (raw & CH224A_ST_EPR_EXIST)  ? 1U : 0U;
    status->avs_exist  = (raw & CH224A_ST_AVS_EXIST)  ? 1U : 0U;
}

/*=============================== 探测 ===============================*/
uint8_t CH224A_Probe(void) {
    uint8_t raw = 0;
    uint8_t found = 0;

    s_addr = 0;

    osMutexAcquire(IIC3_MutexHandle, osWaitForever);

    if (ch_read_status_raw(CH224A_ADDR_PRIMARY, &raw) == HAL_OK) {
        s_addr = CH224A_ADDR_PRIMARY;
        found = 1;
    } else if (ch_read_status_raw(CH224A_ADDR_ALT, &raw) == HAL_OK) {
        s_addr = CH224A_ADDR_ALT;
        found = 1;
    }

    osMutexRelease(IIC3_MutexHandle);
    return found;
}

uint8_t CH224A_Init(void) {
    return CH224A_Probe();
}

uint8_t CH224A_IsPresent(void) {
    return (s_addr != 0U) ? 1U : 0U;
}

uint8_t CH224A_GetAddress(void) {
    return s_addr;
}

/*=============================== 状态与电流 ===============================*/
HAL_StatusTypeDef CH224A_ReadStatus(CH224A_Status_t *status) {
    uint8_t raw = 0;
    HAL_StatusTypeDef ret;

    if ((status == NULL) || (s_addr == 0U)) {
        return HAL_ERROR;
    }

    osMutexAcquire(IIC3_MutexHandle, osWaitForever);
    ret = ch_read_status_raw(s_addr, &raw);
    osMutexRelease(IIC3_MutexHandle);

    if (ret != HAL_OK) {
        s_addr = 0;         // 连续失败当作掉线，下一次由界面负责重新探测
        return HAL_ERROR;
    }

    /* 适配器拔掉后芯片仍在线，但状态位全 0：此时 PD 未握手、0x50 无效，
       界面按 status->raw == 0 显示"未握手"即可 */
    ch_parse_status(raw, status);
    return HAL_OK;
}

HAL_StatusTypeDef CH224A_ReadMaxCurrentMa(uint16_t *current_ma) {
    CH224A_Status_t status;
    uint8_t value = 0;
    HAL_StatusTypeDef ret;

    if ((current_ma == NULL) || (s_addr == 0U)) {
        return HAL_ERROR;
    }

    *current_ma = 0;

    osMutexAcquire(IIC3_MutexHandle, osWaitForever);

    ret = ch_read_status_raw(s_addr, &value);
    if (ret == HAL_OK) {
        ch_parse_status(value, &status);
        if (status.pd == 0U) {
            ret = HAL_ERROR;            // 只有 PD 握手成功后 0x50 才有效
        }
    }
    if (ret == HAL_OK) {
        ret = ch_read_reg(s_addr, CH224A_REG_CURRENT, &value);
    }

    osMutexRelease(IIC3_MutexHandle);

    if (ret != HAL_OK) {
        return HAL_ERROR;
    }

    *current_ma = (uint16_t) value * 50U;   // 单位 50mA
    return HAL_OK;
}

/*=============================== 请求电压 ===============================*/
HAL_StatusTypeDef CH224A_RequestVoltage(CH224A_VoltTypeDef volt) {
    HAL_StatusTypeDef ret;

    if ((s_addr == 0U) || ((uint8_t) volt > (uint8_t) CH224A_VOLT_28V)) {
        return HAL_ERROR;
    }

    osMutexAcquire(IIC3_MutexHandle, osWaitForever);
    ret = ch_write_reg(s_addr, CH224A_REG_VOLT_CTRL, (uint8_t) volt);
    osMutexRelease(IIC3_MutexHandle);

    return ret;
}

const char *CH224A_VoltText(CH224A_VoltTypeDef volt) {
    switch (volt) {
        case CH224A_VOLT_5V:  return "5V";
        case CH224A_VOLT_9V:  return "9V";
        case CH224A_VOLT_12V: return "12V";
        case CH224A_VOLT_15V: return "15V";
        case CH224A_VOLT_20V: return "20V";
        case CH224A_VOLT_28V: return "28V";
        default:              return "--";
    }
}
