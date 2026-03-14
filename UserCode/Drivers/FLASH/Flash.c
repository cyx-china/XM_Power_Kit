/**
  ******************************************************************************
  * @file           : Flash.c
  * @brief          : A W25Q256 FLASH Driver Using DMA.
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


//------------------------- 宏定义 -------------------------//
/* 标志位 */
// RTOS的二进制信号量，用在上下文中
extern osSemaphoreId_t FLASH_TX_Cplt_SemHandle;         // DMA 发送完成 信号量
extern osSemaphoreId_t FLASH_RX_Cplt_SemHandle;         // DMA 接收完成 信号量
// 裸机用的标志位，用在中断里 （请看第144行注释）
volatile bool DMA_TX_Cplt_Flag = false;                 // DMA 发送完成 标志位
volatile bool DMA_RX_Cplt_Flag = false;                 // DMA 接收完成 标志位

/* FLASH写入缓冲区 */
static uint8_t FLASH_Write_Buffer[FLASH_SECTOR_SIZE];   // FLASH写入缓冲区

/* 是否启用调式宏 */
#define FLASH_Debug_Mode        0                             // 1:启用 ;  0:关闭


//---------------------- 串口调试函数 ----------------------//
#if Debug_Mode

#define USART3_TX_BUF_SIZE    256     // 串口发送缓冲区大小

static void USART3_Printf(const char *format, ...)
{
    char tx_buf[USART3_TX_BUF_SIZE] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(tx_buf, USART3_TX_BUF_SIZE - 1, format, args);
    va_end(args);

    // 阻塞发送字符串到USART3（超时100ms）
    HAL_UART_Transmit(&huart3, (uint8_t*)tx_buf, strlen(tx_buf), 100);
}

#endif


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


// SPI 发送4字节地址 （阻塞）
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
    // 入参校验
    if (length == 0) {return HAL_OK;}                   // 长度为0，直接返回成功
    if (data == NULL) {return HAL_ERROR;}               // 若数据指针不存在，返回错误

    // PS:由于STM32的DMA的NDTR寄存器是16bit的，也就是说，DMA一次性最多传输65536单位的数据，所以需要进行分段传输操作

    uint32_t remaining = length;                        // 当前剩余的字节数
    const uint8_t *p = data;                            // 指向当前发送位置的指针

    while (remaining > 0)
    {
        uint16_t chunk = (remaining > 65535) ? 65535 : remaining;         // 本次传输的数据长度，剩余超过65535就发送65535个

        if (HAL_SPI_Transmit_DMA(&FLASH_SPI_HANDLE, (uint8_t *)p, chunk) != HAL_OK)
        {     // 启动DMA传输
            return HAL_ERROR;
        }

        // 等待传输完成
        if (__get_IPSR() != 0U)                // 判断是否在中断中
        {
            DMA_TX_Cplt_Flag = false;          // 是就轮询死等
            while (DMA_TX_Cplt_Flag == false)
            {  // PS:此处这么写是有妥协的！！因为MSC是中断驱动，且单线程强同步，而且他会在中断里调用读写函数....
            }  //         所以用不了信号量只能死等。中断调用的函数要尽可能的快！！！这部分不要学我！！！
        }
        else
        {
            osSemaphoreAcquire(FLASH_TX_Cplt_SemHandle,osWaitForever);  // 不是的话就获取信号量，让任务进入阻塞态
        }

        // 变量更新
        p += chunk;                                       // 更新位置指针
        remaining -= chunk;                               // 更新剩余数据数量
    }
    return HAL_OK;
}

