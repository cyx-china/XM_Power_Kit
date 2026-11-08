/**
 ******************************************************************************
 * @file    boot_i2c.c
 * @brief   bootloader 的 I2C 自检：只做"器件在不在"的探测，不读写业务数据。
 *          I2C1（PB6/PB7）挂 INA226(0x40) 与 TMP102(0x48)；
 *          I2C3（PC9/PA8）挂 MCP4725(0x60)。引脚开漏必须自己配（HAL 的 MspInit
 *          在 bootloader 里没有实现）。
 ******************************************************************************
 */
#include "boot.h"

#define I2C1_ADDR_INA226    0x80U   /* 0x40 << 1 */
#define I2C1_ADDR_TMP102    0x90U   /* 0x48 << 1 */
#define I2C3_ADDR_MCP4725   0xC0U   /* 0x60 << 1 */

static I2C_HandleTypeDef s_i2c1;
static I2C_HandleTypeDef s_i2c3;

static void i2c_config(I2C_HandleTypeDef *dev, I2C_TypeDef *instance)
{
    dev->Instance             = instance;
    dev->Init.ClockSpeed      = 400000U;
    dev->Init.DutyCycle       = I2C_DUTYCYCLE_2;
    dev->Init.OwnAddress1     = 0U;
    dev->Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    dev->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    dev->Init.OwnAddress2     = 0U;
    dev->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    dev->Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

    (void)HAL_I2C_Init(dev);
}

bool Boot_I2c_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_I2C3_CLK_ENABLE();

    /* 复用开漏 + 上拉（Boot_Gpio_Setup 只配成推挽，这里再补 OTYPER） */
    Boot_Gpio_Setup(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, true, 1U, 3U, 4U);   /* I2C1 SCL/SDA */
    Boot_Gpio_Setup(GPIOC, GPIO_PIN_9,              true, 1U, 3U, 4U);   /* I2C3 SCL */
    Boot_Gpio_Setup(GPIOA, GPIO_PIN_8,              true, 1U, 3U, 4U);   /* I2C3 SDA */
    GPIOB->OTYPER |= (GPIO_PIN_6 | GPIO_PIN_7);
    GPIOC->OTYPER |= GPIO_PIN_9;
    GPIOA->OTYPER |= GPIO_PIN_8;

    i2c_config(&s_i2c1, I2C1);
    i2c_config(&s_i2c3, I2C3);

    return true;
}

/**
 * @brief  探测 I2C 器件
 * @return 应答的器件个数（正常应为 3）
 */
uint8_t Boot_I2c_Probe(void)
{
    uint8_t found = 0U;

    if (HAL_I2C_IsDeviceReady(&s_i2c1, I2C1_ADDR_INA226, 2U, 20U) == HAL_OK)
    {
        found++;
    }
    if (HAL_I2C_IsDeviceReady(&s_i2c1, I2C1_ADDR_TMP102, 2U, 20U) == HAL_OK)
    {
        found++;
    }
    if (HAL_I2C_IsDeviceReady(&s_i2c3, I2C3_ADDR_MCP4725, 2U, 20U) == HAL_OK)
    {
        found++;
    }

    return found;
}
