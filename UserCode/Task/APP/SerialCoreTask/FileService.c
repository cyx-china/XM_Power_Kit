/**
******************************************************************************
  * @file           : FileService.c
  * @brief          : 上位机文件管理服务（外部 Flash 上的 FATFS）
  * @date           : 2026/11/08
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * 支持：列目录、读文件、写文件（按偏移分块）、删除、重命名、新建目录、
  *       属性查询、容量查询、图形层缓存刷新。
  * 说明：
  *   1. FatFs 已开启 FF_FS_REENTRANT，卷级互斥由文件系统内部完成；
  *   2. 图形层（graphics.c）会常驻打开字体/图片文件（FF_FS_LOCK），
  *      因此在覆盖写、删除、重命名前先请求它释放文件句柄。
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  ******************************************************************************
  */

#include "SerialCoreTask.h"

#include <string.h>

#include "ff.h"

#ifdef BOOTLOADER
#include "boot.h"
#else
#include "cmsis_os2.h"
#include "graphics.h"
#endif

#define FS_PATH_MAX     128U
#define FS_RSP_MAX      (PROTO_MAX_PAYLOAD - 1U)     // 应答里还要放 1 字节状态码
#define FS_LIST_MAX     24U                          // 单页最多条目数
#define GFX_RELEASE_WAIT_MS 200U

static char s_path[FS_PATH_MAX];
static char s_path2[FS_PATH_MAX];

/* ============================ 内部工具 ============================ */

/* 把主机发来的路径规范成 "0:/xxx" 形式，并做基本安全检查 */
static uint8_t fs_norm(const uint8_t *src, uint16_t len, char *dst) {
    if (src == NULL || dst == NULL) {
        return ST_E_PARAM;
    }

    uint16_t i = 0U;
    if (len >= 2U && src[0] == '0' && src[1] == ':') {
        i = 2U;
    }
    while (i < len && src[i] == '/') {
        i++;
    }
    uint16_t rest = (uint16_t)(len - i);
    if (rest >= (FS_PATH_MAX - 3U)) {
        return ST_E_PARAM;
    }

    dst[0] = '0';
    dst[1] = ':';
    dst[2] = '/';
    for (uint16_t k = 0U; k < rest; k++) {
        char c = (char)src[i + k];
        if (c == '\\') {
            c = '/';
        }
        if (c == ':' || (uint8_t)c < 0x20U) {
            return ST_E_PARAM;
        }
        dst[3U + k] = c;
    }
    dst[3U + rest] = '\0';

    if (strstr(dst, "..") != NULL) {
        return ST_E_PARAM;
    }
    return ST_OK;
}

static uint8_t fs_map_result(FRESULT fr) {
    switch (fr) {
        case FR_OK:                 return ST_OK;
        case FR_NO_FILE:
        case FR_NO_PATH:
        case FR_INVALID_NAME:
        case FR_INVALID_OBJECT:     return ST_E_NOTFOUND;
        case FR_DENIED:
        case FR_LOCKED:
        case FR_TOO_MANY_OPEN_FILES:return ST_E_BUSY;
        case FR_NOT_ENOUGH_CORE:    return ST_E_TOOBIG;
        default:                    return ST_E_FS;
    }
}

/* 请求图形层关闭常驻文件并清空字模缓存，避免覆盖/删除文件时被占用 */
void FileService_InvalidateGfx(void) {
#ifdef BOOTLOADER
    /* bootloader 没有图形层缓存，什么也不用做 */
#else
    GFX_RequestCacheInvalidate();
    uint32_t t0 = osKernelGetTickCount();
    while (!GFX_CacheInvalidateDone()) {
        if ((uint32_t)(osKernelGetTickCount() - t0) > GFX_RELEASE_WAIT_MS) {
            break;                  // 超时也继续，失败会由 f_unlink 返回 FR_LOCKED
        }
        osDelay(2U);
    }
#endif
}

