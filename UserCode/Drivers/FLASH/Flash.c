/**
  ******************************************************************************
  * @file           : Flash.c
  * @brief          : A W25Qxx FLASH Driver Using DMA (W25Q32~W25Q512 Compatible).
  *                   Suit for Freertos,Lvgl,Fatfs,MSC.
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
//----------------------- 头文件引入 ------------------------//
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include "spi.h"
#include "usart.h"
#include "cmsis_os2.h"
#include "Flash.h"

//------------------------- 函数指针类型定义 -------------------------//
typedef HAL_StatusTypeDef (*FLASH_Read_FuncDef)(uint32_t addr, uint8_t* buffer, uint32_t length);
typedef HAL_StatusTypeDef (*FLASH_WritePage_FuncDef)(uint32_t addr, const uint8_t *data, uint32_t len);
typedef HAL_StatusTypeDef (*FLASH_EraseSector_FuncDef)(uint32_t addr);

//------------------------- 宏定义 -------------------------//
/* 标志位 */
// RTOS的二进制信号量，用在上下文中
extern osSemaphoreId_t FLASH_TX_Cplt_SemHandle;         // DMA 发送完成 信号量
extern osSemaphoreId_t FLASH_RX_Cplt_SemHandle;         // DMA 接收完成 信号量

// 裸机用的标志位，用在中断里
volatile bool DMA_TX_Cplt_Flag = false;                 // DMA 发送完成 标志位
volatile bool DMA_RX_Cplt_Flag = false;                 // DMA 接收完成 标志位

/* FLASH写入缓冲区 */
// 擦除的最小单位是 4KB 扇区，但读-改-写只需要按页（256B 的整数倍）分块进行。
// 缓冲区取 1KB（4 页），比整扇区 4KB 省下 3KB RAM；读取总字节数不变。
#define FLASH_WRITE_CHUNK_SIZE  1024

#if (FLASH_SECTOR_SIZE % FLASH_WRITE_CHUNK_SIZE) != 0
#error "FLASH_WRITE_CHUNK_SIZE must be a divisor of FLASH_SECTOR_SIZE"
#endif
#if (FLASH_WRITE_CHUNK_SIZE % FLASH_PAGE_SIZE) != 0
#error "FLASH_WRITE_CHUNK_SIZE must be a multiple of FLASH_PAGE_SIZE"
#endif

static uint8_t FLASH_Write_Buffer[FLASH_WRITE_CHUNK_SIZE];   // FLASH写入缓冲区

//------------------------- 动态全局参数定义 -------------------------//
uint32_t FLASH_TOTAL_SIZE     = 0;
uint32_t FLASH_MAX_ADDR       = 0;
uint32_t FLASH_SECTOR_COUNT   = 0;
uint32_t FLASH_BLOCK_COUNT    = 0;
uint32_t FLASH_PAGE_COUNT     = 0;
uint16_t FLASH_ChipID         = 0;
uint32_t FLASH_JEDEC_ID       = 0;
W25Q_ModelTypeDef FLASH_ChipModel = W25Q_Model_Unknown;
bool FLASH_Is4ByteAddrMode    = false;

//------------------------- 内部函数指针变量 -------------------------//
static FLASH_Read_FuncDef p_FLASH_Read = NULL;
static FLASH_WritePage_FuncDef p_FLASH_WritePage = NULL;
static FLASH_EraseSector_FuncDef p_FLASH_EraseSector = NULL;

//--------------------- DMA传输回调函数 ---------------------//
// DMA发送完成中断回调
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)                        // 判断是不是SPI1
    {
        DMA_TX_Cplt_Flag = true;
        osSemaphoreRelease(FLASH_TX_Cplt_SemHandle);
    }
}

// DMA接收完成中断回调
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)                        // 同上
    {
        DMA_RX_Cplt_Flag = true;
        osSemaphoreRelease(FLASH_RX_Cplt_SemHandle);
    }
}

