/**
 ******************************************************************************
 * @file    boot_state.c
 * @brief   OTA 状态记录（内部 Flash sector1 @ 0x08004000）。
 *          只有 bootloader 会写这块扇区，应用不碰，避免两边擦除打架。
 ******************************************************************************
 */
#include "boot.h"
#include "stm32f4xx_hal.h"
#include <stddef.h>

/* ---------------------------- 擦扇区（直写 FLASH 寄存器） ---------------------------- */
#define SNB_SHIFT   3U

bool Boot_IntFlash_EraseSector(uint32_t sector)
{
    bool ok;

    FLASH->CR &= ~FLASH_CR_PSIZE;
    FLASH->CR |= FLASH_CR_PSIZE_1;                         /* x32 并行位 */
    FLASH->CR |= FLASH_CR_SER;
    FLASH->CR = (FLASH->CR & ~(0xFUL << SNB_SHIFT)) | (sector << SNB_SHIFT);
    FLASH->CR |= FLASH_CR_STRT;

    while ((FLASH->SR & FLASH_SR_BSY) != 0U)
    {
    }

    ok = (FLASH->SR & (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR |
                       FLASH_SR_PGPERR | FLASH_SR_PGSERR)) == 0U;

    FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB | FLASH_CR_STRT);
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_OPERR | FLASH_SR_WRPERR |
                FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR;

    return ok;
}

static uint32_t record_crc(const boot_record_t *rec)
{
    return Boot_Crc32(rec, (uint32_t)offsetof(boot_record_t, crc));
}

void Boot_State_Load(boot_record_t *rec)
{
    const boot_record_t *flash = (const boot_record_t *)BOOT_STATE_ADDR;

    *rec = *flash;

    if (rec->magic != BOOT_REC_MAGIC || rec->crc != record_crc(rec))
    {
        /* 空白或损坏：回落成“稳定、无失败”的默认记录（不写 Flash，等真正需要时再写） */
        rec->magic      = BOOT_REC_MAGIC;
        rec->serial     = 0U;
        rec->state      = BOOT_ST_IDLE;
        rec->fail_count = 0U;
        rec->version[0] = '\0';
        rec->token      = 0U;
        rec->crc        = record_crc(rec);
    }
}

bool Boot_State_Save(const boot_record_t *rec)
{
    boot_record_t tmp = *rec;
    const uint32_t *words;
    bool ok = true;

    tmp.crc = record_crc(&tmp);

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    if (!Boot_IntFlash_EraseSector(1U))                    /* sector1 = 状态记录 */
    {
        ok = false;
    }

    words = (const uint32_t *)&tmp;
    for (uint32_t i = 0U; ok && i < (sizeof(tmp) / 4U); i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, BOOT_STATE_ADDR + (i * 4U), words[i]) != HAL_OK)
        {
            ok = false;
        }
    }

    HAL_FLASH_Lock();
    return ok;
}
