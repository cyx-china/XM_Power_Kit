/**
******************************************************************************
  * @file           : Flash.h
  * @brief          : Header for Flash.c file.
  * @date           : 2026/1/28
  * @license        : CC-BY-NC-SA 4.0
  * @note           : 支持 W25Q32 ~ W25Q512 全系列芯片
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  *
  * This FLASH driver is independently developed by the author.
  * It is released under the CC-BY-NC-SA 4.0 open sourse license.
  * And the author's right of attribution is reserved.
  ******************************************************************************
  */
#ifndef FLASH_H_
#define FLASH_H_
#include "stm32f4xx.h"
#include <stdbool.h>

//------------------------- 固定参数配置 ------------------------ //
// 不同容量芯片的ID
#define ID_Q512       0XEF21
#define ID_Q256       0XEF19
#define ID_Q128       0XEF18
#define ID_Q64        0XEF17
#define ID_Q32        0XEF16

/* 超时时间枚举 */
typedef enum {
  TIMEOUT_SPI = 500,                  // SPI通信超时时间 (500ms)
  TIMEOUT_ERASE_SECTOR = 200,         // 扇区擦除超时时间 (200ms)
  TIMEOUT_ERASE_CHIP = 100000,        // 全盘擦除超时时间 (100000ms = 100s)
  TIMEOUT_PAGE_PROGRAM = 3            // 页编程超时时间 (3ms)
} Flash_Timeout;

/* 硬件接口配置 */
#define FLASH_CS_PORT        GPIOD
#define FLASH_CS_PIN         GPIO_PIN_6
#define FLASH_SPI_HANDLE     hspi1

/* 片选控制宏 */
#define FLASH_CS_LOW()       FLASH_CS_PORT->BSRR = (uint32_t)FLASH_CS_PIN << 16;  // 直接操作寄存器，高效
#define FLASH_CS_HIGH()      FLASH_CS_PORT->BSRR = (uint32_t)FLASH_CS_PIN;

/* 通用存储参数 */
#define FLASH_PAGE_SIZE      256         // 页大小：256字节
#define FLASH_SECTOR_SIZE    4096        // 扇区大小：4KB
#define FLASH_BLOCK_SIZE     65536       // 块大小：64KB

/* 通用指令集 */
#define FLASH_CMD_READ_ID        	    0x90    	// 读ID
#define FLASH_CMD_JEDEC_ID       	    0x9F    	// 读JEDEC ID
#define FLASH_CMD_READ_STATUS1   	    0x05    	// 读状态寄存器1
#define FLASH_CMD_WRITE_ENABLE   	    0x06    	// 写使能
#define FLASH_CMD_WRITE_DISABLE  	    0x04    	// 写禁止
#define FLASH_CMD_CHIP_ERASE     	    0xC7    	// 整片擦除
#define FLASH_CMD_ENABLE_4BYTE   	    0xB7    	// 使能4字节地址
#define FLASH_CMD_DISABLE_4BYTE  	    0xE9    	// 禁用4字节地址
#define FLASH_CMD_WAKE_UP        	    0xAB    	// 唤醒

/* 3字节地址模式专用指令集 */
#define FLASH_CMD_READ_DATA_3B        0x03          // 读数据（3字节地址）
#define FLASH_CMD_PAGE_PROGRAM_3B     0x02          // 页编程（3字节地址）
#define FLASH_CMD_SECTOR_ERASE_3B     0x20          // 扇区擦除（3字节地址）
#define FLASH_CMD_BLOCK_ERASE_64K_3B  0xD8          // 块擦除（3字节地址）

/* 4字节地址模式专用指令集 */
#define FLASH_CMD_PAGE_PROGRAM_4B 	  0x12   		// 页编程（4字节地址）
#define FLASH_CMD_READ_DATA_4B   	  0x13    	    // 读数据（4字节地址）
#define FLASH_CMD_SECTOR_ERASE_4B 	  0x21   		// 扇区擦除（4字节地址）
#define FLASH_CMD_BLOCK_ERASE_64K_4B  0xDC 		    // 块擦除（4字节地址）

/* 支持的芯片型号枚举 */
typedef enum {
    W25Q_Model_Unknown = 0,
    W25Q_Model_W25Q32,
    W25Q_Model_W25Q64,
    W25Q_Model_W25Q128,
    W25Q_Model_W25Q256,
    W25Q_Model_W25Q512
} W25Q_ModelTypeDef;