//------------------ 底层发送&接收函数(阻塞) -------------------//
// SPI 发送一个字节数据 （阻塞）
static HAL_StatusTypeDef SPI_SendByte(const uint8_t data)
{
    return HAL_SPI_Transmit(&FLASH_SPI_HANDLE,&data,1,TIMEOUT_SPI);
}

// SPI 接收一个字节数据 （阻塞）
static HAL_StatusTypeDef SPI_RecvByte(uint8_t *data)
{
    return HAL_SPI_Receive(&FLASH_SPI_HANDLE,data,1,TIMEOUT_SPI);
}

static HAL_StatusTypeDef SPI_SendBytes(const uint8_t *data,uint32_t len) {
    return  HAL_SPI_Transmit(&FLASH_SPI_HANDLE, data, len, TIMEOUT_SPI);
}

static HAL_StatusTypeDef SPI_RecvBytes(uint8_t *data , uint32_t len)
{
    return HAL_SPI_Receive(&FLASH_SPI_HANDLE,data,len,TIMEOUT_SPI);
}

// SPI 发送3字节地址 （阻塞，3字节地址模式专用）
static inline HAL_StatusTypeDef SPI_Send3ByteAddr(uint32_t addr)
{
    uint8_t buf[3] = {
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr >> 0)
    };
    return HAL_SPI_Transmit(&FLASH_SPI_HANDLE, buf, 3, TIMEOUT_SPI);
}

// SPI 发送4字节地址 （阻塞，4字节地址模式专用）
static inline HAL_StatusTypeDef SPI_Send4ByteAddr(uint32_t addr)
{
    uint8_t buf[4] = {
        (uint8_t)(addr >> 24),
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >>  8),
        (uint8_t)(addr >>  0)
    };
    return HAL_SPI_Transmit(&FLASH_SPI_HANDLE, buf, 4, TIMEOUT_SPI);
}

//------------------ 底层发送&接收函数(DMA) -------------------//
// 使用DMA发送任意长数据
static HAL_StatusTypeDef SPI_SendData(const uint8_t *data, const uint32_t length)
{
    if (length == 0) {return HAL_OK;}
    if (data == NULL) {return HAL_ERROR;}

    uint32_t remaining = length;
    const uint8_t *p = data;

    while (remaining > 0)
    {
        uint16_t chunk = (remaining > 65535) ? 65535 : remaining;
        if (HAL_SPI_Transmit_DMA(&FLASH_SPI_HANDLE, (uint8_t *)p, chunk) != HAL_OK)
        {
            return HAL_ERROR;
        }

        if (__get_IPSR() != 0U)
        {
            DMA_TX_Cplt_Flag = false;
            while (DMA_TX_Cplt_Flag == false);
        }
        else
        {
            osSemaphoreAcquire(FLASH_TX_Cplt_SemHandle,osWaitForever);
        }

        p += chunk;
        remaining -= chunk;
    }
    return HAL_OK;
}

// 使用DMA接收任意长数据
static HAL_StatusTypeDef SPI_RecvData(uint8_t *buf, const uint32_t length)
{
    if (length == 0) {return HAL_OK;}
    if (buf == NULL) {return HAL_ERROR;}

    uint32_t remaining = length;
    uint8_t *p = buf;

    while (remaining > 0)
    {
        uint16_t chunk = (remaining > 65535) ? (uint16_t)65535 : (uint16_t)remaining;
        if (HAL_SPI_Receive_DMA(&FLASH_SPI_HANDLE, p, chunk) != HAL_OK)
        {
            return HAL_ERROR;
        }

        if (__get_IPSR() != 0U)
        {
            DMA_RX_Cplt_Flag = false;
            while (DMA_RX_Cplt_Flag == false);
        }
        else
        {
            osSemaphoreAcquire(FLASH_RX_Cplt_SemHandle,osWaitForever);
        }

        p         += chunk;
        remaining -= chunk;
    }
    return HAL_OK;
}

