/**
 ******************************************************************************
 * @file    boot_lcd.c
 * @brief   bootloader 自己的屏幕驱动：FSMC + ST7789，只保留填充/贴图两种原语。
 *          不复用 UserCode/Drivers/TFT_LCD（那套依赖 FreeRTOS 信号量与命令队列）。
 ******************************************************************************
 */
#include "boot.h"
#include "stm32f4xx_hal.h"

/* FSMC bank1 NE1 基址 0x60000000：A16 当 RS（0=命令，1=数据） */
#define LCD_BASE    0x60000000UL
#define LCD_CMD     (*(volatile uint16_t *)(LCD_BASE + 0x00000000UL))
#define LCD_DATA    (*(volatile uint16_t *)(LCD_BASE + 0x00020000UL))

#define LCD_W      320U
#define LCD_H      240U

#define LCD_RST_PORT GPIOC
#define LCD_RST_PIN  GPIO_PIN_3

#define LCD_BL_PORT  GPIOA
#define LCD_BL_PIN   GPIO_PIN_9

/* ============================ 底层读写 ============================ */
static inline void lcd_cmd(uint16_t cmd)
{
    LCD_CMD = cmd;
}

static inline void lcd_data(uint16_t data)
{
    LCD_DATA = data;
}

void Boot_Lcd_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    lcd_cmd(0x2AU);
    lcd_data((uint16_t)(x0 >> 8));
    lcd_data((uint16_t)(x0 & 0xFFU));
    lcd_data((uint16_t)(x1 >> 8));
    lcd_data((uint16_t)(x1 & 0xFFU));

    lcd_cmd(0x2BU);
    lcd_data((uint16_t)(y0 >> 8));
    lcd_data((uint16_t)(y0 & 0xFFU));
    lcd_data((uint16_t)(y1 >> 8));
    lcd_data((uint16_t)(y1 & 0xFFU));

    lcd_cmd(0x2CU);
}

void Boot_Lcd_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    uint32_t count;

    if (x0 >= LCD_W || y0 >= LCD_H || x1 >= LCD_W || y1 >= LCD_H || x0 > x1 || y0 > y1)
    {
        return;
    }

    Boot_Lcd_SetWindow(x0, y0, x1, y1);
    count = (uint32_t)(x1 - x0 + 1U) * (uint32_t)(y1 - y0 + 1U);
    while (count-- > 0U)
    {
        lcd_data(color);
    }
}

void Boot_Lcd_Blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels)
{
    uint32_t count;

    if (w == 0U || h == 0U || (x + w) > LCD_W || (y + h) > LCD_H)
    {
        return;
    }

    Boot_Lcd_SetWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    count = (uint32_t)w * h;
    for (uint32_t i = 0U; i < count; i++)
    {
        lcd_data(pixels[i]);
    }
}

/* ============================ FSMC 初始化（直接写寄存器，省掉 HAL_SRAM） ============================ */
static void boot_fsmc_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_FSMC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* PE7..PE15 = FSMC_D4..D12 */
    gpio.Pin       = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
                     GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF12_FSMC;
    HAL_GPIO_Init(GPIOE, &gpio);

    /* PD0/1 = D2/D3，PD4 = NOE，PD5 = NWE，PD7 = NE1，PD8..PD11 = D13..D15/A16，PD14/15 = D0/D1 */
    gpio.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7 |
                     GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |
                     GPIO_PIN_14 | GPIO_PIN_15;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF12_FSMC;
    HAL_GPIO_Init(GPIOD, &gpio);

    /* BCR1：SRAM 模式、16 位、写使能、不使能复用 */
    FSMC_Bank1->BTCR[0] = FSMC_BCR1_MBKEN | FSMC_BCR1_FACCEN | FSMC_BCR1_WREN |
                          (1UL << 4);                    /* MWID = 01（16 位） */
    /* BTR1：ADDSET=5 ADDHLD=15 DATAST=5 BUSTURN=15 CLKDIV=16 DATLAT=17 模式 A
       与应用里 MX_FSMC_Init 的时序完全一致 */
    FSMC_Bank1->BTCR[1] = 5UL | (15UL << 4) | (5UL << 8) | (15UL << 16) |
                          (16UL << 20) | (17UL << 24);
}

