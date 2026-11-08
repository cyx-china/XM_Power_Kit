/**
 ******************************************************************************
 * @file    boot_main.c
 * @brief   Bootloader 主流程：
 *          背光 -> 时钟 -> GPIO/按键 -> 屏幕 -> 外部 Flash -> 文件系统（挂载失败
 *          就现场格式化）-> 出厂文件（目录/字库/参数文件）-> 开机自检表 ->
 *          OTA 决策（升级/回滚/首次安装）-> 应用有效就跳转。
 *
 *          一切正常时全程不出图（黑屏一闪）直接跳应用；只有需要格式化、自检未通过、
 *          没有应用固件时才把自检表画出来——新机停在这里等上位机刷固件，
 *          已有固件但某项 FAIL 时给 8 秒倒计时，期间按任意键停留。
 ******************************************************************************
 */
#include "boot.h"
#include "stm32f4xx_hal.h"

static void Boot_Clock_Init(void);
static void Boot_Gpio_Init(void);
static void Stay_Forever(void);
static void Ui_Show(void);

/* 自检表是否已经画到屏幕上（正常启动全程不画） */
static bool s_ui_shown = false;
static bool s_lcd_ok   = false;

/* 启动阶段与屏幕就绪标志：故障时由 Boot_Panic() 用来指示卡在哪一步 */
volatile uint8_t g_boot_stage     = 0U;
volatile uint8_t g_boot_lcd_ready = 0U;

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
        Boot_Panic(g_boot_stage);
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK)
    {
        Boot_Panic(g_boot_stage);
    }
}

/* ---------------------------- 自检表：只在出问题时才画 ---------------------------- */
static void Ui_Show(void)
{
    if (s_ui_shown || !s_lcd_ok)
    {
        return;
    }

    Boot_Ui_Screen(Boot_Txt_InitTitle, Boot_Txt_InitFooter, false);
    Boot_SelfTest_Paint();
    s_ui_shown = true;
}

/* 停留：维护模式（USB CDC + 文件管理/一键升级） */
static void Stay_Forever(void)
{
    bool usb_ok    = Boot_Usb_Init();
    bool connected = false;
    bool led       = false;
    uint32_t tick  = HAL_GetTick();

    if (!usb_ok)
    {
        Boot_Ui_Hint(Boot_Txt_StayHint, false);
    }

    for (;;)
    {
        if (usb_ok)
        {
            Boot_Proto_Poll();

            bool now = Boot_Usb_Configured();
            if (now != connected)
            {
                connected = now;
                Boot_Ui_Hint(connected ? Boot_Txt_UsbOk : Boot_Txt_UsbWait, false);
            }
        }

        if ((HAL_GetTick() - tick) >= 400U)
        {
            tick = HAL_GetTick();
            led  = !led;
            Boot_Led(led);
        }

        HAL_Delay(1U);
    }
}

/* ---------------------------- 倒计时提示文字："8 秒后进入系统，按键停留" ---------------------------- */
static void Make_Countdown_Text(char *out, uint32_t seconds)
{
    uint32_t i = 0U;

    if (seconds > 9U)
    {
        seconds %= 10U;
    }
    out[i++] = (char)('0' + seconds);

    const char *tail = Boot_Txt_JumpIn;

    while ((tail[0] != '\0') && (i < 38U))
    {
        out[i++] = *tail++;
    }
    out[i] = '\0';
}

