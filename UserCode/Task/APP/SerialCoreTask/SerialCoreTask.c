/**
******************************************************************************
  * @file           : SerialCoreTask.c
  * @brief          : 上位机通信任务（命令分发）
  * @date           : 2026/11/08
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * 链路无关：USB CDC 与 USART3 蓝牙共用同一套帧格式，应答从请求进来的链路返回。
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  ******************************************************************************
  */

#include "SerialCoreTask.h"

#include <string.h>

#include "cmsis_os2.h"
#include "ff.h"
#include "os_handles.h"
#include "OtaService.h"
#include "stm32f4xx_hal.h"
#include "UserDefineManage.h"
#include "UserTask.h"

#define BT_CFG_PATH     "0:/btcfg.bin"
#define BT_CFG_DEFAULT  115200U

/* s_rsp[0] = 状态码，数据从 s_rsp[1] 开始，正好一帧的负载长度 */
static uint8_t s_rsp[PROTO_MAX_PAYLOAD];

/* ============================ 应答辅助 ============================ */
static void Serial_ReplyStatus(const ProtoFrame_t *req, uint8_t status, uint16_t data_len) {
    s_rsp[0] = status;
    SerialProto_Send((LinkId_t)req->link, req->cmd, req->seq, s_rsp, (uint16_t)(data_len + 1U));
}

/* ============================ 蓝牙波特率配置 ============================ */
static void bt_cfg_load(void) {
    FIL   fil;
    UINT  br = 0U;
    uint8_t buf[4];

    if (f_open(&fil, BT_CFG_PATH, FA_READ) != FR_OK) {
        return;
    }
    FRESULT fr = f_read(&fil, buf, 4U, &br);
    (void)f_close(&fil);
    if (fr != FR_OK || br != 4U) {
        return;
    }
    uint32_t baud = ser_get_u32(buf);
    if (baud >= 1200U && baud <= 921600U) {
        SerialLink_SetBtBaud(baud);
    }
}