bool FileService_GetCapacity(uint32_t *total_kb, uint32_t *free_kb) {
    DWORD nclst = 0U;
    FATFS *fsp = NULL;

    if (f_getfree("0:", &nclst, &fsp) != FR_OK || fsp == NULL) {
        if (total_kb != NULL) { *total_kb = 0U; }
        if (free_kb != NULL)  { *free_kb  = 0U; }
        return false;
    }
    if (total_kb != NULL) {
        *total_kb = (uint32_t)(((DWORD)(fsp->n_fatent - 2) * (DWORD)fsp->csize) / 2UL);
    }
    if (free_kb != NULL) {
        *free_kb = (uint32_t)((nclst * (DWORD)fsp->csize) / 2UL);
    }
    return true;
}

/* ============================ 各命令实现 ============================ */

/* 列目录：请求 [len][path][start u16][max u8]
 * 应答 [count u8][next u16][ (attr u8)(size u32)(namelen u8)(name) ... ] */
static uint8_t fs_list(const uint8_t *p, uint16_t n, uint8_t *out, uint16_t *out_len) {
    if (n < 4U) {
        return ST_E_PARAM;
    }
    uint8_t plen = p[0];
    if ((uint16_t)(1U + plen + 3U) > n) {
        return ST_E_PARAM;
    }
    uint8_t st = fs_norm(&p[1], plen, s_path);
    if (st != ST_OK) {
        return st;
    }

    uint16_t start = ser_get_u16(&p[1U + plen]);
    uint8_t  maxn  = p[1U + plen + 2U];
    if (maxn == 0U || maxn > FS_LIST_MAX) {
        maxn = FS_LIST_MAX;
    }

    DIR     dir;
    FILINFO fno;
    FRESULT fr = f_opendir(&dir, s_path);
    if (fr != FR_OK) {
        out[0] = (uint8_t)fr;
        *out_len = 1U;
        return fs_map_result(fr);
    }

    uint16_t idx = 0U;
    uint16_t pos = 3U;              // 0:count 1..2:next
    uint8_t  cnt = 0U;
    bool     finished = false;

    for (;;) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK) {
            break;
        }
        if (fno.fname[0] == '\0') {
            finished = true;
            break;
        }
        if (idx < start) {
            idx++;
            continue;
        }
        if (cnt >= maxn) {
            break;
        }
        uint16_t namelen = (uint16_t)strlen(fno.fname);
        if ((uint32_t)pos + 6U + namelen > FS_RSP_MAX) {
            break;                  // 本页装不下，下次从 start+cnt 继续
        }
        out[pos++] = (uint8_t)fno.fattrib;
        pos += ser_put_u32(&out[pos], (uint32_t)fno.fsize);
        out[pos++] = (uint8_t)namelen;
        memcpy(&out[pos], fno.fname, namelen);
        pos = (uint16_t)(pos + namelen);
        idx++;
        cnt++;
    }
    (void)f_closedir(&dir);

    if (fr != FR_OK) {
        out[0] = (uint8_t)fr;
        *out_len = 1U;
        return fs_map_result(fr);
    }

    out[0] = cnt;
    uint16_t next = finished ? 0xFFFFU : (uint16_t)(start + cnt);
    (void)ser_put_u16(&out[1], next);
    *out_len = pos;
    return ST_OK;
}

/* 读文件：请求 [len][path][offset u32][len u16]，应答 [len u16][data] */
static uint8_t fs_read(const uint8_t *p, uint16_t n, uint8_t *out, uint16_t *out_len) {
    if (n < 7U) {
        return ST_E_PARAM;
    }
    uint8_t plen = p[0];
    if ((uint16_t)(1U + plen + 6U) > n) {
        return ST_E_PARAM;
    }
    uint8_t st = fs_norm(&p[1], plen, s_path);
    if (st != ST_OK) {
        return st;
    }

    uint32_t offset = ser_get_u32(&p[1U + plen]);
    uint16_t want   = ser_get_u16(&p[1U + plen + 4U]);
    if (want > FS_CHUNK_MAX) {
        want = FS_CHUNK_MAX;
    }

    FIL fil;
    UINT br = 0U;
    FRESULT fr = f_open(&fil, s_path, FA_READ);
    if (fr != FR_OK) {
        out[0] = (uint8_t)fr;
        *out_len = 1U;
        return fs_map_result(fr);
    }
    fr = f_lseek(&fil, offset);
    if (fr == FR_OK) {
        fr = f_read(&fil, &out[2], want, &br);
    }
    (void)f_close(&fil);

    if (fr != FR_OK) {
        out[0] = (uint8_t)fr;
        *out_len = 1U;
        return fs_map_result(fr);
    }
    (void)ser_put_u16(&out[0], (uint16_t)br);
    *out_len = (uint16_t)(2U + br);
    return ST_OK;
}