/* ============================ ST7789 初始化 ============================ */
static void lcd_hw_reset(void)
{
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(10);
}

static void lcd_panel_init(void)
{
    lcd_cmd(0x3A);                          /* 像素格式：RGB565 */
    lcd_data(0x05);

    lcd_cmd(0xC5);                          /* VCOM */
    lcd_data(0x1A);

    lcd_cmd(0x36);                          /* 显示方向：横屏 270° */
    lcd_data(0x60);

    lcd_cmd(0xB2);                          /* porch */
    lcd_data(0x0C);
    lcd_data(0x0C);
    lcd_data(0x00);
    lcd_data(0x33);
    lcd_data(0x33);

    lcd_cmd(0xB7);                          /* 栅极控制 */
    lcd_data(0x55);

    lcd_cmd(0xBB);                          /* VCOM */
    lcd_data(0x1A);

    lcd_cmd(0xC0);                          /* 电源控制1 */
    lcd_data(0x2C);

    lcd_cmd(0xC2);
    lcd_data(0x01);

    lcd_cmd(0xC3);
    lcd_data(0x19);

    lcd_cmd(0xC4);
    lcd_data(0x20);

    lcd_cmd(0xC6);                          /* 帧速率 */
    lcd_data(0x0F);

    lcd_cmd(0xD0);
    lcd_data(0xA7);
    lcd_cmd(0xD0);
    lcd_data(0xA4);
    lcd_data(0xA1);

    lcd_cmd(0xD6);
    lcd_data(0xA1);

    lcd_cmd(0xE8);
    lcd_data(0x03);

    lcd_cmd(0xE9);
    lcd_data(0x09);
    lcd_data(0x09);
    lcd_data(0x08);

    lcd_cmd(0xE0);                          /* 正伽马 */
    lcd_data(0xF0);
    lcd_data(0x03);
    lcd_data(0x09);
    lcd_data(0x0B);
    lcd_data(0x0A);
    lcd_data(0x16);
    lcd_data(0x2B);
    lcd_data(0x33);
    lcd_data(0x41);
    lcd_data(0x38);
    lcd_data(0x14);
    lcd_data(0x14);
    lcd_data(0x29);
    lcd_data(0x2F);

    lcd_cmd(0xE1);                          /* 负伽马 */
    lcd_data(0xF0);
    lcd_data(0x04);
    lcd_data(0x06);
    lcd_data(0x09);
    lcd_data(0x08);
    lcd_data(0x04);
    lcd_data(0x2B);
    lcd_data(0x32);
    lcd_data(0x41);
    lcd_data(0x36);
    lcd_data(0x12);
    lcd_data(0x12);
    lcd_data(0x2A);
    lcd_data(0x30);

    lcd_cmd(0x11);                          /* 退出睡眠 */
    HAL_Delay(120);
    lcd_cmd(0x29);                          /* 显示开 */
}

/* ============================ 对外初始化 ============================ */
bool Boot_Lcd_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 复位脚 PC3 */
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);
    gpio.Pin   = LCD_RST_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LCD_RST_PORT, &gpio);

    /* 背光 PA9：bootloader 里直接常亮，不做 PWM。
       应用启动后 MX_TIM1_Init 会把 PA9 重新配成 TIM1_CH2 复用，不会相互影响。 */
    HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_SET);
    gpio.Pin   = LCD_BL_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LCD_BL_PORT, &gpio);

    boot_fsmc_init();

    lcd_hw_reset();
    lcd_panel_init();

    return true;
}
