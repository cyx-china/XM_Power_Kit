/**
******************************************************************************
  * @file           : OtaService.c
  * @brief          : 应用侧 OTA 服务实现
  * @date           : 2026/11/08
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  ******************************************************************************
  */

#include "OtaService.h"

#include <string.h>

#include "ff.h"
#include "stm32f4xx_hal.h"
#include "UserDefineManage.h"
#include "UserTask.h"

/* 链接脚本里的应用镜像结束地址 */
extern uint32_t _image_end;

#define OTA_CHUNK           512U
#define OTA_REC_ADDR        0x08004000U
#define OTA_REC_MAGIC       0x374B4D58U      /* 'XMK7' */
#define OTA_ST_WAIT_BOOTOK  1U               /* bootloader 刚升级完，等应用确认 */

/* 与 bootloader 一致的内部 Flash 状态记录（只读，写由 bootloader 负责） */
typedef struct {
    uint32_t magic;
    uint32_t serial;
    uint32_t state;
    uint32_t fail_count;
    char     version[32];
    uint32_t token;
    uint32_t crc;
} ota_record_t;

static FIL        s_file;
static bool       s_open;
static bool       s_reboot;
static uint32_t   s_written;
static uint32_t   s_crc;
static ota_head_t s_head;
static uint8_t    s_buf[OTA_CHUNK];

/* ============================ CRC32 ============================ */
/* 16 项半字节查表：只占 64 B RAM（256 项表要 1 KB），速度对此处用途（每秒几十 KB）绰绰有余 */
static uint32_t s_crc_nibble[16];
static bool     s_crc_ready;

static void crc_init(void) {
    if (s_crc_ready) {
        return;
    }
    for (uint32_t i = 0U; i < 16U; i++) {
        uint32_t c = i;
        for (uint32_t k = 0U; k < 4U; k++) {
            c = (c & 1U) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
        }
        s_crc_nibble[i] = c;
    }
    s_crc_ready = true;
}

static uint32_t crc_update(uint32_t crc, const void *buf, uint32_t len) {
    const uint8_t *p = (const uint8_t *)buf;

    crc_init();
    crc = ~crc;
    while (len-- > 0U) {
        crc ^= *p++;
        crc = (crc >> 4) ^ s_crc_nibble[crc & 0x0FU];
        crc = (crc >> 4) ^ s_crc_nibble[crc & 0x0FU];
    }
    return ~crc;
}

/* ============================ 小工具 ============================ */
static uint32_t app_image_size(void) {
    return (uint32_t)&_image_end - OTA_APP_ADDR;
}

static void read_record(ota_record_t *rec) {
    const ota_record_t *flash = (const ota_record_t *)OTA_REC_ADDR;

    *rec = *flash;
    if (rec->magic != OTA_REC_MAGIC ||
        rec->crc != crc_update(0xFFFFFFFFU, rec, (uint32_t)offsetof(ota_record_t, crc))) {
        memset(rec, 0, sizeof(*rec));
    }
}

static uint32_t next_token(void) {
    ota_record_t rec;

    read_record(&rec);
    if (rec.magic != OTA_REC_MAGIC) {
        return 1U;
    }
    return rec.token + 1U;
}

static bool head_from_file(const char *path, ota_head_t *head) {
    FIL   file;
    UINT  br = 0U;
    bool  ok = false;

    if (f_open(&file, path, FA_READ) != FR_OK) {
        return false;
    }
    if (f_read(&file, head, OTA_HEAD_SIZE, &br) == FR_OK && br == OTA_HEAD_SIZE &&
        memcmp(head->magic, OTA_MAGIC, 8U) == 0) {
        ok = true;
    }
    (void)f_close(&file);
    return ok;
}