/* -------------------------- 动态全局参数声明 -------------------------- */
// 芯片存储核心参数（初始化时根据ID自动配置）
extern uint32_t FLASH_TOTAL_SIZE;     // 总容量（字节）
extern uint32_t FLASH_MAX_ADDR;       // 最大地址
extern uint32_t FLASH_SECTOR_COUNT;   // 总扇区数
extern uint32_t FLASH_BLOCK_COUNT;    // 总块数
extern uint32_t FLASH_PAGE_COUNT;     // 总页数

// 芯片识别信息
extern uint16_t FLASH_ChipID;         // 芯片设备ID(90h读取)
extern uint32_t FLASH_JEDEC_ID;       // 芯片JEDEC ID(9Fh读取)
extern W25Q_ModelTypeDef FLASH_ChipModel; // 当前芯片型号

// 芯片型号与地址模式标识
extern bool FLASH_Is4ByteAddrMode;    // 当前是否为4字节地址模式

/* -------------------------- 函数声明 -------------------------- */
/**
 * @name FLASH_Init
 * @brief FLASH芯片初始化（多型号兼容版：W25Q32~W25Q512）
 * @param 无
 * @retval HAL_StatusTypeDef: HAL_OK(初始化成功), HAL_ERROR(初始化失败/不支持的芯片)
 * @note 所有FLASH操作前必须先调用此函数完成初始化
 */
HAL_StatusTypeDef FLASH_Init(void);

/**
 * @name FLASH_ReadID
 * @brief 读取FLASH芯片的设备ID（2字节）
 * @param 无
 * @retval uint16_t: 成功返回设备ID值，失败返回0xFF
 */
uint16_t FLASH_ReadID(void);

/**
 * @name FLASH_ReadJEDECID
 * @brief 读取FLASH芯片的JEDEC ID（3字节，厂商+型号+容量）
 * @param 无
 * @retval uint32_t: 成功返回JEDEC ID值，失败返回0xFF
 */
uint32_t FLASH_ReadJEDECID(void);

/**
 * @name FLASH_EraseSector
 * @brief 擦除指定地址所在的4KB扇区（自动适配3/4字节地址模式）
 * @param addr: 扇区任意地址 （自动对齐）
 * @retval HAL_StatusTypeDef:
 *         HAL_OK(擦除成功), HAL_ERROR(地址非法), HAL_TIMEOUT(擦除超时)
 */
HAL_StatusTypeDef FLASH_EraseSector(uint32_t addr);

/**
 * @name FLASH_EraseChip
 * @brief 擦除整个FLASH芯片（所有存储区域，全系列通用）
 * @param 无
 * @retval HAL_StatusTypeDef:
 *         HAL_OK(擦除成功), HAL_TIMEOUT(擦除超时)
 */
HAL_StatusTypeDef FLASH_EraseChip(void);

/**
 * @name FLASH_Read
 * @brief 从指定地址读取任意长度数据到缓冲区（自动适配3/4字节地址模式）
 * @param addr: 读取起始地址
 * @param buffer: 数据接收缓冲区
 * @param length: 读取数据长度（字节）
 * @retval HAL_StatusTypeDef:
 *         HAL_OK(读取成功), HAL_ERROR(参数非法/读取失败)
 */
HAL_StatusTypeDef FLASH_Read(uint32_t addr, uint8_t* buffer, uint32_t length);

/**
 * @name FLASH_WritePage
 * @brief 页编程（向指定地址写入最多256字节数据，不带自动擦除，自动适配3/4字节地址模式）
 * @param addr: 写入起始地址
 * @param data: 待写入数据缓冲区
 * @param len: 写入数据长度（最大256字节，超出会截断）
 * @retval HAL_StatusTypeDef:
 *         HAL_OK(写入成功), HAL_ERROR(参数非法), HAL_TIMEOUT(写入超时)
 */
HAL_StatusTypeDef FLASH_WritePage(uint32_t addr, const uint8_t *data, uint32_t len);

/**
 * @name FLASH_Write
 * @brief 从指定地址写入任意长度数据（自动擦除扇区，保留扇区内未覆盖数据，自动适配3/4字节地址模式）
 * @param addr: 写入起始地址
 * @param data: 待写入数据缓冲区
 * @param len: 写入数据长度（字节）
 * @retval HAL_StatusTypeDef:
 *         HAL_OK(写入成功), HAL_ERROR(参数非法), HAL_TIMEOUT(操作超时)
 */
HAL_StatusTypeDef FLASH_Write(uint32_t addr, const uint8_t *data, const uint32_t len);

#endif /* FLASH_H_ */