// 使用DMA接收任意长数据
static HAL_StatusTypeDef SPI_RecvData(uint8_t *buf, const uint32_t length)
{
    // 入参校验
    if (length == 0) {return HAL_OK;}                   // 长度为0，直接返回成功
    if (buf == NULL) {return HAL_ERROR;}                // 若缓冲区指针不存在，返回错误

    uint32_t remaining = length;
    uint8_t *p = buf;

    // 思路同上

    while (remaining > 0)
    {
        uint16_t chunk = (remaining > 65535) ? (uint16_t)65535 : (uint16_t)remaining;

        if (HAL_SPI_Receive_DMA(&FLASH_SPI_HANDLE, p, chunk) != HAL_OK)
        {
            return HAL_ERROR;
        }

        // 等待传输完成
        if (__get_IPSR() != 0U)                 // 判断是否在中断中
        {
            DMA_RX_Cplt_Flag = false;           // 是就轮询死等
            while (DMA_RX_Cplt_Flag == false)
            {  // PS:理由同上
            }
        }
        else
        {
            osSemaphoreAcquire(FLASH_RX_Cplt_SemHandle,osWaitForever);  // 不是的话就获取信号量，让任务进入阻塞态
        }

        // 更新变量
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
    if (time_ms == 0)
    {
        return HAL_OK;
    }

    uint32_t start_tick = osKernelGetTickCount();
    uint32_t elapsed_tick = 0;

    // 将毫秒转换为 tick
    uint32_t timeout_tick = (time_ms * osKernelGetTickFreq()) / 1000;

    do
    {
        uint8_t sr1 = FLASH_ReadStatusReg1();
        if ((sr1 & 0x01) == 0)
        {
            return HAL_OK;
        }

        // 根据上下文选择不同的等待策略
        if (__get_IPSR() != 0U)                           // 此处在中断里使用轮询的原因 同 130行注释
        {
            // 中断上下文：轻量轮询
            for (volatile uint32_t i = 0; i < 1000; i++)
            {
                __NOP();
            }
        }
        else
        {
            // 任务上下文
            osDelay(1);
        }

        // 计算已经过去多少 tick
        elapsed_tick = osKernelGetTickCount() - start_tick;

    } while (elapsed_tick < timeout_tick);

    return HAL_TIMEOUT;
}

// 写使能
static HAL_StatusTypeDef FLASH_WriteEnable(void)
{
    uint8_t Retry_times = 0;        // 重试次数
    while (Retry_times < 3)         // 当重试次数 < 3
    {
        // 尝试写使能
        FLASH_CS_LOW();
        SPI_SendByte(FLASH_CMD_WRITE_ENABLE);
        FLASH_CS_HIGH();
        // 判断是否写使能成功
        uint8_t RegValue = FLASH_ReadStatusReg1();            // 读取 状态寄存器 值
        if (RegValue & (0x01 << 1) ) {return HAL_OK;}         // 判断 bit1 是否为1 （为1表示已使能写入）
        Retry_times++;                                        // 若不为1，则增加重试次数
        osDelay(1);                                           // 短暂延时
    }
    return HAL_ERROR;                                         // 尝试3次 写使能 失败，则返回错误
}

// ------------------------ 配置函数 ----------------------- //
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

// FLASH初始化
HAL_StatusTypeDef FLASH_Init(void)
{
    HAL_StatusTypeDef status = FLASH_WakeUp();    // 唤醒芯片
    if (status != HAL_OK) {return status;}

    status = FLASH_Enable4ByteAddr();             // 启动4字节模式
    if (status != HAL_OK) {return status;}

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
HAL_StatusTypeDef FLASH_EraseSector(const uint32_t addr)     // 地址可以不做4k对齐
{
    //入参检查
    if (addr > FLASH_MAX_ADDR){return HAL_ERROR;}
    // 地址4k对齐
    uint32_t sector_start_addr = addr & ~ (FLASH_SECTOR_SIZE - 1);

    HAL_StatusTypeDef status = FLASH_WriteEnable();           // 写使能
    if (status != HAL_OK) {return status;}

    FLASH_CS_LOW();
    status = SPI_SendByte(FLASH_CMD_SECTOR_ERASE_4B);    // 发送扇区擦除指令
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}

    status = SPI_Send4ByteAddr(sector_start_addr);
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}    // 发送扇区首地址
    FLASH_CS_HIGH();

    status = FLASH_WaitBusy(TIMEOUT_ERASE_SECTOR);    // 等待处理扇区擦除
    if (status != HAL_OK) {return status;}

    return HAL_OK;
}

// 整片擦除
HAL_StatusTypeDef FLASH_EraseChip(void)
{
    HAL_StatusTypeDef status = FLASH_WriteEnable();         // 写使能
    if (status != HAL_OK) {return status;}

    FLASH_CS_LOW();
    status = SPI_SendByte(FLASH_CMD_CHIP_ERASE);       // 发送整片擦除指令
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    FLASH_CS_HIGH();

    status = FLASH_WaitBusy(TIMEOUT_ERASE_CHIP);    // 等待处理芯片擦除
    if (status != HAL_OK) {return status;}

    return HAL_OK;
}


// ----------------------- 读写操作 ------------------------- //
// 从任意位置读取任意长度数据到缓冲区
HAL_StatusTypeDef FLASH_Read(uint32_t addr,uint8_t* buffer,uint32_t length)
{
    // 入参校验
    if (addr > FLASH_MAX_ADDR || addr + length > FLASH_MAX_ADDR || buffer == NULL){return HAL_ERROR;}   // 参数范围校验 & 空指针检查
    if (length == 0) {return HAL_OK;}

    FLASH_CS_LOW();
    HAL_StatusTypeDef status = SPI_SendByte(FLASH_CMD_READ_DATA_4B);    // 发送读指令
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}

    status = SPI_Send4ByteAddr(addr);                                        // 发送读取起始地址
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}

    status = SPI_RecvData(buffer,length);                                    // 接收数据
    if (status != HAL_OK) {FLASH_CS_HIGH();return status;}
    FLASH_CS_HIGH();

    return HAL_OK;
}