/* ============================ 写 BOOTOK / REQ ============================ */
static bool write_bootok(const char *version) {
    typedef struct {
        uint32_t magic;
        char     version[32];
        uint32_t token;
        uint32_t crc;
    } bootok_t;

    bootok_t ok;
    FIL      file;
    UINT     bw = 0U;

    memset(&ok, 0, sizeof(ok));
    ok.magic = 0x314B4F42U;                 /* 'BOK1' */
    strncpy(ok.version, version, sizeof(ok.version) - 1U);
    ok.token = next_token() - 1U;
    ok.crc   = crc_update(0xFFFFFFFFU, &ok, 40U);

    (void)f_mkdir(OTA_DIR);
    if (f_open(&file, OTA_FILE_BOOTOK, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        return false;
    }
    bool okw = (f_write(&file, &ok, sizeof(ok), &bw) == FR_OK && bw == sizeof(ok));
    (void)f_close(&file);
    return okw;
}

static bool write_req(uint16_t op) {
    typedef struct {
        uint32_t token;
        uint16_t op;
        uint16_t crc;
    } req_t;

    req_t req;
    FIL   file;
    UINT  bw = 0U;

    req.token = next_token();
    req.op    = op;
    req.crc   = (uint16_t)(crc_update(0xFFFFFFFFU, &req, 6U) & 0xFFFFU);

    (void)f_mkdir(OTA_DIR);
    if (f_open(&file, OTA_FILE_REQ, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        return false;
    }
    bool ok = (f_write(&file, &req, sizeof(req), &bw) == FR_OK && bw == sizeof(req));
    (void)f_close(&file);
    return ok;
}

/* ============================ 回滚快照 ============================ */
static bool snapshot_current(void) {
    ota_head_t head;
    FIL        file;
    UINT       bw = 0U;
    uint32_t   size = app_image_size();

    memset(&head, 0, sizeof(head));
    memcpy(head.magic, OTA_MAGIC, 8U);
    strncpy(head.version, SYS_VERSION_CODE, sizeof(head.version) - 1U);
    head.image_size = size;
    head.image_crc  = crc_update(0xFFFFFFFFU, (const void *)OTA_APP_ADDR, size);
    head.load_addr  = OTA_APP_ADDR;
    memcpy(head.board, OTA_BOARD, strlen(OTA_BOARD));
    head.head_crc   = crc_update(0xFFFFFFFFU, &head, 48U);

    if (f_open(&file, OTA_FILE_PREV, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        return false;
    }

    if (f_write(&file, &head, OTA_HEAD_SIZE, &bw) != FR_OK || bw != OTA_HEAD_SIZE) {
        (void)f_close(&file);
        return false;
    }

    for (uint32_t addr = OTA_APP_ADDR; addr < (OTA_APP_ADDR + size); addr += OTA_CHUNK) {
        uint32_t left = (OTA_APP_ADDR + size) - addr;
        uint32_t n = (left > OTA_CHUNK) ? OTA_CHUNK : left;

        memcpy(s_buf, (const void *)addr, n);
        if (f_write(&file, s_buf, n, &bw) != FR_OK || bw != n) {
            (void)f_close(&file);
            return false;
        }
    }

    (void)f_close(&file);
    return true;
}

/* ============================ 启动确认 ============================ */
void Ota_BootConfirm(void) {
    ota_record_t rec;

    read_record(&rec);
    if (rec.magic != OTA_REC_MAGIC || rec.state != OTA_ST_WAIT_BOOTOK) {
        return;                     /* 稳定运行：不碰外部 Flash，避免每次开机都擦写 */
    }

    /* bootloader 记的版本和当前跑的版本不一致，说明搬进去的镜像不是要升级的那个：
       不写 BOOTOK，让 bootloader 继续累计失败次数，够 3 次自动回滚到 PREV.BIN */
    if (strncmp(rec.version, SYS_VERSION_CODE, sizeof(rec.version)) != 0) {
        return;
    }

    /* 先写成功标记再清请求：中途掉电时 bootloader 也已经认为本次升级成功 */
    if (write_bootok(SYS_VERSION_CODE)) {
        /* BOOTOK.BIN 要留给 bootloader 读（它确认后自己删），这里不能删 */
        (void)f_unlink(OTA_FILE_REQ);
        UserParam_SaveAllValues();             /* 让参数与镜像保持一致 */
    }
}

/* ============================ 命令实现 ============================ */
static uint8_t ota_info(uint8_t *out, uint16_t *out_len) {
    FILINFO fno;
    ota_head_t head;
    ota_record_t rec;
    uint8_t  flags = 0U;
    uint16_t i = 0U;

    if (f_stat(OTA_FILE_PREV, &fno) == FR_OK)   { flags |= 0x01U; }
    if (f_stat(OTA_FILE_FW, &fno) == FR_OK)     { flags |= 0x02U; }
    if (f_stat(OTA_FILE_REQ, &fno) == FR_OK)    { flags |= 0x04U; }
    if (f_stat(OTA_FILE_BOOTOK, &fno) == FR_OK) { flags |= 0x08U; }

    read_record(&rec);

    out[i++] = flags;
    out[i++] = s_open ? 1U : 0U;                                  /* 是否有正在进行的上传 */
    i = (uint16_t)(i + ser_put_u32(&out[i], app_image_size()));

    memset(&head, 0, sizeof(head));
    (void)head_from_file(OTA_FILE_PREV, &head);
    memcpy(&out[i], head.version, 32U);  i = (uint16_t)(i + 32U);
    (void)ser_put_u32(&out[i], head.image_size);
    i = (uint16_t)(i + 4U);

    memset(&head, 0, sizeof(head));
    (void)head_from_file(OTA_FILE_FW, &head);
    memcpy(&out[i], head.version, 32U);  i = (uint16_t)(i + 32U);
    (void)ser_put_u32(&out[i], head.image_size); i = (uint16_t)(i + 4U);
    (void)ser_put_u32(&out[i], head.image_crc);  i = (uint16_t)(i + 4U);

    out[i++] = (uint8_t)((rec.magic == OTA_REC_MAGIC) ? ((rec.fail_count > 0xFFU) ? 0xFFU : rec.fail_count) : 0U);
    out[i++] = (uint8_t)((rec.magic == OTA_REC_MAGIC) ? rec.state : 0U);

    *out_len = i;
    return ST_OK;
}

static uint8_t ota_begin(const ProtoFrame_t *req, uint8_t *out, uint16_t *out_len) {
    UINT bw = 0U;

    if (req->len != OTA_HEAD_SIZE) {
        return ST_E_PARAM;
    }

    memcpy(&s_head, req->data, OTA_HEAD_SIZE);

    if (memcmp(s_head.magic, OTA_MAGIC, 8U) != 0) {
        return ST_E_PARAM;
    }
    if (s_head.head_crc != crc_update(0xFFFFFFFFU, &s_head, 48U)) {
        return ST_E_CRC;
    }
    if (s_head.load_addr != OTA_APP_ADDR) {
        return ST_E_PARAM;
    }
    if (s_head.image_size == 0U || s_head.image_size > OTA_APP_MAX) {
        return ST_E_PARAM;
    }
    if (s_head.board[0] != '\0' &&
        memcmp(s_head.board, OTA_BOARD, strlen(OTA_BOARD)) != 0) {
        return ST_E_PARAM;
    }

    if (s_open) {
        (void)f_close(&s_file);
        s_open = false;
    }

    (void)f_mkdir(OTA_DIR);

    if (f_open(&s_file, OTA_FILE_FW, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        return ST_E_FS;
    }
    if (f_write(&s_file, &s_head, OTA_HEAD_SIZE, &bw) != FR_OK || bw != OTA_HEAD_SIZE) {
        (void)f_close(&s_file);
        return ST_E_FS;
    }

    s_open    = true;
    s_written = 0U;
    s_crc     = 0xFFFFFFFFU;

    (void)ser_put_u32(out, 0U);
    *out_len = 4U;
    return ST_OK;
}

static uint8_t ota_data(const ProtoFrame_t *req, uint8_t *out, uint16_t *out_len) {
    uint32_t offset;
    uint16_t count;
    UINT     bw = 0U;

    if (!s_open) {
        return ST_E_PARAM;
    }
    if (req->len < 5U) {
        return ST_E_PARAM;
    }

    offset = ser_get_u32(req->data);
    count  = (uint16_t)(req->len - 4U);

    if (offset != (OTA_HEAD_SIZE + s_written)) {
        /* 偏移对不上：把期望偏移回给主机，主机从这里续传 */
        (void)ser_put_u32(out, OTA_HEAD_SIZE + s_written);
        *out_len = 4U;
        return ST_E_PARAM;
    }
    if ((s_written + count) > s_head.image_size) {
        return ST_E_TOOBIG;
    }

    if (f_write(&s_file, &req->data[4], count, &bw) != FR_OK || bw != count) {
        return ST_E_FS;
    }

    s_crc = crc_update(s_crc, &req->data[4], count);
    s_written += count;

    (void)ser_put_u32(out, s_written);
    *out_len = 4U;
    return ST_OK;
}

static uint8_t ota_commit(void) {
    if (!s_open) {
        return ST_E_PARAM;
    }
    if (s_written != s_head.image_size) {
        return ST_E_PARAM;
    }
    if (s_crc != s_head.image_crc) {
        return ST_E_CRC;
    }

    (void)f_close(&s_file);
    s_open = false;

    /* 先把当前运行的镜像存成 PREV.BIN，升级失败才有得回滚 */
    if (!snapshot_current()) {
        (void)f_unlink(OTA_FILE_FW);
        return ST_E_FS;
    }

    if (!write_req(OTA_OP_UPGRADE)) {
        return ST_E_FS;
    }

    s_reboot = true;
    return ST_OK;
}

static uint8_t ota_abort(void) {
    if (s_open) {
        (void)f_close(&s_file);
        s_open = false;
    }
    (void)f_unlink(OTA_FILE_FW);
    s_written = 0U;
    return ST_OK;
}

static uint8_t ota_rollback(void) {
    ota_head_t head;

    if (!head_from_file(OTA_FILE_PREV, &head)) {
        return ST_E_NOTFOUND;
    }
    if (!write_req(OTA_OP_ROLLBACK)) {
        return ST_E_FS;
    }
    s_reboot = true;
    return ST_OK;
}

/* ============================ 分发 ============================ */
uint8_t Ota_Handle(const ProtoFrame_t *req, uint8_t *out, uint16_t *out_len) {
    *out_len = 0U;

    switch (req->cmd) {
        case CMD_OTA_INFO:
            return ota_info(out, out_len);

        case CMD_OTA_BEGIN:
            return ota_begin(req, out, out_len);

        case CMD_OTA_DATA:
            return ota_data(req, out, out_len);

        case CMD_OTA_COMMIT:
            return ota_commit();

        case CMD_OTA_ABORT:
            return ota_abort();

        case CMD_OTA_ROLLBACK:
            return ota_rollback();

        default:
            return ST_E_UNSUPPORTED;
    }
}

bool Ota_RebootPending(void) {
    if (s_reboot) {
        s_reboot = false;
        return true;
    }
    return false;
}
