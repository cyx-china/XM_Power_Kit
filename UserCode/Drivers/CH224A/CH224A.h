#ifndef __CH224A_H
#define __CH224A_H

/**
  ******************************************************************************
  * @file           : CH224A.h
  * @brief          : CH224A（USB PD 受电端协议芯片）I2C 驱动
  * @date           : 2026/11/14
  ******************************************************************************
  * @attention
  *
  * 依据《CH224 Datasheet》V2.0（南京沁恒 WCH）第 5.2.3 节与表 5-3 重写：
  *
  *  · 7 位 I2C 地址为 0x22 或 0x23（不含读写位），本驱动上电时自动探测；
  *  · **CH224A 只支持单字节读/写**，不能连写；PPS(0x53)/AVS(0x51/0x52) 与
  *    PD 功率数据区(0x60~0x8F) 只有 CH224Q 才支持，CH224A 上调用无意义；
  *  · 用单电阻配置档位时 I2C 功能自动使能，此时可以用 I2C 改请求电压并读回状态；
  *  · 寄存器 0x0A 是"请求电压"：0=5V 1=9V 2=12V 3=15V 4=20V 5=28V；
  *    28V 属于 EPR（PD3.1），需要适配器支持 EPR 且用了带 eMarker 的 Type-C 公头；
  *  · 寄存器 0x50 是"当前档位最大可用电流"，单位 50mA，只在 PD 握手成功时有效。
  *
  * I2C3 与 MCP4725(DC 输出) 共用，驱动内部会取 IIC3_MutexHandle（不可递归：
  * 公开函数之间不要互相调用后再调用，内部实现已拆成不带锁的 static 版本）。
  ******************************************************************************
  */

#include "stm32f4xx_hal.h"
#include <stdint.h>

/*=============================== 基础配置 ===============================*/
#define CH224A_ADDR_PRIMARY     0x22            // 7 位地址（不含读写位）
#define CH224A_ADDR_ALT         0x23            // 7 位地址备选（CH224A/Q 均可能）
#define CH224A_IIC_HANDLE       (&hi2c3)        // 挂在 I2C3 上
#define CH224A_MEM_ADDR_SIZE    I2C_MEMADD_SIZE_8BIT
#define CH224A_IIC_TIMEOUT      20              // ms，PD 芯片响应很快，不要挂住总线
#define CH224A_MUTEX_TIMEOUT    100             // ms，取 I2C3 互斥锁的超时

/*=============================== 寄存器地址 ===============================*/
#define CH224A_REG_STATUS       0x09            // 协议状态（只读）
#define CH224A_REG_VOLT_CTRL    0x0A            // 请求电压（只写）
#define CH224A_REG_CURRENT      0x50            // 当前档位最大电流（只读，×50mA）

/*=============================== 状态位 ===============================*/
#define CH224A_ST_BC12          0x01U           // BIT0 BC1.2 握手成功
#define CH224A_ST_QC2           0x02U           // BIT1 QC2.0 握手成功
#define CH224A_ST_QC3           0x04U           // BIT2 QC3.0 握手成功
#define CH224A_ST_PD            0x08U           // BIT3 PD 握手成功
#define CH224A_ST_EPR_ACTIVE    0x10U           // BIT4 EPR 已激活（28V 档）
#define CH224A_ST_EPR_EXIST     0x20U           // BIT5 电源支持 EPR（>100W 档）
#define CH224A_ST_AVS_EXIST     0x40U           // BIT6 电源支持 AVS

/*=============================== 类型定义 ===============================*/
// 请求电压档位（对应 0x0A 寄存器取值）
typedef enum {
    CH224A_VOLT_5V  = 0,
    CH224A_VOLT_9V  = 1,
    CH224A_VOLT_12V = 2,
    CH224A_VOLT_15V = 3,
    CH224A_VOLT_20V = 4,
    CH224A_VOLT_28V = 5
} CH224A_VoltTypeDef;

// 协议状态（寄存器的解析结果）
typedef struct {
    uint8_t raw;            // 0x09 原始值
    uint8_t bc12;           // BC1.2
    uint8_t qc2;            // QC2.0
    uint8_t qc3;            // QC3.0
    uint8_t pd;             // PD（含 PD3.0）
    uint8_t epr_active;     // EPR 已激活（28V）
    uint8_t epr_exist;      // 电源支持 EPR
    uint8_t avs_exist;      // 电源支持 AVS
} CH224A_Status_t;

/*=============================== 函数声明 ===============================*/
/* 上电探测：依次试 0x22 / 0x23，找到就记住地址并返回 1，否则返回 0 */
uint8_t CH224A_Init(void);
/* 是否已经探测到芯片（0 = 不在线） */
uint8_t CH224A_IsPresent(void);
/* 已探测到的 7 位地址（0 = 未探测） */
uint8_t CH224A_GetAddress(void);
/* 重新探测（热插拔适配器后调用） */
uint8_t CH224A_Probe(void);

/* 读协议状态；返回 HAL_OK 表示读取成功 */
HAL_StatusTypeDef CH224A_ReadStatus(CH224A_Status_t *status);
/* 读当前档位最大可用电流（mA），仅 PD 握手成功后有效；PD 未握手返回 HAL_ERROR */
HAL_StatusTypeDef CH224A_ReadMaxCurrentMa(uint16_t *current_ma);
/* 请求某个电压档位（写入 0x0A，芯片会重新和适配器协商） */
HAL_StatusTypeDef CH224A_RequestVoltage(CH224A_VoltTypeDef volt);
/* 档位对应的字符串（"5V"、"20V" ...） */
const char *CH224A_VoltText(CH224A_VoltTypeDef volt);

#endif  // __CH224A_H