//------------------------ 辅助函数 -------------------------//
// 读取状态寄存器1的值
static uint8_t FLASH_ReadStatusReg1(void)
{
    uint8_t RegValue = 0;
    FLASH_CS_LOW();
    HAL_StatusTypeDef status = SPI_SendByte(FLASH_CMD_READ_STATUS1);
    if (status == HAL_OK)
    {
        SPI_RecvByte(&RegValue);
    }
    FLASH_CS_HIGH();
    return RegValue;
}

// 忙等待
static HAL_StatusTypeDef FLASH_WaitBusy(const Flash_Timeout time_ms)
{
    uint32_t start_tick = osKernelGetTickCount();
    uint32_t elapsed_tick = 0;
    uint32_t timeout_tick = (time_ms * osKernelGetTickFreq()) / 1000;

    do
    {
        uint8_t sr1 = FLASH_ReadStatusReg1();
        if ((sr1 & 0x01) == 0)
        {
            return HAL_OK;
        }

        if (__get_IPSR() != 0U)
        {
            for (volatile uint32_t i = 0; i < 1000; i++)
            {
                __NOP();
            }
        }
        else
        {
            osDelay(1);
        }

        elapsed_tick = osKernelGetTickCount() - start_tick;
    } while (elapsed_tick < timeout_tick);
    return HAL_TIMEOUT;
}

// 写使能
static HAL_StatusTypeDef FLASH_WriteEnable(void)
{
    uint8_t Retry_times = 0;
    while (Retry_times < 3)
    {
        FLASH_CS_LOW();
        SPI_SendByte(FLASH_CMD_WRITE_ENABLE);
        FLASH_CS_HIGH();

        uint8_t RegValue = FLASH_ReadStatusReg1();
        if (RegValue & (0x01 << 1) ) {return HAL_OK;}

        Retry_times++;
        osDelay(1);
    }
    return HAL_ERROR;
}

// 使能4字节模式
static HAL_StatusTypeDef FLASH_Enable4ByteAddr(void)
{
    FLASH_CS_LOW();
    HAL_StatusTypeDef status = SPI_SendByte(FLASH_CMD_ENABLE_4BYTE);
    FLASH_CS_HIGH();
    return status;
}

// 唤醒芯片
static HAL_StatusTypeDef FLASH_WakeUp(void)
{
    FLASH_CS_LOW();
    HAL_StatusTypeDef status = SPI_SendByte(FLASH_CMD_WAKE_UP);
    FLASH_CS_HIGH();
    return status;
}

//-------------------------- 3字节地址模式专用函数 --------------------------//
static HAL_StatusTypeDef FLASH_Read_3B(uint32_t addr, uint8_t* buffer, uint32_t length)
{
    if (addr > FLASH_MAX_ADDR || addr + length > FLASH_MAX_ADDR || buffer == NULL){
        return HAL_ERROR;
    }
    if (length == 0) {return HAL_OK;}

    FLASH_CS_LOW();
    HAL_StatusTypeDef status = SPI_SendByte(FLASH_CMD_READ_DATA_3B);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    status = SPI_Send3ByteAddr(addr);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    status = SPI_RecvData(buffer,length);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    FLASH_CS_HIGH();
    return HAL_OK;
}

static HAL_StatusTypeDef FLASH_WritePage_3B(const uint32_t addr, const uint8_t *data, const uint32_t len)
{
    if (len == 0){return HAL_OK;}
    if (addr > FLASH_MAX_ADDR || data == NULL  || len > FLASH_PAGE_SIZE) {return HAL_ERROR;}

    uint32_t data_len = len;
    if (((addr & ~(FLASH_PAGE_SIZE - 1)) + FLASH_PAGE_SIZE - 1 ) < addr + len - 1)
    {
        data_len = (addr & ~(FLASH_PAGE_SIZE - 1)) + FLASH_PAGE_SIZE - addr ;
    }

    HAL_StatusTypeDef status =  FLASH_WriteEnable();
    if (status != HAL_OK) {return status;}

    FLASH_CS_LOW();
    status = SPI_SendByte(FLASH_CMD_PAGE_PROGRAM_3B);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    status = SPI_Send3ByteAddr(addr);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    status = SPI_SendData(data,data_len);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    FLASH_CS_HIGH();

    return FLASH_WaitBusy(TIMEOUT_PAGE_PROGRAM);
}