// 页编程
HAL_StatusTypeDef FLASH_WritePage(const uint32_t addr, const uint8_t *data, const uint32_t len)
{
    // 入参检查
    if (len == 0){return HAL_OK;}
    if (addr > FLASH_MAX_ADDR || data == NULL  || len > FLASH_PAGE_SIZE) {return HAL_ERROR;}
    uint32_t data_len = len;
    if (((addr & ~(FLASH_PAGE_SIZE - 1)) + FLASH_PAGE_SIZE - 1 ) < addr + len - 1)     // 如果 实际的末地址 超过了 页末尾地址
    {  //|<------------ 计算传入地址所在页的页末地址 -------------->|
        data_len = (addr & ~(FLASH_PAGE_SIZE - 1)) + FLASH_PAGE_SIZE - addr ;          // 更新写入的字节数为 传入地址 到 页末尾 的长度
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

    return FLASH_WaitBusy(TIMEOUT_PAGE_PROGRAM);  // 等待页编程完成
}


// 从任意位置写入任意长度数据 （保留同扇区数据）
HAL_StatusTypeDef FLASH_Write(uint32_t addr, const uint8_t *data, const uint32_t len)
{
    // 入参校验
    if (len == 0){return HAL_OK;}
    if (addr > FLASH_MAX_ADDR || data == NULL || (addr + len - 1) > FLASH_MAX_ADDR)
    {
        return HAL_ERROR;
    }

    // 一些变量
    uint32_t remaining = len;               // 剩余的代写入字节数
    uint32_t current_addr = addr;           // 当前的写地址
    const uint8_t* current_data = data;     // 当前的数据指针

    // 写入逻辑
    while (remaining > 0)                   // 当还有数据剩余
    {
        uint32_t sector_start = current_addr & ~(FLASH_SECTOR_SIZE - 1); // 得到当前数据所在的扇区首地址
        uint32_t offset = current_addr - sector_start;                   // 当前地址在此扇区的偏移
        uint32_t DataCanWrite_ThisSector = FLASH_SECTOR_SIZE - offset;   // 得到此扇区可以还能写多少字节
        uint32_t write_len = (remaining < DataCanWrite_ThisSector)       // 本次实际写入的字节数
                             ? remaining : DataCanWrite_ThisSector;

        // 将此扇区的数据读出到缓冲区
        HAL_StatusTypeDef status = FLASH_Read(sector_start, FLASH_Write_Buffer, FLASH_SECTOR_SIZE);
        if (status != HAL_OK) return status;

        // 对比缓冲区的数据是否全为0xFF

        // PS:这里用的是32字节展开，然后一次性比较8个uint32_t,编译器优化开个-02/-03效率很高（主要是memcpy费内存）
        {
            const uint32_t * restrict p = (const uint32_t *)FLASH_Write_Buffer;     // 强转32位指针
            const uint32_t ff = 0xFFFFFFFFu;                                        // 用于比较的值
            uint_fast8_t i = FLASH_SECTOR_SIZE / 32;                                // 比较的次数 （128次）

            bool is_all_ff = true;                                                  // 比较结果

            do                                                                      // 很简单的判断逻辑
            {
                if (p[0] != ff || p[1] != ff || p[2] != ff || p[3] != ff ||
                    p[4] != ff || p[5] != ff || p[6] != ff || p[7] != ff)
                {
                    is_all_ff = false;                                              // 如果发现不全为0xff，改结果，跳出循环
                    break;
                }
                p += 8;
            } while (--i);

            if (!is_all_ff)                                                     // 若此时FLASH缓冲区不全是0xFF
            {
                status = FLASH_EraseSector(sector_start);                       // 则需要擦除此扇区
                if (status != HAL_OK) return status;
            }
        }

        // 将要改写的数据覆盖到缓冲区里
        memcpy(&FLASH_Write_Buffer[offset], current_data, write_len);

        // 将更新后的缓冲区分页写回FLASH
        for (uint32_t page = 0; page < (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE); page++)
        {
            // 循环写入16次
            uint32_t page_addr = sector_start + page * FLASH_PAGE_SIZE; // 当前页的首地址
            status = FLASH_WritePage(page_addr, &FLASH_Write_Buffer[page * FLASH_PAGE_SIZE], FLASH_PAGE_SIZE); // 进行页写入
            if (status != HAL_OK) return status;
        }

        // 更新变量
        current_addr += write_len;
        current_data += write_len;
        remaining -= write_len;
    }

    return HAL_OK;
}

// ========================= FATFS相关函数 =============================//


