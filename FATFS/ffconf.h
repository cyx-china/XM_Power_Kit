/*---------------------------------------------------------------------------/
/  Configurations of FatFs Module
/---------------------------------------------------------------------------*/
#pragma once

#define FFCONF_DEF  80386   /* Revision ID */

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_READONLY      0       /* 读写都支持 */
#define FF_FS_MINIMIZE      1       /* 禁用stat/getfree/unlink/mkdir/truncate/rename */
#define FF_USE_FIND         0       /* 不需模式搜索 */
#define FF_USE_MKFS         0       /* 不需格式化 */
#define FF_USE_FASTSEEK     1       /* LVGLseek频繁，保留 */
#define FF_USE_EXPAND       0
#define FF_USE_CHMOD        0
#define FF_USE_LABEL        0
#define FF_USE_FORWARD      0
#define FF_USE_STRFUNC      0       /* 禁用f_printf等 */

/*---------------------------------------------------------------------------/
/ Locale and Namespace Configurations
/---------------------------------------------------------------------------*/

#define FF_CODE_PAGE        437     /* LFN+Unicode时必须437 */
#define FF_USE_LFN          2       /* 启用LFN，用栈动态缓冲 */
#define FF_MAX_LFN          64      /* 最大LFN长度 */
#define FF_LFN_UNICODE      2       /* UTF-8编码，最兼容LVGL（TCHAR=char） */
#define FF_LFN_BUF          64      /* 与MAX_LFN匹配 */
#define FF_SFN_BUF          12
#define FF_FS_RPATH         0       /* 禁用相对路径 */

/*---------------------------------------------------------------------------/
/ Drive/Volume Configurations
/---------------------------------------------------------------------------*/

#define FF_VOLUMES          1       /* 只一个卷 */
#define FF_MULTI_PARTITION  0
#define FF_MIN_SS           4096    /* Flash 4K扇区 */
#define FF_MAX_SS           4096
#define FF_LBA64            0       /* SPI Flash不会>2TB */
#define FF_USE_TRIM         0       /* SPI Flash不支持TRIM */

/*---------------------------------------------------------------------------/
/ System Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_TINY          1       /* 文件对象只含有必要成员 */
#define FF_FS_EXFAT         0       /* Flash通常FAT32，不需exFAT */

#define FF_FS_NORTC         1       /* 无RTC，所有文件时间戳固定 */
#define FF_NORTC_MON        1
#define FF_NORTC_MDAY       1
#define FF_NORTC_YEAR       2026

#define FF_FS_CRTIME        0
#define FF_FS_NOFSINFO      0
#define FF_FS_LOCK          3
#define FF_FS_REENTRANT     0
#define _SYNC_t             SemaphoreHandle_t
#define FF_FS_TIMEOUT       pdMS_TO_TICKS(1000)

/*--- End of configuration options ---*/