/* 写文件：请求 [len][path][offset u32][len u16][data]，应答 [written u16] */
static uint8_t fs_write(const uint8_t *p, uint16_t n, uint8_t *out, uint16_t *out_len) {
    if (n < 7U) {
        return ST_E_PARAM;
    }
    uint8_t plen = p[0];
    if ((uint16_t)(1U + plen + 6U) > n) {
        return ST_E_PARAM;
    }
    uint8_t st = fs_norm(&p[1], plen, s_path);
    if (st != ST_OK) {
        return st;
    }

    uint32_t offset = ser_get_u32(&p[1U + plen]);
    uint16_t wlen   = ser_get_u16(&p[1U + plen + 4U]);
    if (wlen > FS_CHUNK_MAX) {
        return ST_E_TOOBIG;
    }
    if ((uint32_t)1U + plen + 6U + wlen > n) {
        return ST_E_PARAM;
    }
    const uint8_t *src = &p[1U + plen + 6U];

    if (offset == 0U) {
        FileService_InvalidateGfx();        // 覆盖已有文件前先让图形层放手
    }

    FIL fil;
    UINT bw = 0U;
    BYTE mode = (offset == 0U) ? (BYTE)(FA_CREATE_ALWAYS | FA_WRITE)
                               : (BYTE)(FA_OPEN_EXISTING | FA_WRITE);
    FRESULT fr = f_open(&fil, s_path, mode);
    if (fr == FR_OK && offset > 0U) {
        fr = f_lseek(&fil, offset);
    }
    if (fr == FR_OK) {
        fr = f_write(&fil, src, wlen, &bw);
    }
    /* f_close 才真正把窗口缓冲/目录项落盘，它的错误不能被吞掉 */
    FRESULT fc = f_close(&fil);
    if (fr == FR_OK) {
        fr = fc;
    }

    if (fr != FR_OK) {
        out[0] = (uint8_t)fr;
        *out_len = 1U;
        return fs_map_result(fr);
    }
    (void)ser_put_u16(&out[0], (uint16_t)bw);
    *out_len = 2U;
    return ST_OK;
}

/* 删除：请求 [len][path] */
static uint8_t fs_delete(const uint8_t *p, uint16_t n, uint8_t *out, uint16_t *out_len) {
    if (n < 1U) {
        return ST_E_PARAM;
    }
    uint8_t plen = p[0];
    if ((uint16_t)(1U + plen) > n) {
        return ST_E_PARAM;
    }
    uint8_t st = fs_norm(&p[1], plen, s_path);
    if (st != ST_OK) {
        return st;
    }
    if (strcmp(s_path, "0:/") == 0) {
        return ST_E_PARAM;                  // 不允许删根目录
    }

    FileService_InvalidateGfx();
    FRESULT fr = f_unlink(s_path);
    if (fr != FR_OK) {
        out[0] = (uint8_t)fr;
        *out_len = 1U;
    }
    return fs_map_result(fr);
}

