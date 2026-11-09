/**
 ******************************************************************************
 * @file    boot_img.c
 * @brief   升级包解析 + 内部 Flash 搬运（擦除 / 按字写 / 回读 CRC 校验）。
 *          支持两种输入：
 *            1) 带 80 字节包头的升级包（.xmkota / FW.BIN）
 *            2) 裸 .bin（直接烧编译产物，没有包头，版本串从镜像里嗅出来）
 ******************************************************************************
 */
#include "boot.h"
#include "ff.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define CHUNK_SIZE  512U
#define RAW_MIN_SIZE 1024U      /* 裸镜像至少这么大才当成固件 */

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

/* ---------------------------- 版本串嗅探 ---------------------------- */
/* 裸 .bin 没有包头，版本串就在 .rodata 里（"STM32F407VE.261108.Rel.0007"）。
   一边算 CRC 一边扫，不额外读文件。 */
static const char VER_PREFIX[] = "STM32F407VE.";

typedef struct
{
    uint8_t match;      /* 已匹配的前缀长度 */
    uint8_t len;        /* 已收集的版本串长度 */
    uint8_t done;
    char    text[32];
} ver_scan_t;

static bool ver_char(uint8_t c)
{
    return ((c >= '0') && (c <= '9')) ||
           ((c >= 'A') && (c <= 'Z')) ||
           ((c >= 'a') && (c <= 'z')) || (c == '.');
}

static void ver_feed(ver_scan_t *s, const uint8_t *p, uint32_t n)
{
    const uint8_t pfx = (uint8_t)(sizeof(VER_PREFIX) - 1U);

    for (uint32_t i = 0U; i < n; i++)
    {
        uint8_t c = p[i];

        if (s->done != 0U)
        {
            return;
        }

        if (s->len > 0U)                            /* 正在收集版本串 */
        {
            if (ver_char(c) && (s->len < (uint8_t)(sizeof(s->text) - 1U)))
            {
                s->text[s->len++] = (char)c;
            }
            else
            {
                s->text[s->len] = '\0';
                s->done = 1U;
            }
            continue;
        }

        if (c == (uint8_t)VER_PREFIX[s->match])
        {
            if (++s->match == pfx)
            {
                memcpy(s->text, VER_PREFIX, pfx);
                s->len   = pfx;
                s->match = 0U;
            }
        }
        else
        {
            s->match = (c == (uint8_t)VER_PREFIX[0]) ? 1U : 0U;
        }
    }
}

/* ---------------------------- 解析升级包 ---------------------------- */
bool Boot_Pkg_Load(const char *path, boot_pkg_info_t *info)
{
    FIL     file;
    UINT    read = 0U;
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t left;
    boot_pkg_head_t head;
    ver_scan_t scan;
    bool has_head = false;

    if (info == NULL)
    {
        return false;
    }
    memset(info, 0, sizeof(*info));
    info->reason = BOOT_PKG_E_OPEN;

    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        return false;
    }

    info->file_size = (uint32_t)f_size(&file);

    /* --- 先看成不成升级包 --- */
    if (info->file_size >= BOOT_PKG_HEAD_SIZE)
    {
        if ((f_read(&file, &head, BOOT_PKG_HEAD_SIZE, &read) != FR_OK) ||
            (read != BOOT_PKG_HEAD_SIZE))
        {
            (void)f_close(&file);
            info->reason = BOOT_PKG_E_READ;
            return false;
        }

        if (memcmp(head.magic, BOOT_PKG_MAGIC, 8U) == 0)
        {
            info->reason = BOOT_PKG_E_HEAD;

            if ((head.head_crc != Boot_Crc32(&head, 48U)) ||
                (head.load_addr != BOOT_APP_ADDR) ||
                (head.image_size == 0U) ||
                (head.image_size > BOOT_APP_SIZE) ||
                ((head.board[0] != '\0') &&
                 (memcmp(head.board, BOOT_PKG_BOARD, strlen(BOOT_PKG_BOARD)) != 0)))
            {
                (void)f_close(&file);
                return false;
            }

            has_head          = true;
            info->has_head    = 1U;
            info->data_offset = BOOT_PKG_HEAD_SIZE;
            info->image_size  = head.image_size;
            info->want_crc    = head.image_crc;

            for (uint32_t i = 0U; i < (sizeof(info->version) - 1U) && head.version[i] != '\0'; i++)
            {
                info->version[i] = head.version[i];
            }
        }
    }

    /* --- 不是升级包就当成裸 .bin --- */
    if (!has_head)
    {
        info->image_size = info->file_size;
        info->want_crc   = 0U;
        info->version[0] = '\0';
        info->reason     = BOOT_PKG_E_EMPTY;

        if ((info->image_size < RAW_MIN_SIZE) || (info->image_size > BOOT_APP_SIZE))
        {
            (void)f_close(&file);
            return false;
        }
        if (f_lseek(&file, 0U) != FR_OK)
        {
            (void)f_close(&file);
            info->reason = BOOT_PKG_E_READ;
            return false;
        }
        memset(&scan, 0, sizeof(scan));
    }

    /* --- 算镜像 CRC（裸镜像顺手嗅版本串） --- */
    left = info->image_size;
    while (left > 0U)
    {
        UINT want = (left > CHUNK_SIZE) ? CHUNK_SIZE : (UINT)left;

        if ((f_read(&file, s_buf, want, &read) != FR_OK) || (read != want))
        {
            (void)f_close(&file);
            info->reason = BOOT_PKG_E_READ;
            return false;
        }
        crc = Boot_Crc32_Update(crc, s_buf, want);
        if (!has_head)
        {
            ver_feed(&scan, s_buf, want);
        }
        left -= want;
    }
    (void)f_close(&file);

    info->got_crc = crc;

    if (!has_head)
    {
        info->want_crc = crc;                       /* 裸镜像：自己算的就是标准 */
        for (uint32_t i = 0U; (i < (sizeof(info->version) - 1U)) && (scan.text[i] != '\0'); i++)
        {
            info->version[i] = scan.text[i];
        }
        info->reason = BOOT_PKG_OK;
        return true;
    }

    if (info->got_crc != info->want_crc)
    {
        info->reason = BOOT_PKG_E_CRC;
        return false;
    }

    info->reason = BOOT_PKG_OK;
    return true;
}

