/**
 ******************************************************************************
 * @file    boot_it.c
 * @brief   中断与故障处理。
 *
 *          bootloader 不链接 FreeRTOS，所以 HAL 的 1ms 时基（SysTick）必须自己
 *          提供：之前缺了这个函数，SysTick 落到 startup 里的弱符号 Default_Handler
 *          （死循环），HAL_Init() 之后第一次 tick 就把设备挂住，屏幕自然不亮。
 ******************************************************************************
 */
#include "boot.h"

/* 故障路径里不能用 HAL_Delay（它依赖 SysTick），用简单忙等 */
static void panic_wait(uint32_t loops)
{
    volatile uint32_t n = loops;
    while (n-- > 0U)
    {
        __NOP();
    }
}

/**
 * @brief  兜底：屏幕填暗红 + LED 闪 (stage+1) 次后停顿，循环往复。
 *         不接调试器时数 LED 闪烁次数就能知道死在哪个阶段。
 */
void Boot_Panic(uint32_t stage)
{
    Boot_Lcd_Backlight(true);

    if (g_boot_lcd_ready != 0U)
    {
        Boot_Lcd_Fill(0U, 0U, 319U, 239U, 0x8000U);
    }

    for (;;)
    {
        Boot_Led(false);
        panic_wait(6000000U);

        for (uint32_t i = 0U; i <= stage && i < 8U; i++)
        {
            Boot_Led(true);
            panic_wait(600000U);
            Boot_Led(false);
            panic_wait(400000U);
        }
    }
}

/* HAL 时基（bootloader 唯一使用的中断） */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

void HardFault_Handler(void)
{
    Boot_Panic(g_boot_stage);
}

void MemManage_Handler(void)
{
    Boot_Panic(g_boot_stage);
}

void BusFault_Handler(void)
{
    Boot_Panic(g_boot_stage);
}

void UsageFault_Handler(void)
{
    Boot_Panic(g_boot_stage);
}