static HAL_StatusTypeDef FLASH_EraseSector_3B(const uint32_t addr)
{
    if (addr > FLASH_MAX_ADDR){return HAL_ERROR;}

    uint32_t sector_start_addr = addr & ~ (FLASH_SECTOR_SIZE - 1);
    HAL_StatusTypeDef status = FLASH_WriteEnable();
    if (status != HAL_OK) {return status;}

    FLASH_CS_LOW();
    status = SPI_SendByte(FLASH_CMD_SECTOR_ERASE_3B);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    status = SPI_Send3ByteAddr(sector_start_addr);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    FLASH_CS_HIGH();

    return FLASH_WaitBusy(TIMEOUT_ERASE_SECTOR);
}

//-------------------------- 4字节地址模式专用函数 --------------------------//
static HAL_StatusTypeDef FLASH_Read_4B(uint32_t addr, uint8_t* buffer, uint32_t length)
{
    if (addr > FLASH_MAX_ADDR || addr + length > FLASH_MAX_ADDR || buffer == NULL){
        return HAL_ERROR;
    }
    if (length == 0) {return HAL_OK;}

    FLASH_CS_LOW();
    HAL_StatusTypeDef status = SPI_SendByte(FLASH_CMD_READ_DATA_4B);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    status = SPI_Send4ByteAddr(addr);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    status = SPI_RecvData(buffer,length);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    FLASH_CS_HIGH();
    return HAL_OK;
}

static HAL_StatusTypeDef FLASH_WritePage_4B(const uint32_t addr, const uint8_t *data, const uint32_t len)
{
    if (len == 0){return HAL_OK;}
    if (addr > FLASH_MAX_ADDR || data == NULL  || len > FLASH_PAGE_SIZE) {return HAL_ERROR;}

    uint32_t data_len = len;
    if (((addr & ~(FLASH_PAGE_SIZE - 1)) + FLASH_PAGE_SIZE - 1 ) < addr + len - 1)
    {
        data_len = (addr & ~(FLASH_PAGE_SIZE - 1)) + FLASH_PAGE_SIZE - addr ;
    }

    HAL_StatusTypeDef status =  FLASH_WriteEnable();
    if (status != HAL_OK) {return status;}

    FLASH_CS_LOW();
    status = SPI_SendByte(FLASH_CMD_PAGE_PROGRAM_4B);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    status = SPI_Send4ByteAddr(addr);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    status = SPI_SendData(data,data_len);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    FLASH_CS_HIGH();

    return FLASH_WaitBusy(TIMEOUT_PAGE_PROGRAM);
}

static HAL_StatusTypeDef FLASH_EraseSector_4B(const uint32_t addr)
{
    if (addr > FLASH_MAX_ADDR){return HAL_ERROR;}

    uint32_t sector_start_addr = addr & ~ (FLASH_SECTOR_SIZE - 1);
    HAL_StatusTypeDef status = FLASH_WriteEnable();
    if (status != HAL_OK) {return status;}

    FLASH_CS_LOW();
    status = SPI_SendByte(FLASH_CMD_SECTOR_ERASE_4B);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    status = SPI_Send4ByteAddr(sector_start_addr);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    FLASH_CS_HIGH();

    return FLASH_WaitBusy(TIMEOUT_ERASE_SECTOR);
}

