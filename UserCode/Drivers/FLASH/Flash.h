/**
******************************************************************************
  * @file           : Flash.h
  * @brief          : Header for Flash.c file.
  * @date           : 2026/1/28
  * @license        : CC-BY-NC-SA 4.0
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

/* W25Q256存储参数（32MB） */
#define FLASH_PAGE_SIZE      256         // 页大小：256字节
#define FLASH_SECTOR_SIZE    4096        // 扇区大小：4KB
#define FLASH_BLOCK_SIZE     65536       // 块大小：64KB
#define FLASH_TOTAL_SIZE     0x2000000   // 总容量：32MB (0x2000000 = 32*1024*1024)
#define FLASH_MAX_ADDR       (FLASH_TOTAL_SIZE - 1)  // 最大地址：0x1FFFFFFF

#define FLASH_SECTOR_COUNT   (FLASH_TOTAL_SIZE / FLASH_SECTOR_SIZE)  // 8192个扇区
#define FLASH_BLOCK_COUNT    (FLASH_TOTAL_SIZE / FLASH_BLOCK_SIZE)    // 512个块
#define FLASH_PAGE_COUNT     (FLASH_TOTAL_SIZE / FLASH_PAGE_SIZE)     // 131072个页

/* 指令集 */
#define FLASH_CMD_READ_ID        	    0x90    	// 读ID
#define FLASH_CMD_JEDEC_ID       	    0x9F    	// 读JEDEC ID
#define FLASH_CMD_READ_STATUS1   	    0x05    	// 读状态寄存器1
#define FLASH_CMD_WRITE_ENABLE   	    0x06    	// 写使能
#define FLASH_CMD_WRITE_DISABLE  	    0x04    	// 写禁止
#define FLASH_CMD_PAGE_PROGRAM_4B 	  0x12   		// 页编程（4字节地址）
#define FLASH_CMD_READ_DATA_4B   	    0x13    	// 读数据（4字节地址）
#define FLASH_CMD_SECTOR_ERASE_4B 	  0x21   		// 扇区擦除（4字节地址）
#define FLASH_CMD_BLOCK_ERASE_64K_4B  0xDC 		  // 块擦除（4字节地址）
#define FLASH_CMD_CHIP_ERASE     	    0xC7    	// 整片擦除
#define FLASH_CMD_ENABLE_4BYTE   	    0xB7    	// 使能4字节地址
#define FLASH_CMD_DISABLE_4BYTE  	    0xE9    	// 禁用4字节地址
#define FLASH_CMD_WAKE_UP        	    0xAB    	// 唤醒

/* ID */
#define W25Q256_ID                    0xEF18    // Q256的ID
#define W25Q256_JEDEC_ID              0xEF4019  // Q256的JEDEC_ID

/* -------------------------- 函数声明 -------------------------- */

/**
 * @name FLASH_Init
 * @brief FLASH芯片初始化（唤醒+使能4字节地址）
 * @param 无
 * @retval HAL_StatusTypeDef: HAL_OK(初始化成功), HAL_ERROR(初始化失败)
 * @note 所有FLASH操作前必须先调用此函数完成初始化
 */
HAL_StatusTypeDef FLASH_Init(void);

/**
 * @name FLASH_ReadID
 * @brief 读取FLASH芯片的设备ID（2字节）
 * @param 无
 * @retval uint16_t: 成功返回设备ID值，失败返回0xFF
 * @note W25Q256的典型ID为0xEF18（高位在前）
 */
uint16_t FLASH_ReadID(void);

/**
 * @name FLASH_ReadJEDECID
 * @brief 读取FLASH芯片的JEDEC ID（3字节，厂商+型号+容量）
 * @param 无
 * @retval uint32_t: 成功返回JEDEC ID值，失败返回0xFF
 * @note W25Q256的JEDEC ID为0xEF4019（高位在前）
 */
uint32_t FLASH_ReadJEDECID(void);

/**
 * @name FLASH_EraseSector
 * @brief 擦除指定地址所在的4KB扇区
 * @param addr: 扇区任意地址 （自动对齐）
 * @retval HAL_StatusTypeDef:
 *         HAL_OK(擦除成功), HAL_ERROR(地址非法), HAL_TIMEOUT(擦除超时)
 * @note 擦除后扇区所有字节变为0xFF，擦除期间FLASH_IS_BUSY置位
 */
HAL_StatusTypeDef FLASH_EraseSector(uint32_t addr);

/**
 * @name FLASH_EraseChip
 * @brief 擦除整个FLASH芯片（所有存储区域）
 * @param 无
 * @retval HAL_StatusTypeDef:
 *         HAL_OK(擦除成功), HAL_TIMEOUT(擦除超时)
 * @note 整片擦除耗时极长（约100秒），擦除期间FLASH_IS_BUSY置位
 */
HAL_StatusTypeDef FLASH_EraseChip(void);

/**
 * @name FLASH_Read
 * @brief 从指定地址读取任意长度数据到缓冲区
 * @param addr: 读取起始地址（4字节地址）
 * @param buffer: 数据接收缓冲区
 * @param length: 读取数据长度（字节）
 * @retval HAL_StatusTypeDef:
 *         HAL_OK(读取成功), HAL_ERROR(参数非法/读取失败)
 * @note 读取长度+起始地址不能超过FLASH最大地址(0x1FFFFFFF)
 */
HAL_StatusTypeDef FLASH_Read(uint32_t addr, uint8_t* buffer, uint32_t length);

/**
 * @name FLASH_WritePage
 * @brief 页编程（向指定地址写入最多256字节数据，不带自动擦除）
 * @param addr: 写入起始地址（4字节地址）
 * @param data: 待写入数据缓冲区
 * @param len: 写入数据长度（最大256字节，超出会截断）
 * @retval HAL_StatusTypeDef:
 *         HAL_OK(写入成功), HAL_ERROR(参数非法), HAL_TIMEOUT(写入超时)
 * @note 1. 写入地址必须在单个页范围内（256字节）
 *       2. 写入前必须确保目标区域已被擦除（字节值为0xFF）
 */
HAL_StatusTypeDef FLASH_WritePage(uint32_t addr, const uint8_t *data, uint32_t len);

/**
 * @name FLASH_Write
 * @brief 从指定地址写入任意长度数据（自动擦除扇区，保留扇区内未覆盖数据）
 * @param addr: 写入起始地址（4字节地址）
 * @param data: 待写入数据缓冲区
 * @param len: 写入数据长度（字节）
 * @retval HAL_StatusTypeDef:
 *         HAL_OK(写入成功), HAL_ERROR(参数非法), HAL_TIMEOUT(操作超时)
 * @note 1. 自动处理跨扇区/跨页写入，无需手动擦除
 *       2. 自动分段DMA传输，可以发送超过uint16_t的数据长度（虽然大概率用不到）
 */
HAL_StatusTypeDef FLASH_Write(uint32_t addr, const uint8_t *data, uint32_t len);


#endif /* FLASH_H_ */