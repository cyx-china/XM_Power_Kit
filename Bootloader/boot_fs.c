/**
 ******************************************************************************
 * @file    boot_fs.c
 * @brief   bootloader 的 FATFS 适配层：极简 diskio（直连轮询 SPI Flash）+ 裸机
 *          互斥量空实现（ffconf.h 里 FF_FS_REENTRANT=1，但 bootloader 是单线程）。
 ******************************************************************************
 */
#include "boot.h"
#include "ff.h"
#include "diskio.h"

#define BOOT_SECTOR_SIZE    4096U

static FATFS   s_fs;
static DSTATUS s_status = STA_NOINIT;

/* ============================ diskio ============================ */
DSTATUS disk_status(BYTE pdrv)
{
    (void)pdrv;
    return s_status;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    (void)pdrv;

    if (!Boot_Flash_Init())
    {
        s_status = STA_NOINIT;
    }
    else
    {
        s_status = 0U;
    }
    return s_status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    (void)pdrv;

    if (s_status & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    Boot_Flash_Read((uint32_t)sector * BOOT_SECTOR_SIZE, buff, count * BOOT_SECTOR_SIZE);
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    uint32_t addr = (uint32_t)sector * BOOT_SECTOR_SIZE;

    (void)pdrv;

    if (s_status & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    /* FATFS 的写永远是整扇区（FF_FS_TINY=1 时窗口在 FATFS 对象里），不需要读-改-写 */
    for (UINT i = 0U; i < count; i++)
    {
        if (!Boot_Flash_EraseSector(addr) ||
            !Boot_Flash_Write(addr, buff + ((uint32_t)i * BOOT_SECTOR_SIZE), BOOT_SECTOR_SIZE))
        {
            return RES_ERROR;
        }
        addr += BOOT_SECTOR_SIZE;
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    (void)pdrv;

    if (s_status & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    switch (cmd)
    {
    case CTRL_SYNC:
        return RES_OK;

    case GET_SECTOR_COUNT:
        *(LBA_t *)buff = (LBA_t)(Boot_Flash_Size() / BOOT_SECTOR_SIZE);
        return RES_OK;

    case GET_SECTOR_SIZE:
        *(WORD *)buff = BOOT_SECTOR_SIZE;
        return RES_OK;

    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1U;
        return RES_OK;

    default:
        return RES_PARERR;
    }
}

/* ============================ 裸机互斥量 ============================ */
#if FF_FS_REENTRANT
int ff_mutex_create(int vol)
{
    (void)vol;
    return 1;
}

void ff_mutex_delete(int vol)
{
    (void)vol;
}

int ff_mutex_take(int vol)
{
    (void)vol;
    return 1;
}

void ff_mutex_give(int vol)
{
    (void)vol;
}
#endif

/* ============================ 挂载 ============================ */
bool Boot_Fs_Mount(void)
{
    DIR dir;

    if (f_mount(&s_fs, "0:", 1) != FR_OK)
    {
        return false;
    }

    /* 真正读一次根目录，确认 FAT 和外部 Flash 都可用 */
    if (f_opendir(&dir, "0:/") != FR_OK)
    {
        return false;
    }
    f_closedir(&dir);

    return true;
}