/* ---------------------------- 入口 ---------------------------- */
int main(void)
{
    /* 上电第一件事：点亮背光。不依赖 HAL_Init 与时钟配置，几微秒内就有光，
       这样即使后面任何一步出错，屏幕也不会是"完全黑的"，故障一眼可见。 */
    Boot_Lcd_Backlight(true);

    HAL_Init();                 /* SysTick 1ms，由 boot_it.c 的 SysTick_Handler 驱动 */

    g_boot_stage = 1U;
    Boot_Clock_Init();

    g_boot_stage = 2U;
    Boot_Gpio_Init();
    Boot_Key_Init();
    Boot_Crc32_Init();
    Boot_Led(true);             /* 指示"正在启动"，跳转前熄灭 */

    g_boot_stage = 3U;
    s_lcd_ok = Boot_Lcd_Init();

    g_boot_stage = 4U;
    bool flash_ok = Boot_Flash_Init();

    /* 屏幕与字库：字库优先外部 Flash，缺失时用内嵌的那份。
       只清成黑屏，不画界面——正常启动时直接跳应用，不开机 logo。 */
    if (s_lcd_ok)
    {
        Boot_Font_Init();
        Boot_Lcd_Fill(0U, 0U, 319U, 239U, 0x0000U);
    }

    /* ---------------- 文件系统：挂载失败就现场格式化 ---------------- */
    g_boot_stage = 5U;
    bool fs_ok      = flash_ok && Boot_Fs_Mount();
    bool did_format = false;

    if (!fs_ok && flash_ok)
    {
        Ui_Show();                       /* 要格式化了：先把界面亮出来，别让用户面对黑屏 */
        Boot_Ui_Status(Boot_Txt_Formatting, true);
        fs_ok      = Boot_Fs_Format();
        did_format = fs_ok;
    }

    /* ---------------- 出厂文件：目录 / 字库 / 参数文件 ---------------- */
    uint8_t prov = 0U;

    if (fs_ok)
    {
        if (s_ui_shown)
        {
            Boot_Ui_Status(Boot_Txt_Provision, true);
        }
        prov = Boot_Storage_Provision();
    }

    /* ---------------- 开机自检（静默，只记录结果） ---------------- */
    Boot_SelfTest_Begin();
    (void)Boot_SelfTest_Run(fs_ok, did_format, prov);

    if (s_ui_shown)
    {
        Boot_SelfTest_Paint();
        Boot_Ui_Status(Boot_Txt_SelfTest, true);
    }

    /* ---------------- OTA：升级 / 回滚 / 首次安装 ---------------- */
    g_boot_stage = 6U;
    if (fs_ok)
    {
        Boot_Ota_Boot();
    }

    /* 搬运可能刚换了固件，这一行重新算 */
    bool app_ok = Boot_App_Valid();
    Boot_Item_Set(BOOT_ITEM_APP, app_ok ? BOOT_ITEM_OK : BOOT_ITEM_FAIL, app_ok ? "有效" : "无");

    g_boot_stage = 7U;

    /* ---------------- 1) 文件系统不可用 / 没有可用固件：自检表常驻 ---------------- */
    if (!fs_ok || !app_ok)
    {
        Ui_Show();
        Boot_SelfTest_Paint();
        Boot_Ui_Status(!fs_ok ? Boot_Txt_FormatFail : Boot_Txt_Stay, false);
        Boot_Ui_Hint(Boot_Txt_StayHint, false);
        Stay_Forever();
    }

    /* ---------------- 2) 全部通过：直接进主程序（不出图、不等待） ---------------- */
    if (Boot_SelfTest_AllOk())
    {
        Boot_App_Jump();
    }

    /* ---------------- 3) 有项目未通过：8 秒倒计时，期间按任意键停留 ---------------- */
    Ui_Show();
    Boot_SelfTest_Paint();
    Boot_Ui_Status(Boot_Txt_SomeFail, false);

    char hint[40];

    for (uint32_t sec = 8U; sec > 0U; sec--)
    {
        Make_Countdown_Text(hint, sec);
        Boot_Ui_Hint(hint, false);

        for (uint32_t tick = 0U; tick < 10U; tick++)
        {
            if (Boot_Key_Pressed())
            {
                Boot_Ui_Status(Boot_Txt_Stay, false);
                Boot_Ui_Hint(Boot_Txt_StayHint, false);
                Stay_Forever();
            }
            HAL_Delay(100U);
        }
    }

    Boot_App_Jump();

    Stay_Forever();            /* 正常情况下不会执行到这里 */
}