// ------------------------- 配置函数 ----------------------- //
// FLASH初始化
HAL_StatusTypeDef FLASH_Init(void)
{
    HAL_StatusTypeDef status;

    // 唤醒芯片
    status = FLASH_WakeUp();
    if (status != HAL_OK) {return status;}
    osDelay(1); // 如果你在裸机，记得换成裸机下的延时函数

    // 读取JEDEC ID
    FLASH_JEDEC_ID = FLASH_ReadJEDECID();
    FLASH_ChipID = FLASH_ReadID();
    if (FLASH_JEDEC_ID == 0xFFFFFFFF || FLASH_JEDEC_ID == 0x00000000)
    {
        return HAL_ERROR;
    }

    // 匹配芯片型号，配置动态参数
    switch (FLASH_JEDEC_ID)
    {
        case 0xEF4016: // W25Q32
            FLASH_ChipModel = W25Q_Model_W25Q32;
            FLASH_TOTAL_SIZE = 0x400000;    // 4MB
            FLASH_Is4ByteAddrMode = false;
            break;

        case 0xEF4017: // W25Q64
            FLASH_ChipModel = W25Q_Model_W25Q64;
            FLASH_TOTAL_SIZE = 0x800000;    // 8MB
            FLASH_Is4ByteAddrMode = false;
            break;

        case 0xEF4018: // W25Q128
            FLASH_ChipModel = W25Q_Model_W25Q128;
            FLASH_TOTAL_SIZE = 0x1000000;   // 16MB
            FLASH_Is4ByteAddrMode = false;
            break;

        case 0xEF4019: // W25Q256
            FLASH_ChipModel = W25Q_Model_W25Q256;
            FLASH_TOTAL_SIZE = 0x2000000;   // 32MB
            FLASH_Is4ByteAddrMode = true;
            break;

        case 0xEF4020: // W25Q512
            FLASH_ChipModel = W25Q_Model_W25Q512;
            FLASH_TOTAL_SIZE = 0x4000000;   // 64MB
            FLASH_Is4ByteAddrMode = true;
            break;

        default: // 不支持的芯片型号
            FLASH_ChipModel = W25Q_Model_Unknown;
            FLASH_TOTAL_SIZE = 0;
            FLASH_Is4ByteAddrMode = false;
            return HAL_ERROR;
    }

    // 第四步：计算衍生的动态参数
    FLASH_MAX_ADDR = FLASH_TOTAL_SIZE - 1;                          // 计算最大地址
    FLASH_SECTOR_COUNT = FLASH_TOTAL_SIZE / FLASH_SECTOR_SIZE;      // 计算扇区数量
    FLASH_BLOCK_COUNT = FLASH_TOTAL_SIZE / FLASH_BLOCK_SIZE;        // 计算块数量
    FLASH_PAGE_COUNT = FLASH_TOTAL_SIZE / FLASH_PAGE_SIZE;          // 计算页数量

    // 地址模式切换（Q256/Q512需要使能4字节地址）
    if (FLASH_Is4ByteAddrMode)
    {
        status = FLASH_Enable4ByteAddr();
        if (status != HAL_OK) {return status;}
    }

    // 函数指针映射，绑定对应模式的底层函数
    if (FLASH_Is4ByteAddrMode)
    {
        p_FLASH_Read = FLASH_Read_4B;
        p_FLASH_WritePage = FLASH_WritePage_4B;
        p_FLASH_EraseSector = FLASH_EraseSector_4B;
    }
    else
    {
        p_FLASH_Read = FLASH_Read_3B;
        p_FLASH_WritePage = FLASH_WritePage_3B;
        p_FLASH_EraseSector = FLASH_EraseSector_3B;
    }

    // 校验函数指针是否初始化完成
    if (p_FLASH_Read == NULL || p_FLASH_WritePage == NULL || p_FLASH_EraseSector == NULL)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

// ------------------------- ID读取 ------------------------ //
// 读取ID
uint16_t FLASH_ReadID(void)
{
    uint16_t id = 0;
    uint8_t command_pack[4] = {FLASH_CMD_READ_ID,0,0,0};
    uint8_t id_bytes[2];
    FLASH_CS_LOW();
    HAL_StatusTypeDef status = SPI_SendBytes(command_pack,4);
    if (status != HAL_OK) {FLASH_CS_HIGH();return 0xff;}
    status = SPI_RecvBytes(id_bytes,2);
    if (status != HAL_OK) {FLASH_CS_HIGH();return 0xff;}
    FLASH_CS_HIGH();
    id = id_bytes[0] << 8 | id_bytes[1];
    return id;
}

// 读取JEDEC ID
uint32_t FLASH_ReadJEDECID(void)
{
    uint32_t jedec_id = 0;
    uint8_t id_bytes[3];
    FLASH_CS_LOW();
    HAL_StatusTypeDef status = SPI_SendByte(FLASH_CMD_JEDEC_ID);
    if (status != HAL_OK) {FLASH_CS_HIGH();return 0xff;}
    status = SPI_RecvBytes(id_bytes,3);
    if (status != HAL_OK) {FLASH_CS_HIGH();return 0xff;}
    FLASH_CS_HIGH();
    jedec_id = id_bytes[0] << 16 | id_bytes[1] << 8 | id_bytes[2];
    return jedec_id;
}

// ----------------------- 扇区擦除 ------------------------- //
// 擦除4kb扇区
HAL_StatusTypeDef FLASH_EraseSector(const uint32_t addr)
{
    if (p_FLASH_EraseSector == NULL) {return HAL_ERROR;}
    return p_FLASH_EraseSector(addr);
}

// 整片擦除
HAL_StatusTypeDef FLASH_EraseChip(void)
{
    HAL_StatusTypeDef status = FLASH_WriteEnable();
    if (status != HAL_OK) {return status;}
    FLASH_CS_LOW();
    status = SPI_SendByte(FLASH_CMD_CHIP_ERASE);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    FLASH_CS_HIGH();
    status = FLASH_WaitBusy(TIMEOUT_ERASE_CHIP);
    if (status != HAL_OK) {return status;}
    return HAL_OK;
}

// ----------------------- 读写操作 ------------------------- //
// 从任意位置读取任意长度数据到缓冲区
HAL_StatusTypeDef FLASH_Read(uint32_t addr,uint8_t* buffer,uint32_t length)
{
    if (p_FLASH_Read == NULL) {return HAL_ERROR;}
    return p_FLASH_Read(addr, buffer, length);
}

// 页编程
HAL_StatusTypeDef FLASH_WritePage(const uint32_t addr, const uint8_t *data, const uint32_t len)
{
    if (p_FLASH_WritePage == NULL) {return HAL_ERROR;}
    return p_FLASH_WritePage(addr, data, len);
}

// 从任意位置写入任意长度数据 （保留同扇区数据）
HAL_StatusTypeDef FLASH_Write(uint32_t addr, const uint8_t *data, const uint32_t len)
{
    // 初始化校验
    if (p_FLASH_Read == NULL || p_FLASH_WritePage == NULL || p_FLASH_EraseSector == NULL)
    {
        return HAL_ERROR;
    }

    // 入参校验
    if (len == 0){return HAL_OK;}
    if (addr > FLASH_MAX_ADDR || data == NULL || (addr + len - 1) > FLASH_MAX_ADDR)
    {
        return HAL_ERROR;
    }

    // 一些变量
    uint32_t remaining = len;
    uint32_t current_addr = addr;
    const uint8_t* current_data = data;

    // 写入逻辑
    while (remaining > 0)
    {
        uint32_t sector_start = current_addr & ~(FLASH_SECTOR_SIZE - 1);
        uint32_t offset = current_addr - sector_start;
        uint32_t DataCanWrite_ThisSector = FLASH_SECTOR_SIZE - offset;
        uint32_t write_len = (remaining < DataCanWrite_ThisSector)
                             ? remaining : DataCanWrite_ThisSector;

        // 本扇区的写入区间在当前扇区内是 [offset, offset + write_len)
        const bool is_full_sector = (offset == 0) && (write_len == FLASH_SECTOR_SIZE);
        bool need_erase = false;

        // 第一趟：只扫描"本次要写入的区间"，判断是否需要擦除
        // NOR FLASH 只能把位由 1 写成 0，所以只有当某个目标字节出现 0->1 时才必须擦除；
        // 这样往已用扇区的空白尾部写数据时就不必再整扇区擦一遍（省下一次约 45ms 的擦除）
        for (uint32_t chunk = 0; chunk < FLASH_SECTOR_SIZE; chunk += FLASH_WRITE_CHUNK_SIZE)
        {
            uint32_t io_start = (offset > chunk) ? offset : chunk;
            uint32_t io_end   = (chunk + FLASH_WRITE_CHUNK_SIZE < offset + write_len)
                                ? (chunk + FLASH_WRITE_CHUNK_SIZE) : (offset + write_len);
            if (io_start >= io_end) { continue; }   // 本块不在写入区间内

            HAL_StatusTypeDef status = FLASH_Read(sector_start + chunk, FLASH_Write_Buffer,
                                                FLASH_WRITE_CHUNK_SIZE);
            if (status != HAL_OK) return status;

            const uint8_t *old_data = &FLASH_Write_Buffer[io_start - chunk];
            const uint8_t *new_data = current_data + (io_start - offset);
            for (uint32_t b = 0; b < (io_end - io_start); b++)
            {
                if ((old_data[b] & new_data[b]) != new_data[b])
                {
                    need_erase = true;
                    break;
                }
            }
            if (need_erase) { break; }
        }

        // 部分覆盖的扇区一旦需要擦除，就必须先把整个 4KB 扇区快照下来再回写，
        // 而本驱动为省 RAM 只保留 1KB 缓冲，故这种情形直接返回错误（当前的唯一调用者
        // FATFS disk_write 总是整扇区写入，不会走到这里）
        if (need_erase && !is_full_sector)
        {
            return HAL_ERROR;
        }

        if (need_erase)
        {
            HAL_StatusTypeDef status = FLASH_EraseSector(sector_start);
            if (status != HAL_OK) return status;
        }

        // 第二趟：分块写回，每块 1KB（4 页）
        for (uint32_t chunk = 0; chunk < FLASH_SECTOR_SIZE; chunk += FLASH_WRITE_CHUNK_SIZE)
        {
            uint32_t chunk_addr = sector_start + chunk;
            const uint8_t *chunk_src;

            if (offset <= chunk && (offset + write_len) >= (chunk + FLASH_WRITE_CHUNK_SIZE))
            {
                // 整块都被新数据覆盖，直接用源数据，无需读回
                chunk_src = current_data + (chunk - offset);
            }
            else
            {
                // 读回该块内容（擦除后为 0xFF，未擦除时为原数据），再把新数据覆盖进去
                HAL_StatusTypeDef status = FLASH_Read(chunk_addr, FLASH_Write_Buffer,
                                                    FLASH_WRITE_CHUNK_SIZE);
                if (status != HAL_OK) return status;

                uint32_t io_start = (offset > chunk) ? offset : chunk;
                uint32_t io_end   = (chunk + FLASH_WRITE_CHUNK_SIZE < offset + write_len)
                                    ? (chunk + FLASH_WRITE_CHUNK_SIZE) : (offset + write_len);
                if (io_start < io_end)
                {
                    memcpy(&FLASH_Write_Buffer[io_start - chunk],
                           current_data + (io_start - offset),
                           io_end - io_start);
                }
                chunk_src = FLASH_Write_Buffer;
            }

            // 将该块的 4 个页依次写回 FLASH
            for (uint32_t page = 0; page < (FLASH_WRITE_CHUNK_SIZE / FLASH_PAGE_SIZE); page++)
            {
                uint32_t page_addr = chunk_addr + page * FLASH_PAGE_SIZE;
                HAL_StatusTypeDef status = FLASH_WritePage(page_addr,
                                                           &chunk_src[page * FLASH_PAGE_SIZE],
                                                           FLASH_PAGE_SIZE);
                if (status != HAL_OK) return status;
            }
        }

        // 更新变量
        current_addr += write_len;
        current_data += write_len;
        remaining -= write_len;
    }
    return HAL_OK;
}