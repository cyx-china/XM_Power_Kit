/**
 ******************************************************************************
 * @file    boot_img.c
 * @brief   升级包解析 + 内部 Flash 搬运（擦除 / 按字写 / 回读 CRC 校验）。
 ******************************************************************************
 */
#include "boot.h"
#include "ff.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define CHUNK_SIZE  512U

static uint8_t s_buf[CHUNK_SIZE];

/* ---------------------------- 内部 Flash 扇区 ---------------------------- */
static uint32_t sector_index_of(uint32_t addr)
{
    if (addr < 0x08004000U) { return 0U; }
    if (addr < 0x08008000U) { return 1U; }
    if (addr < 0x0800C000U) { return 2U; }
    if (addr < 0x08010000U) { return 3U; }
    if (addr < 0x08020000U) { return 4U; }
    if (addr < 0x08040000U) { return 5U; }
    if (addr < 0x08060000U) { return 6U; }
    return 7U;
}

static void clear_flash_flags(void)
{
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
}

/* ---------------------------- 读文件 + 校验 ---------------------------- */
bool Boot_Pkg_Load(const char *path, boot_pkg_head_t *head)
{
    FIL file;
    UINT read = 0U;
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t left;

    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        return false;
    }

    if (f_read(&file, head, BOOT_PKG_HEAD_SIZE, &read) != FR_OK || read != BOOT_PKG_HEAD_SIZE)
    {
        (void)f_close(&file);
        return false;
    }

    if (memcmp(head->magic, BOOT_PKG_MAGIC, 8U) != 0 ||
        head->head_crc != Boot_Crc32(head, 48U) ||
        head->load_addr != BOOT_APP_ADDR ||
        head->image_size == 0U ||
        head->image_size > BOOT_APP_SIZE)
    {
        (void)f_close(&file);
        return false;
    }

    /* 机型串：允许留空，写了就必须匹配 */
    if (head->board[0] != '\0' &&
        memcmp(head->board, BOOT_PKG_BOARD, strlen(BOOT_PKG_BOARD)) != 0)
    {
        (void)f_close(&file);
        return false;
    }

    left = head->image_size;
    while (left > 0U)
    {
        UINT want = (left > CHUNK_SIZE) ? CHUNK_SIZE : (UINT)left;

        if (f_read(&file, s_buf, want, &read) != FR_OK || read != want)
        {
            (void)f_close(&file);
            return false;
        }
        crc = Boot_Crc32_Update(crc, s_buf, want);
        left -= want;
    }

    (void)f_close(&file);

    return crc == head->image_crc;
}

/* ---------------------------- 搬运算法 ---------------------------- */
bool Boot_Pkg_Flash(const char *path, const boot_pkg_head_t *head, boot_progress_t progress)
{
    FIL file;
    UINT read = 0U;
    FRESULT fr;
    uint32_t addr = BOOT_APP_ADDR;
    uint32_t end = BOOT_APP_ADDR + head->image_size;
    uint32_t write_end = (end + 3U) & ~3U;      /* 按字写，末尾补齐 */
    uint32_t first_sector = sector_index_of(BOOT_APP_ADDR);
    uint32_t last_sector = sector_index_of(write_end - 1U);
    uint32_t sector_error = 0U;
    FLASH_EraseInitTypeDef erase = {0};
    bool ok = true;

    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        return false;
    }
    if (f_lseek(&file, BOOT_PKG_HEAD_SIZE) != FR_OK)
    {
        (void)f_close(&file);
        return false;
    }

    /* ---- 擦除 ---- */
    if (progress != NULL)
    {
        progress(0U, 0U);
    }

    HAL_FLASH_Unlock();
    clear_flash_flags();

    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.NbSectors    = 1U;

    for (uint32_t sector = first_sector; ok && sector <= last_sector; sector++)
    {
        erase.Sector = sector;
        if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
        {
            ok = false;
            break;
        }
        if (progress != NULL)
        {
            progress(0U, (uint16_t)(((sector - first_sector + 1U) * 100U) / (last_sector - first_sector + 1U)));
        }
    }

    /* ---- 写入 ---- */
    while (ok && addr < end)
    {
        UINT want = (UINT)(((end - addr) > CHUNK_SIZE) ? CHUNK_SIZE : (end - addr));
        uint32_t padded = (want + 3U) & ~3U;

        fr = f_read(&file, s_buf, want, &read);
        if (fr != FR_OK || read != want)
        {
            ok = false;
            break;
        }

        for (uint32_t i = want; i < padded; i++)
        {
            s_buf[i] = 0xFFU;
        }

        for (uint32_t i = 0U; i < padded; i += 4U)
        {
            uint32_t word = (uint32_t)s_buf[i] |
                            ((uint32_t)s_buf[i + 1U] << 8) |
                            ((uint32_t)s_buf[i + 2U] << 16) |
                            ((uint32_t)s_buf[i + 3U] << 24);
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK)
            {
                ok = false;
                break;
            }
        }

        if (!ok)
        {
            break;
        }

        addr += want;
        if (progress != NULL)
        {
            progress(1U, (uint16_t)(((addr - BOOT_APP_ADDR) * 100U) / head->image_size));
        }
    }

    HAL_FLASH_Lock();
    (void)f_close(&file);

    if (!ok)
    {
        return false;
    }

    /* ---- 回读校验 ---- */
    if (progress != NULL)
    {
        progress(2U, 0U);
    }

    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t a = BOOT_APP_ADDR; a < end; a += CHUNK_SIZE)
    {
        uint32_t n = ((end - a) > CHUNK_SIZE) ? CHUNK_SIZE : (end - a);

        crc = Boot_Crc32_Update(crc, (const void *)a, n);
        if (progress != NULL)
        {
            progress(2U, (uint16_t)(((a - BOOT_APP_ADDR) * 100U) / head->image_size));
        }
    }

    if (progress != NULL)
    {
        progress(2U, 100U);
    }

    return crc == head->image_crc;
}