static void bt_cfg_save(uint32_t baud) {
    FIL   fil;
    UINT  bw = 0U;
    uint8_t buf[4];

    if (f_open(&fil, BT_CFG_PATH, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        return;
    }
    (void)ser_put_u32(buf, baud);
    (void)f_write(&fil, buf, 4U, &bw);
    (void)f_close(&fil);
}

/* ============================ 命令：设备信息 ============================ */
static void Serial_HandleSysInfo(const ProtoFrame_t *req) {
    uint8_t *p = &s_rsp[1];
    uint16_t i = 0U;

    const char *ver = SYS_VERSION_CODE;
    uint16_t    vl  = (uint16_t)strlen(ver);
    if (vl > 40U) {
        vl = 40U;
    }

    p[i++] = (uint8_t)PROTO_VER;
    p[i++] = (uint8_t)vl;
    memcpy(&p[i], ver, vl);
    i = (uint16_t)(i + vl);

    p[i++] = (uint8_t)Param_Number;
    i = (uint16_t)(i + ser_put_u16(&p[i], (uint16_t)PARAM_TOTAL_BYTES));
    i = (uint16_t)(i + ser_put_u16(&p[i], 0x000FU));            // bit0 USB / bit1 BT / bit2 FS / bit3 OTA
    i = (uint16_t)(i + ser_put_u32(&p[i], SerialLink_GetBtBaud()));

    uint32_t total_kb = 0U;
    uint32_t free_kb  = 0U;
    (void)FileService_GetCapacity(&total_kb, &free_kb);
    i = (uint16_t)(i + ser_put_u32(&p[i], total_kb));
    i = (uint16_t)(i + ser_put_u32(&p[i], free_kb));

    Serial_ReplyStatus(req, ST_OK, i);
}

/* ============================ 命令：参数读写 ============================ */
static void Serial_HandleParam(const ProtoFrame_t *req) {
    uint8_t *p = &s_rsp[1];

    if (req->cmd == CMD_PARAM_GET) {
        if (req->len < 1U) {
            Serial_ReplyStatus(req, ST_E_PARAM, 0U);
            return;
        }
        HAL_StatusTypeDef ret = (req->data[0] == 1U)
                                ? Process_Default_Data(p)
                                : Process_Current_Data(p);
        if (ret != HAL_OK) {
            Serial_ReplyStatus(req, ST_E_FS, 0U);
            return;
        }
        Serial_ReplyStatus(req, ST_OK, (uint16_t)PARAM_TOTAL_BYTES);
        return;
    }

    /* CMD_PARAM_SET */
    if (req->len != (uint16_t)PARAM_TOTAL_BYTES) {
        Serial_ReplyStatus(req, ST_E_PARAM, 0U);
        return;
    }
    if (Write_Current_Data((uint8_t *)req->data, PARAM_TOTAL_BYTES) != HAL_OK) {
        Serial_ReplyStatus(req, ST_E_FS, 0U);
        return;
    }
    (void)UserParam_LoadAllValues();
    Serial_ReplyStatus(req, ST_OK, 0U);
}

/* ============================ 命令分发 ============================ */
static void Serial_Handle(const ProtoFrame_t *req) {
    switch (req->cmd) {
        case CMD_SYS_INFO:
            Serial_HandleSysInfo(req);
            break;

        case CMD_SYS_REBOOT:
            Serial_ReplyStatus(req, ST_OK, 0U);
            osDelay(100U);
            NVIC_SystemReset();
            break;

        case CMD_SYS_BT_BAUD: {
            if (req->len < 4U) {
                Serial_ReplyStatus(req, ST_E_PARAM, 0U);
                break;
            }
            uint32_t baud = ser_get_u32(req->data);
            if (baud < 1200U || baud > 921600U) {
                Serial_ReplyStatus(req, ST_E_PARAM, 0U);
                break;
            }
            Serial_ReplyStatus(req, ST_OK, 0U);       // 先应答，再切换波特率
            osDelay(50U);
            bt_cfg_save(baud);
            SerialLink_SetBtBaud(baud);
            break;
        }

        case CMD_PARAM_GET:
        case CMD_PARAM_SET:
            Serial_HandleParam(req);
            break;

        case CMD_FS_LIST:
        case CMD_FS_READ:
        case CMD_FS_WRITE:
        case CMD_FS_DELETE:
        case CMD_FS_RENAME:
        case CMD_FS_MKDIR:
        case CMD_FS_STAT:
        case CMD_FS_INFO:
        case CMD_FS_SYNC: {
            uint16_t dlen = 0U;
            uint8_t  st = FileService_Handle(req, &s_rsp[1], &dlen);
            Serial_ReplyStatus(req, st, dlen);
            break;
        }

        case CMD_OTA_INFO:
        case CMD_OTA_BEGIN:
        case CMD_OTA_DATA:
        case CMD_OTA_COMMIT:
        case CMD_OTA_ABORT:
        case CMD_OTA_ROLLBACK: {
            uint16_t dlen = 0U;
            uint8_t  st = Ota_Handle(req, &s_rsp[1], &dlen);
            Serial_ReplyStatus(req, st, dlen);
            break;
        }

        default:
            Serial_ReplyStatus(req, ST_E_UNSUPPORTED, 0U);
            break;
    }
}

/* 应答发完再复位，保证主机能收到 COMMIT / ROLLBACK 的 OK */
static void Serial_RebootIfPending(void) {
    if (!Ota_RebootPending()) {
        return;
    }
    osDelay(500U);
    NVIC_SystemReset();
}

/* ============================ 初始化 / 任务 ============================ */
void Serial_Init(void) {
    bt_cfg_load();              // 需在 f_mount 之后调用
    SerialLink_Init();
}

void Start_SerialCoreTask(void const * argument) {
    (void)argument;
    static ProtoFrame_t s_req;      // 519 字节，放静态区，省任务栈

    for (;;) {
        if (osMessageQueueGet(SerialReqQueueHandle, &s_req, NULL, osWaitForever) != osOK) {
            continue;
        }
        Serial_Handle(&s_req);
        Serial_RebootIfPending();
    }
}
