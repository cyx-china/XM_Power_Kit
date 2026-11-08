/**
 ******************************************************************************
 * @file    boot_storage.c
 * @brief   出厂初始化：建目录、把内置字库写到外部 Flash、生成默认参数文件。
 *          新机第一次上电（或外部 Flash 内容被清空）时由 bootloader 自动完成，
 *          不需要连电脑，也不需要主程序参与。
 *          字库在 CMake 里用 objcopy 内嵌成 .rodata，符号 _binary_boot_font_bin_start。
 ******************************************************************************
 */
#include "boot.h"
#include "ff.h"
#include "boot_param_default.h"

/* 打包进固件的字库（Bootloader/boot_font.bin，符号由 boot_font_bin.o 提供） */
extern const uint8_t _binary_boot_font_bin_start[];
extern const uint8_t _binary_boot_font_bin_end[];

#define BLOB_SIZE ((uint32_t)(_binary_boot_font_bin_end - _binary_boot_font_bin_start))

static FILINFO s_info;

static bool write_file(const char *path, const void *data, uint32_t len)
{
    FIL  file;
    UINT bw   = 0U;
    FRESULT res;

    if (f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
    {
        return false;
    }

    res = f_write(&file, data, len, &bw);
    f_close(&file);

    return (res == FR_OK) && (bw == len);
}

/**
 * @brief  补齐出厂文件：目录、字库、参数文件
 * @return 本次真正做了哪些事（BOOT_PROV_xxx 位）
 */
uint8_t Boot_Storage_Provision(void)
{
    static const char *const dirs[] =
    {
        BOOT_DIR_FONT, BOOT_DIR_FONT_BOOT, BOOT_DIR_IMAGES, BOOT_DIR_OTA
    };
    uint8_t flags = 0U;

    for (uint32_t i = 0U; i < sizeof(dirs) / sizeof(dirs[0]); i++)
    {
        if (f_mkdir(dirs[i]) == FR_OK)
        {
            flags |= BOOT_PROV_DIRS;
        }
    }

    /* 字库：文件不存在或长度和内置字库不一致就重写（覆盖旧版本字库） */
    if ((f_stat(BOOT_FILE_FONT, &s_info) != FR_OK) || (s_info.fsize != (FSIZE_t)BLOB_SIZE))
    {
        if (write_file(BOOT_FILE_FONT, _binary_boot_font_bin_start, BLOB_SIZE))
        {
            flags |= BOOT_PROV_FONT;
        }
    }

    /* 参数文件：只在缺失时创建，绝不覆盖用户校准值 */
    if (f_stat(BOOT_FILE_PARAM, &s_info) != FR_OK)
    {
        if (write_file(BOOT_FILE_PARAM, boot_param_default, BOOT_PARAM_BYTES))
        {
            flags |= BOOT_PROV_PARAM;
        }
    }

    return flags;
}
