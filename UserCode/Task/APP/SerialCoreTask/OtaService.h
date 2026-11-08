/**
******************************************************************************
  * @file           : OtaService.h
  * @brief          : 应用侧 OTA 服务：接收新固件包、生成回滚快照、写升级请求
  * @date           : 2026/11/08
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * 与 bootloader 的约定：
  *   /ota/FW.BIN      新固件包（80 字节头 + 镜像），升级完由应用改名为 GOOD.BIN
  *   /ota/GOOD.BIN    当前可运行版本的快照，回滚源
  *   /ota/BOOTOK.BIN  应用启动成功后写入，bootloader 用它确认新版本能起来
  *   /ota/REQ.BIN     升级/回滚请求，应用写、bootloader 读并消费
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  ******************************************************************************
  */
#ifndef XM_POWER_KIT_OTA_SERVICE_H
#define XM_POWER_KIT_OTA_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

#include "SerialCoreTask.h"

#define OTA_DIR             "0:/ota"
#define OTA_FILE_FW         "0:/ota/FW.BIN"
#define OTA_FILE_GOOD       "0:/ota/GOOD.BIN"
#define OTA_FILE_BOOTOK     "0:/ota/BOOTOK.BIN"
#define OTA_FILE_REQ        "0:/ota/REQ.BIN"

#define OTA_HEAD_SIZE       80U
#define OTA_MAGIC           "XMK7OTA1"
#define OTA_BOARD           "XM_Power_Kit_V7"
#define OTA_APP_ADDR        0x08008000U
#define OTA_APP_MAX         (480U * 1024U)

#define OTA_OP_UPGRADE      1U
#define OTA_OP_ROLLBACK     2U

/* 升级包 80 字节头（与 bootloader/上位机一致，小端） */
typedef struct {
    char     magic[8];       /* "XMK7OTA1" */
    char     version[32];    /* 固件版本串，如 "STM32F407VE.261108.Rel.0007" */
    uint32_t image_size;
    uint32_t image_crc;
    uint32_t head_crc;       /* 覆盖前 48 字节 */
    uint32_t load_addr;
    char     build_time[8];
    char     board[16];
} ota_head_t;

/* 启动成功后调用（放在 Sys_Init 里，f_mount 之后）：
   写 BOOTOK.BIN，并把 FW.BIN 提升为 GOOD.BIN、清掉 REQ.BIN */
void    Ota_BootConfirm(void);

/* 处理 OTA 命令：返回状态码，数据写到 out（去掉状态码字节） */
uint8_t Ota_Handle(const ProtoFrame_t *req, uint8_t *out, uint16_t *out_len);

/* 命令处理完需要复位时返回 true（由任务在应答发完后执行） */
bool    Ota_RebootPending(void);

/* 设备信息里展示用的 OTA 能力位 */
#define OTA_FEATURE_BITS    0x0001U

#endif //XM_POWER_KIT_OTA_SERVICE_H