/* 重命名：请求 [len][old][len][new] */
static uint8_t fs_rename(const uint8_t *p, uint16_t n, uint8_t *out, uint16_t *out_len) {
    if (n < 2U) {
        return ST_E_PARAM;
    }
    uint8_t l1 = p[0];
    if ((uint16_t)(1U + l1 + 1U) > n) {
        return ST_E_PARAM;
    }
    uint8_t l2 = p[1U + l1];
    if ((uint16_t)(2U + l1 + l2) > n) {
        return ST_E_PARAM;
    }
    uint8_t st = fs_norm(&p[1], l1, s_path);
    if (st == ST_OK) {
        st = fs_norm(&p[2U + l1], l2, s_path2);
    }
    if (st != ST_OK) {
        return st;
    }

    FileService_InvalidateGfx();
    FRESULT fr = f_rename(s_path, s_path2);
    if (fr != FR_OK) {
        out[0] = (uint8_t)fr;
        *out_len = 1U;
    }
    return fs_map_result(fr);
}

/* 新建目录：请求 [len][path] */
static uint8_t fs_mkdir(const uint8_t *p, uint16_t n, uint8_t *out, uint16_t *out_len) {
    if (n < 1U) {
        return ST_E_PARAM;
    }
    uint8_t plen = p[0];
    if ((uint16_t)(1U + plen) > n) {
        return ST_E_PARAM;
    }
    uint8_t st = fs_norm(&p[1], plen, s_path);
    if (st != ST_OK) {
        return st;
    }
    FRESULT fr = f_mkdir(s_path);
    if (fr != FR_OK) {
        out[0] = (uint8_t)fr;
        *out_len = 1U;
    }
    return fs_map_result(fr);
}

/* 属性：请求 [len][path]，应答 [attr u8][size u32] */
static uint8_t fs_stat(const uint8_t *p, uint16_t n, uint8_t *out, uint16_t *out_len) {
    if (n < 1U) {
        return ST_E_PARAM;
    }
    uint8_t plen = p[0];
    if ((uint16_t)(1U + plen) > n) {
        return ST_E_PARAM;
    }
    uint8_t st = fs_norm(&p[1], plen, s_path);
    if (st != ST_OK) {
        return st;
    }
    FILINFO fno;
    FRESULT fr = f_stat(s_path, &fno);
    if (fr != FR_OK) {
        out[0] = (uint8_t)fr;
        *out_len = 1U;
        return fs_map_result(fr);
    }
    out[0] = (uint8_t)fno.fattrib;
    (void)ser_put_u32(&out[1], (uint32_t)fno.fsize);
    *out_len = 5U;
    return ST_OK;
}

/* 容量：应答 [total_kb u32][free_kb u32] */
static uint8_t fs_info(uint8_t *out, uint16_t *out_len) {
    uint32_t total = 0U;
    uint32_t freekb = 0U;
    if (!FileService_GetCapacity(&total, &freekb)) {
        return ST_E_FS;
    }
    (void)ser_put_u32(&out[0], total);
    (void)ser_put_u32(&out[4], freekb);
    *out_len = 8U;
    return ST_OK;
}

/* ============================ 分发入口 ============================ */
uint8_t FileService_Handle(const ProtoFrame_t *req, uint8_t *out, uint16_t *out_len) {
    if (req == NULL || out == NULL || out_len == NULL) {
        return ST_E_PARAM;
    }
    *out_len = 0U;

    switch (req->cmd) {
        case CMD_FS_LIST:   return fs_list(req->data, req->len, out, out_len);
        case CMD_FS_READ:   return fs_read(req->data, req->len, out, out_len);
        case CMD_FS_WRITE:  return fs_write(req->data, req->len, out, out_len);
        case CMD_FS_DELETE: return fs_delete(req->data, req->len, out, out_len);
        case CMD_FS_RENAME: return fs_rename(req->data, req->len, out, out_len);
        case CMD_FS_MKDIR:  return fs_mkdir(req->data, req->len, out, out_len);
        case CMD_FS_STAT:   return fs_stat(req->data, req->len, out, out_len);
        case CMD_FS_INFO:   return fs_info(out, out_len);
        case CMD_FS_SYNC:
            FileService_InvalidateGfx();
            return ST_OK;
        default:
            return ST_E_UNSUPPORTED;
    }
}
