#ifndef XM_POWER_KIT_ST7789_H
#define XM_POWER_KIT_ST7789_H

//------------------头文件引入--------------------//
#include <stdint.h>

#include "stm32f4xx_hal.h"

//------------------FSMC地址定义-----------------//
#define FSMC_BANK1_NE1_BASE         0x60000000UL   // NE1基地址
#define LCD_CMD_ADDR                ((volatile uint16_t *)(FSMC_BANK1_NE1_BASE + 0x00000000))   // HADDR[17]=0 -> FSMC_A16=0
#define LCD_DATA_ADDR               ((volatile uint16_t *)(FSMC_BANK1_NE1_BASE + 0x00020000))   // HADDR[17]=1 -> FSMC_A16=1

//------------------FSMC引脚定义-----------------//
#define LCD_RST_GPIO            GPIOC
#define LCD_RST_PIN             GPIO_PIN_3

#define LCD_RST_LOW()           HAL_GPIO_WritePin(LCD_RST_GPIO, LCD_RST_PIN, GPIO_PIN_RESET)    // 复位拉低
#define LCD_RST_HIGH()          HAL_GPIO_WritePin(LCD_RST_GPIO, LCD_RST_PIN, GPIO_PIN_SET)      // 复位拉高

//----------------屏幕常量定义-----------------//
// PS:屏幕的坐标范围是 x: 0 ~ 319 ; y: 0~239 !!!!!
#define LCD_LINE_COUNT          240     // 屏幕行数
#define LCD_COLUMN_COUNT        320     // 屏幕列数
#define LCD_PIXEL_NUM           LCD_LINE_COUNT * LCD_COLUMN_COUNT   // 像素数量

#define DIS_DIR                 3       // 显示方向（0-竖显/1-横显90°/2-竖显180°/3-横显270°）
//------------------函数声明-----------------//

/**
 * @name   LCD_WR_CMD
 * @brief  通过FSMC向屏幕传输一个命令
 * @param  command :命令
 **/
void LCD_WR_CMD(uint16_t command);

/**
 * @name   LCD_WR_DATA
 * @brief  通过FSMC向屏幕传输一个命令
 * @param  data :命令
 **/
void LCD_WR_DATA(uint16_t data);

uint16_t LCD_ReadPoint(uint16_t Xpos, uint16_t Ypos);


/**
 * @name   LCD_Init
 * @brief  初始化屏幕显示
 * @retval HAL_StatusTypeDef : 成功则返回HAL_OK
 **/
HAL_StatusTypeDef LCD_Init(void);

/**
 * @name   LCD_SET_WINDOWS
 * @brief  设定屏幕选中区域
 * @param  X:起点的横坐标
 * @param  Y:起点的纵坐标
 * @param  X_END:终点的横坐标
 * @param  Y_END:终点的纵坐标
 **/
void LCD_SET_WINDOWS(uint16_t X, uint16_t Y, uint16_t X_END, uint16_t Y_END);

/**
 * @name   LCD_SetCursor
 * @brief  选中屏幕上一个点
 * @param  Xpos:横坐标
 * @param  Ypos:纵坐标
 **/
void LCD_SetPoint(uint16_t Xpos, uint16_t Ypos);

/**
 * @name   LCD_Clear
 * @brief  将整个屏幕刷成指定颜色
 * @param  Color:RGB565格式的颜色
 * @note   这个函数是逐点刷屏，效率较低，适合初始化时使用（无 rtos）
 * @note   若需要更快的速度，使用 LCD_Clear_DMA(),需要在 rtos中
 **/
void LCD_Clear(uint16_t Color);

/**
 * @name   LCD_FLUSH_DMA
 * @brief  通过 DMA 将指定地址，指定长度的数据通过 FSMC 发送出去
 * @param  src_addr:数据地址
 * @param  data_length:数据长度
 * @note   DMA的中断回调函数定义于ST7789.c中
 **/
void LCD_FLUSH_DMA(uint32_t src_addr,uint32_t data_length);


#endif //XM_POWER_KIT_ST7789_H