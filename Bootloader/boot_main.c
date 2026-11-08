/**
 ******************************************************************************
 * @file    boot_main.c
 * @brief   OTA Bootloader 主流程：
 *          时钟 -> GPIO -> SPI Flash -> FATFS -> 读状态记录 -> （是否搬运新固件）
 *          -> 校验应用向量表 -> 跳转。
 *          P2 阶段只实现“应用有效就跳转”，搬运逻辑由 P3 的 boot_ota.c 接入。
 ******************************************************************************
 */
#include "boot.h"
#include "stm32f4xx_hal.h"

static void Boot_Error(void);
static void Boot_Clock_Init(void);
static void Boot_Gpio_Init(void);

/* ---------------------------- 指示灯 ---------------------------- */
void Boot_Led(bool on)
{
    if (on)
    {
        BOOT_LED_PORT->BSRR = (uint32_t)BOOT_LED_PIN;
    }
    else
    {
        BOOT_LED_PORT->BSRR = (uint32_t)BOOT_LED_PIN << 16;
    }
}

/* ---------------------------- GPIO 直写 ---------------------------- */
void Boot_Gpio_Setup(GPIO_TypeDef *port, uint32_t pins, bool is_af,
                     uint32_t pull, uint32_t speed, uint32_t af)
{
    for (uint32_t i = 0U; i < 16U; i++)
    {
        if ((pins & (1UL << i)) == 0UL)
        {
            continue;
        }

        port->MODER = (port->MODER & ~(3UL << (i * 2U))) |
                      ((is_af ? 2UL : 1UL) << (i * 2U));
        port->OTYPER &= ~(1UL << i);
        port->OSPEEDR = (port->OSPEEDR & ~(3UL << (i * 2U))) | (speed << (i * 2U));
        port->PUPDR = (port->PUPDR & ~(3UL << (i * 2U))) | (pull << (i * 2U));

        if (is_af)
        {
            uint32_t shift = (i & 7U) * 4U;
            uint32_t idx = (i < 8U) ? 0U : 1U;

            port->AFR[idx] = (port->AFR[idx] & ~(0xFUL << shift)) | (af << shift);
        }
    }
}

static void Boot_Gpio_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PC0：状态指示灯，复位后先灭 */
    BOOT_LED_PORT->BSRR = (uint32_t)BOOT_LED_PIN << 16;
    Boot_Gpio_Setup(BOOT_LED_PORT, BOOT_LED_PIN, false, 0U, 0U, 0U);
}

/* ---------------------------- 时钟（与应用完全一致） ---------------------------- */
static void Boot_Clock_Init(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState            = RCC_HSE_ON;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM            = 4U;
    osc.PLL.PLLN            = 144U;
    osc.PLL.PLLP            = RCC_PLLP_DIV2;
    osc.PLL.PLLQ            = 6U;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Boot_Error();
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK)
    {
        Boot_Error();
    }
}

/* ---------------------------- 致命错误：快闪等待 ---------------------------- */
static void Boot_Error(void)
{
    while (1)
    {
        Boot_Led(true);
        HAL_Delay(60);
        Boot_Led(false);
        HAL_Delay(60);
    }
}

/* ---------------------------- 入口 ---------------------------- */
int main(void)
{
    HAL_Init();                 /* SysTick 1ms，bootloader 里只用来做延时与超时 */

    Boot_Clock_Init();
    Boot_Gpio_Init();
    Boot_Crc32_Init();
    Boot_Led(false);

    bool lcd_ok   = Boot_Lcd_Init();
    bool flash_ok = Boot_Flash_Init();
    bool fs_ok    = flash_ok && Boot_Fs_Mount();

    /* 屏幕与字库（字库在外部 Flash：0:/font/boot.bin，缺失时只显示进度条） */
    if (lcd_ok)
    {
        Boot_Font_Init();
        Boot_Ui_Init(Boot_Font_Ready());
        if (!Boot_Font_Ready())
        {
            Boot_Led(true);
        }
    }

    /* OTA 决策：处理升级/回滚请求、确认上次升级、应用损坏时用快照兜底 */
    if (fs_ok)
    {
        Boot_Ota_Boot();
    }
    else
    {
        Boot_Ui_Status(Boot_Txt_NoPkg, false);
    }

    if (Boot_App_Valid())
    {
        Boot_Led(true);
        HAL_Delay(20);
        Boot_App_Jump();
        /* Boot_App_Jump() 正常情况下不返回 */
    }

    /* 应用分区无效且暂时无包可刷：慢闪待机，等待 USB/ST-Link 处理 */
    while (1)
    {
        Boot_Led(true);
        HAL_Delay(200);
        Boot_Led(false);
        HAL_Delay(200);
    }
}
