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

/* ---------------------------- GPIO ---------------------------- */
static void Boot_Gpio_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PC0：状态指示灯，复位后先灭 */
    BOOT_LED_PORT->BSRR = (uint32_t)BOOT_LED_PIN << 16;
    gpio.Pin   = BOOT_LED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BOOT_LED_PORT, &gpio);
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

    bool flash_ok = Boot_Flash_Init();
    bool fs_ok    = flash_ok && Boot_Fs_Mount();

    boot_record_t record;
    Boot_State_Load(&record);

    (void)fs_ok;
    (void)record;

    /* P3 在这里插入：检查 REQ.BIN / FW.BIN，搬运并校验新固件 */

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
