/**
 ******************************************************************************
 * @file    boot_app.c
 * @brief   应用镜像的合法性检查与跳转。
 ******************************************************************************
 */
#include "boot.h"
#include "stm32f4xx_hal.h"

static bool read_vector(uint32_t offset, uint32_t *value)
{
    uint32_t addr = BOOT_APP_ADDR + offset;

    if ((addr + 4U) > (BOOT_APP_ADDR + BOOT_APP_SIZE))
    {
        return false;
    }
    *value = *(const volatile uint32_t *)addr;
    return true;
}

bool Boot_App_Valid(void)
{
    uint32_t sp;
    uint32_t pc;

    if (!read_vector(0U, &sp) || !read_vector(4U, &pc))
    {
        return false;
    }

    /* 初始栈指针必须落在 SRAM 区间内 */
    if (sp < BOOT_RAM_START || sp > BOOT_RAM_END)
    {
        return false;
    }

    /* 复位向量必须落在应用分区内，且是 Thumb 地址 */
    if (pc < (BOOT_APP_ADDR + 0x100U) || pc >= (BOOT_APP_ADDR + BOOT_APP_SIZE))
    {
        return false;
    }

    return (pc & 1U) != 0U;
}

void Boot_App_Jump(void)
{
    uint32_t sp;
    uint32_t pc;

    if (!read_vector(0U, &sp) || !read_vector(4U, &pc))
    {
        return;
    }

    Boot_Led(false);

    /* 撤掉 bootloader 建立的现场：时基、外设中断、向量表 */
    __disable_irq();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;
    SCB->ICSR     = SCB_ICSR_PENDSTCLR_Msk;

    for (uint32_t i = 0U; i < 8U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    SCB->VTOR = BOOT_APP_ADDR;
    __DSB();
    __ISB();
    __set_MSP(sp);
    ((void (*)(void))pc)();

    /* 正常情况下不会返回 */
    while (1)
    {
    }
}