/* ---------------------------- 搬运算法 ---------------------------- */
bool Boot_Pkg_Flash(const char *path, const boot_pkg_info_t *info, boot_progress_t progress)
{
    FIL file;
    UINT read = 0U;
    FRESULT fr;
    uint32_t addr = BOOT_APP_ADDR;
    uint32_t end = BOOT_APP_ADDR + info->image_size;
    uint32_t write_end = (end + 3U) & ~3U;      /* 按字写，末尾补齐 */
    uint32_t first_sector = sector_index_of(BOOT_APP_ADDR);
    uint32_t last_sector = sector_index_of(write_end - 1U);
    bool ok = true;

    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        return false;
    }
    if (f_lseek(&file, info->data_offset) != FR_OK)
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

    for (uint32_t sector = first_sector; ok && sector <= last_sector; sector++)
    {
        if (!Boot_IntFlash_EraseSector(sector))
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
            progress(1U, (uint16_t)(((addr - BOOT_APP_ADDR) * 100U) / info->image_size));
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
            progress(2U, (uint16_t)(((a - BOOT_APP_ADDR) * 100U) / info->image_size));
        }
    }

    if (progress != NULL)
    {
        progress(2U, 100U);
    }

    return crc == info->want_crc;
}

/* ---------------------------- 找待安装的包 ---------------------------- */
static bool name_is_pkg(const char *n)
{
    /* 这几个是我们自己的控制文件，绝不能当固件刷 */
    if ((strcmp(n, "PREV.BIN") == 0) || (strcmp(n, "BOOTOK.BIN") == 0) || (strcmp(n, "REQ.BIN") == 0))
    {
        return false;
    }

    uint32_t len = (uint32_t)strlen(n);
    if (len < 5U)
    {
        return false;
    }

    const char *e = n + len - 4U;
    return (e[0] == '.') &&
           ((e[1] == 'B') || (e[1] == 'b')) &&
           ((e[2] == 'I') || (e[2] == 'i')) &&
           ((e[3] == 'N') || (e[3] == 'n'));
}

static bool join_ota(char *path, uint32_t size, const char *name)
{
    const char *dir = BOOT_DIR_OTA "/";
    uint32_t i = 0U;

    while ((dir[i] != '\0') && ((i + 1U) < size))
    {
        path[i] = dir[i];
        i++;
    }
    for (uint32_t k = 0U; (name[k] != '\0') && ((i + 1U) < size); k++)
    {
        path[i++] = name[k];
    }
    path[i] = '\0';

    return (i > 0U);
}

bool Boot_FindPackage(char *path, uint32_t size)
{
    FILINFO info;
    DIR     dir;
    bool    found = false;

    if ((path == NULL) || (size < 16U))
    {
        return false;
    }

    /* 优先标准名字 */
    if ((f_stat(BOOT_FILE_FW, &info) == FR_OK) && ((info.fattrib & AM_DIR) == 0U))
    {
        return join_ota(path, size, "FW.BIN");
    }

    /* 没有 FW.BIN 就在 /ota 里找第一个 .BIN（方便直接把编译产物丢进去） */
    if (f_opendir(&dir, BOOT_DIR_OTA) != FR_OK)
    {
        return false;
    }

    for (;;)
    {
        if ((f_readdir(&dir, &info) != FR_OK) || (info.fname[0] == '\0'))
        {
            break;
        }
        if ((info.fattrib & AM_DIR) != 0U)
        {
            continue;
        }
        if (!name_is_pkg(info.fname))
        {
            continue;
        }
        found = join_ota(path, size, info.fname);
        break;
    }

    (void)f_closedir(&dir);
    return found;
}
