/**
******************************************************************************
  * @file           : SerialCoreTask.h
  * @brief          : 上位机通信协议（v2）与任务接口
  * @date           : 2026/11/08
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * 帧格式（小端）：
  *   A5 5A | LEN_L LEN_H | CMD | SEQ | DATA[LEN] | CRC_L CRC_H
  *
  *   LEN   : 负载长度，0 ~ PROTO_MAX_PAYLOAD
  *   CRC16 : CCITT-FALSE（poly 0x1021，初值 0xFFFF），覆盖 LEN_H ... DATA 末尾
  *   应答  : CMD 与 SEQ 原样返回；DATA[0] = 状态码（ProtoStatus_t），DATA[1..] 为数据
  *
  * 链路  : USB CDC 与 USART3 蓝牙，两条链路共用同一套帧与命令，
  *         应答总是从请求进来的那条链路发出。
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  ******************************************************************************
  */
#ifndef XM_POWER_KIT_SERIAL_H
#define XM_POWER_KIT_SERIAL_H

#include <stdint.h>
#include <stdbool.h>

/* ============================== 链路 ============================== */
typedef enum {
    LINK_USB = 0,       // USB CDC 虚拟串口
    LINK_BT  = 1,       // USART3 蓝牙串口
    LINK_NUM = 2
} LinkId_t;

/* ============================== 帧 ============================== */
#define PROTO_SOF0              0xA5U
#define PROTO_SOF1              0x5AU
#define PROTO_VER               2U

#define PROTO_HDR_SIZE          6U      // A5 5A LEN(2) CMD SEQ
#define PROTO_CRC_SIZE          2U
#define PROTO_MAX_PAYLOAD       512U
#define PROTO_FRAME_MAX         (PROTO_HDR_SIZE + PROTO_MAX_PAYLOAD + PROTO_CRC_SIZE)

/* 单次文件读写的数据量上限（主机必须遵守） */
#define FS_CHUNK_MAX            432U

/* ============================== 命令 ============================== */
typedef enum {
    CMD_SYS_INFO        = 0x01,     // 设备信息
    CMD_SYS_REBOOT      = 0x02,     // 重启设备
    CMD_SYS_BT_BAUD     = 0x03,     // 设置蓝牙波特率 [u32]

    CMD_PARAM_GET       = 0x10,     // 读参数 [kind: 0=当前值 1=默认值]
    CMD_PARAM_SET       = 0x11,     // 写参数 [PARAM_TOTAL_BYTES]

    CMD_FS_LIST         = 0x20,     // 列目录 [len][path][start u16][max u8]
    CMD_FS_READ         = 0x21,     // 读文件 [len][path][offset u32][len u16]
    CMD_FS_WRITE        = 0x22,     // 写文件 [len][path][offset u32][len u16][data]
    CMD_FS_DELETE       = 0x23,     // 删除文件/空目录 [len][path]
    CMD_FS_RENAME       = 0x24,     // 重命名 [len][old][len][new]
    CMD_FS_INFO         = 0x25,     // 容量信息
    CMD_FS_MKDIR        = 0x26,     // 新建目录 [len][path]
    CMD_FS_STAT         = 0x27,     // 文件属性 [len][path]
    CMD_FS_SYNC         = 0x28,     // 刷新图形层文件/字模缓存

    CMD_OTA_INFO        = 0x30,     // 升级状态（包信息、快照、失败计数）
    CMD_OTA_BEGIN       = 0x31,     // 开始上传 [80 字节包头]
    CMD_OTA_DATA        = 0x32,     // 上传一块 [offset u32][data]
    CMD_OTA_COMMIT      = 0x33,     // 校验 + 生成快照 + 写升级请求
    CMD_OTA_ABORT       = 0x34,     // 放弃本次上传
    CMD_OTA_ROLLBACK    = 0x35      // 回滚到 GOOD.BIN
} ProtoCmd_t;

/* ============================== 状态码 ============================== */
typedef enum {
    ST_OK           = 0,
    ST_E_PARAM      = 1,    // 参数错误
    ST_E_FS         = 2,    // 文件系统错误（DATA[1] 为 FRESULT）
    ST_E_NOTFOUND   = 3,    // 文件不存在
    ST_E_TOOBIG     = 4,    // 数据过长
    ST_E_BUSY       = 5,    // 忙
    ST_E_UNSUPPORTED= 6,    // 不支持
    ST_E_CRC        = 7     // 校验失败
} ProtoStatus_t;

/* ============================== 请求帧 ============================== */
typedef struct {
    uint8_t  link;                      // 来源链路 LinkId_t
    uint8_t  cmd;                       // 命令
    uint8_t  seq;                       // 序号（原样返回）
    uint16_t len;                       // 负载长度
    uint8_t  data[PROTO_MAX_PAYLOAD];   // 负载
} ProtoFrame_t;

/* ============================== 小端辅助 ============================== */
static inline uint16_t ser_get_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t ser_get_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint16_t ser_put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)(v >> 8);
    return 2;
}

static inline uint16_t ser_put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
    return 4;
}

/* ============================== 协议层 SerialProto.c ============================== */
void SerialProto_Feed(LinkId_t link, const uint8_t *data, uint32_t len);   // 可在中断中调用
void SerialProto_Send(LinkId_t link, uint8_t cmd, uint8_t seq,
                      const uint8_t *data, uint16_t len);                  // 仅任务上下文

/* ============================== 链路层 SerialLink.c ============================== */
void     SerialLink_Init(void);
uint16_t SerialLink_Send(LinkId_t link, const uint8_t *data, uint16_t len);
void     SerialLink_UsbRx(const uint8_t *data, uint32_t len);              // USB 中断中调用
uint32_t SerialLink_GetBtBaud(void);
void     SerialLink_SetBtBaud(uint32_t baud);

/* ============================== 文件服务 FileService.c ============================== */
uint8_t  FileService_Handle(const ProtoFrame_t *req, uint8_t *out, uint16_t *out_len);
bool     FileService_GetCapacity(uint32_t *total_kb, uint32_t *free_kb);
void     FileService_InvalidateGfx(void);

/* ============================== 任务 SerialCoreTask.c ============================== */
void Serial_Init(void);

#endif //XM_POWER_KIT_SERIAL_H
