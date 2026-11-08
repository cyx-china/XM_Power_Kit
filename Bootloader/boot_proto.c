/**
 ******************************************************************************
 * @file    boot_proto.c
 * @brief   维护模式协议服务（只走 USB CDC）：
 *            SYS_INFO   设备信息（带"维护模式"特性位）
 *            FS_*       文件管理（与主程序同一套命令，上位机代码不用改）
 *            OTA_APPLY  把已经传上来的 0:/ota/FW.BIN 立刻写进内部 Flash
 *            SYS_REBOOT 重启
 *          组帧/校验复用主程序的 SerialProto.c（BOOTLOADER 宏切换投递方式）。
 ******************************************************************************
 */
#include "boot.h"
#include "SerialCoreTask.h"
#include "ff.h"

#include <string.h>

#define BOOT_FW_STRING      "XM Power Kit Bootloader"
#define BOOT_FEATURE_BITS   (0x0001U | 0x0004U | 0x0008U | 0x0010U)   /* USB/FS/OTA/维护模式 */

static uint8_t s_rsp[PROTO_MAX_PAYLOAD];

/* ---------------------------- 发送 ---------------------------- */
static void reply(uint8_t cmd, uint8_t seq, uint8_t status, const uint8_t *data, uint16_t len)
{
    static uint8_t out[PROTO_MAX_PAYLOAD + 1U];

    out[0] = status;
    if ((len > 0U) && (data != NULL))
    {
        for (uint16_t i = 0U; i < len; i++)
        {
            out[1U + i] = data[i];
        }
    }

    (void)SerialProto_Send(LINK_USB, cmd, seq, out, (uint16_t)(len + 1U));
}

/* ---------------------------- SYS_INFO ---------------------------- */
static uint8_t cmd_info(uint8_t *out, uint16_t *out_len)
{
    uint32_t total_kb = 0U;
    uint32_t free_kb  = 0U;
    uint16_t i        = 0U;
    const char *fw    = BOOT_FW_STRING;
    uint16_t fw_len   = (uint16_t)strlen(fw);

    out[i++] = PROTO_VER;
    out[i++] = (uint8_t)fw_len;
    for (uint16_t k = 0U; k < fw_len; k++)
    {
        out[i++] = (uint8_t)fw[k];
    }

    out[i++] = 0U;                                   /* 参数个数：维护模式没有参数 */
    (void)ser_put_u16(&out[i], 0U);                  /* 参数总字节数 */
    i = (uint16_t)(i + 2U);
    (void)ser_put_u16(&out[i], BOOT_FEATURE_BITS);
    i = (uint16_t)(i + 2U);
    (void)ser_put_u32(&out[i], 0U);                  /* 蓝牙波特率（维护模式不涉及） */
    i = (uint16_t)(i + 4U);

    (void)FileService_GetCapacity(&total_kb, &free_kb);
    (void)ser_put_u32(&out[i], total_kb);
    i = (uint16_t)(i + 4U);
    (void)ser_put_u32(&out[i], free_kb);
    i = (uint16_t)(i + 4U);

    *out_len = i;
    return ST_OK;
}

/* ---------------------------- OTA_APPLY ---------------------------- */
/**
 * @brief  校验已经上传的 0:/ota/FW.BIN，并写下升级请求后重启。
 *         真正的擦写发生在下一次启动的 bootloader 里（Boot_Ota_Boot），
 *         这样掉电/中断都能幂等重来，也复用同一套搬运代码。
 */
static uint8_t cmd_apply(void)
{
    boot_pkg_head_t head;
    boot_record_t   rec;
    boot_req_t      req;
    FIL             file;
    UINT            bw = 0U;
    FRESULT         fr;

    if (!Boot_Pkg_Load(BOOT_FILE_FW, &head))
    {
        return ST_E_CRC;
    }

    Boot_State_Load(&rec);

    memset(&req, 0, sizeof(req));
    req.token = rec.token + 1U;
    req.op    = BOOT_OP_UPGRADE;
    req.crc   = (uint16_t)Boot_Crc32(&req, 6U);

    fr = f_open(&file, BOOT_FILE_REQ, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
    {
        return ST_E_FS;
    }
    fr = f_write(&file, &req, sizeof(req), &bw);
    (void)f_close(&file);

    if ((fr != FR_OK) || (bw != sizeof(req)))
    {
        return ST_E_FS;
    }

    return ST_OK;
}

/* ---------------------------- 分发 ---------------------------- */
static void handle(const ProtoFrame_t *req)
{
    uint8_t  status = ST_E_UNSUPPORTED;
    uint16_t len    = 0U;
    bool     reboot = false;

    switch (req->cmd)
    {
    case CMD_SYS_INFO:
        status = cmd_info(s_rsp, &len);
        break;

    case CMD_FS_LIST:
    case CMD_FS_READ:
    case CMD_FS_WRITE:
    case CMD_FS_DELETE:
    case CMD_FS_RENAME:
    case CMD_FS_MKDIR:
    case CMD_FS_STAT:
    case CMD_FS_INFO:
    case CMD_FS_SYNC:
        status = FileService_Handle(req, s_rsp, &len);
        break;

    case CMD_OTA_APPLY:
        status = cmd_apply();
        reboot = (status == ST_OK);
        break;

    case CMD_SYS_REBOOT:
        status = ST_OK;
        reboot = true;
        break;

    default:
        break;
    }

    reply(req->cmd, req->seq, status, s_rsp, len);

    if (reboot)
    {
        HAL_Delay(300U);            /* 留时间把应答发出去 */
        __disable_irq();
        NVIC_SystemReset();
    }
}

/* SerialProto.c 在 BOOTLOADER 下把组好的帧交给这里（主循环上下文，可以放心做文件操作） */
void Boot_Proto_OnFrame(const ProtoFrame_t *frame)
{
    handle(frame);
}

void Boot_Proto_Poll(void)
{
    uint8_t  chunk[64];
    uint16_t n;

    while ((n = Boot_Usb_Read(chunk, sizeof(chunk))) > 0U)
    {
        SerialProto_Feed(LINK_USB, chunk, n);
    }
}
