/**
******************************************************************************
  * @file           : SerialProto.c
  * @brief          : 通信协议帧编解码（CRC16 + 帧重组 + 发送）
  * @date           : 2026/11/08
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  ******************************************************************************
  */

#include "SerialCoreTask.h"

#include <string.h>

#ifdef BOOTLOADER
/* bootloader 单线程：组好的帧直接回调处理，不需要队列 */
#include "boot.h"
extern void Boot_Proto_OnFrame(const ProtoFrame_t *frame);
#else
#include "cmsis_os2.h"
#include "os_handles.h"
#endif

/* ============================ CRC16-CCITT-FALSE ============================ */
static const uint16_t s_crc_table[256] = {
    0x0000U, 0x1021U, 0x2042U, 0x3063U, 0x4084U, 0x50A5U, 0x60C6U, 0x70E7U,
    0x8108U, 0x9129U, 0xA14AU, 0xB16BU, 0xC18CU, 0xD1ADU, 0xE1CEU, 0xF1EFU,
    0x1231U, 0x0210U, 0x3273U, 0x2252U, 0x52B5U, 0x4294U, 0x72F7U, 0x62D6U,
    0x9339U, 0x8318U, 0xB37BU, 0xA35AU, 0xD3BDU, 0xC39CU, 0xF3FFU, 0xE3DEU,
    0x2462U, 0x3443U, 0x0420U, 0x1401U, 0x64E6U, 0x74C7U, 0x44A4U, 0x5485U,
    0xA56AU, 0xB54BU, 0x8528U, 0x9509U, 0xE5EEU, 0xF5CFU, 0xC5ACU, 0xD58DU,
    0x3653U, 0x2672U, 0x1611U, 0x0630U, 0x76D7U, 0x66F6U, 0x5695U, 0x46B4U,
    0xB75BU, 0xA77AU, 0x9719U, 0x8738U, 0xF7DFU, 0xE7FEU, 0xD79DU, 0xC7BCU,
    0x48C4U, 0x58E5U, 0x6886U, 0x78A7U, 0x0840U, 0x1861U, 0x2802U, 0x3823U,
    0xC9CCU, 0xD9EDU, 0xE98EU, 0xF9AFU, 0x8948U, 0x9969U, 0xA90AU, 0xB92BU,
    0x5AF5U, 0x4AD4U, 0x7AB7U, 0x6A96U, 0x1A71U, 0x0A50U, 0x3A33U, 0x2A12U,
    0xDBFDU, 0xCBDCU, 0xFBBFU, 0xEB9EU, 0x9B79U, 0x8B58U, 0xBB3BU, 0xAB1AU,
    0x6CA6U, 0x7C87U, 0x4CE4U, 0x5CC5U, 0x2C22U, 0x3C03U, 0x0C60U, 0x1C41U,
    0xEDAEU, 0xFD8FU, 0xCDECU, 0xDDCDU, 0xAD2AU, 0xBD0BU, 0x8D68U, 0x9D49U,
    0x7E97U, 0x6EB6U, 0x5ED5U, 0x4EF4U, 0x3E13U, 0x2E32U, 0x1E51U, 0x0E70U,
    0xFF9FU, 0xEFBEU, 0xDFDDU, 0xCFFCU, 0xBF1BU, 0xAF3AU, 0x9F59U, 0x8F78U,
    0x9188U, 0x81A9U, 0xB1CAU, 0xA1EBU, 0xD10CU, 0xC12DU, 0xF14EU, 0xE16FU,
    0x1080U, 0x00A1U, 0x30C2U, 0x20E3U, 0x5004U, 0x4025U, 0x7046U, 0x6067U,
    0x83B9U, 0x9398U, 0xA3FBU, 0xB3DAU, 0xC33DU, 0xD31CU, 0xE37FU, 0xF35EU,
    0x02B1U, 0x1290U, 0x22F3U, 0x32D2U, 0x4235U, 0x5214U, 0x6277U, 0x7256U,
    0xB5EAU, 0xA5CBU, 0x95A8U, 0x8589U, 0xF56EU, 0xE54FU, 0xD52CU, 0xC50DU,
    0x34E2U, 0x24C3U, 0x14A0U, 0x0481U, 0x7466U, 0x6447U, 0x5424U, 0x4405U,
    0xA7DBU, 0xB7FAU, 0x8799U, 0x97B8U, 0xE75FU, 0xF77EU, 0xC71DU, 0xD73CU,
    0x26D3U, 0x36F2U, 0x0691U, 0x16B0U, 0x6657U, 0x7676U, 0x4615U, 0x5634U,
    0xD94CU, 0xC96DU, 0xF90EU, 0xE92FU, 0x99C8U, 0x89E9U, 0xB98AU, 0xA9ABU,
    0x5844U, 0x4865U, 0x7806U, 0x6827U, 0x18C0U, 0x08E1U, 0x3882U, 0x28A3U,
    0xCB7DU, 0xDB5CU, 0xEB3FU, 0xFB1EU, 0x8BF9U, 0x9BD8U, 0xABBBU, 0xBB9AU,
    0x4A75U, 0x5A54U, 0x6A37U, 0x7A16U, 0x0AF1U, 0x1AD0U, 0x2AB3U, 0x3A92U,
    0xFD2EU, 0xED0FU, 0xDD6CU, 0xCD4DU, 0xBDAAU, 0xAD8BU, 0x9DE8U, 0x8DC9U,
    0x7C26U, 0x6C07U, 0x5C64U, 0x4C45U, 0x3CA2U, 0x2C83U, 0x1CE0U, 0x0CC1U,
    0xEF1FU, 0xFF3EU, 0xCF5DU, 0xDF7CU, 0xAF9BU, 0xBFBAU, 0x8FD9U, 0x9FF8U,
    0x6E17U, 0x7E36U, 0x4E55U, 0x5E74U, 0x2E93U, 0x3EB2U, 0x0ED1U, 0x1EF0U,
};

static uint16_t proto_crc16(const uint8_t *data, uint32_t len) {
    uint16_t crc = 0xFFFFU;
    while (len-- > 0U) {
        crc = (uint16_t)((uint16_t)(crc << 8) ^ s_crc_table[(uint8_t)((crc >> 8) ^ *data)]);
        data++;
    }
    return crc;
}

/* ============================ 内部状态 ============================ */
typedef struct {
    uint8_t  buf[PROTO_FRAME_MAX];
    uint16_t len;
} ProtoAssembler_t;

static ProtoAssembler_t s_asm[LINK_NUM];      // 每条链路独立重组，互不干扰
static uint8_t           s_tx[PROTO_FRAME_MAX];   // 仅任务上下文使用，无需加锁

/* 丢掉缓冲区第一个字节后重新找帧头（用于帧头误判/校验失败的重同步） */
static void proto_drop_first(ProtoAssembler_t *a) {
    if (a->len > 1U) {
        memmove(a->buf, &a->buf[1], (size_t)(a->len - 1U));
        a->len--;
    } else {
        a->len = 0U;
    }
}

/* ============================ 接收：字节流 -> 完整帧 ============================ */
void SerialProto_Feed(LinkId_t link, const uint8_t *data, uint32_t len) {
    if ((uint32_t)link >= (uint32_t)LINK_NUM || data == NULL || len == 0U) {
        return;
    }

    ProtoAssembler_t *a = &s_asm[link];

    /* 只保留最新的 PROTO_FRAME_MAX 个字节，避免缓冲区溢出 */
    if (len > PROTO_FRAME_MAX) {
        data += (len - PROTO_FRAME_MAX);
        len = PROTO_FRAME_MAX;
    }
    if ((uint32_t)a->len + len > PROTO_FRAME_MAX) {
        a->len = 0U;                    // 半帧积压过多，直接丢弃重新同步
    }
    memcpy(&a->buf[a->len], data, (size_t)len);
    a->len = (uint16_t)(a->len + len);

    for (;;) {
        if (a->len < 2U) {
            return;
        }

        /* 1. 找帧头 A5 5A */
        uint16_t i = 0U;
        while ((uint16_t)(i + 1U) < a->len) {
            if (a->buf[i] == PROTO_SOF0 && a->buf[i + 1U] == PROTO_SOF1) {
                break;
            }
            i++;
        }
        if ((uint16_t)(i + 1U) >= a->len) {
            /* 没有帧头：若最后一个字节可能是帧头首字节则保留，其余丢弃 */
            if (a->buf[a->len - 1U] == PROTO_SOF0) {
                a->buf[0] = PROTO_SOF0;
                a->len = 1U;
            } else {
                a->len = 0U;
            }
            return;
        }
        if (i > 0U) {
            memmove(a->buf, &a->buf[i], (size_t)(a->len - i));
            a->len = (uint16_t)(a->len - i);
        }

        /* 2. 长度字段合法性 */
        if (a->len < PROTO_HDR_SIZE) {
            return;
        }
        uint16_t payload_len = ser_get_u16(&a->buf[2]);
        if (payload_len > PROTO_MAX_PAYLOAD) {
            proto_drop_first(a);
            continue;
        }

        /* 3. 整帧到齐了吗 */
        uint16_t total = (uint16_t)(PROTO_HDR_SIZE + payload_len + PROTO_CRC_SIZE);
        if (a->len < total) {
            return;
        }

        /* 4. 校验 */
        uint16_t calc   = proto_crc16(&a->buf[2], (uint32_t)payload_len + 4U);
        uint16_t recv   = ser_get_u16(&a->buf[total - 2U]);
        if (calc != recv) {
            proto_drop_first(a);
            continue;
        }

#ifdef BOOTLOADER
        static ProtoFrame_t frame;              /* bootloader 栈很小，放静态区 */
#else
        /* 5. 投递到命令队列（中断安全，超时必须为 0） */
        ProtoFrame_t frame;
#endif
        frame.link = (uint8_t)link;
        frame.cmd  = a->buf[4];
        frame.seq  = a->buf[5];
        frame.len  = payload_len;
        if (payload_len > 0U) {
            memcpy(frame.data, &a->buf[PROTO_HDR_SIZE], (size_t)payload_len);
        }
#ifdef BOOTLOADER
        Boot_Proto_OnFrame(&frame);
#else
        (void)osMessageQueuePut(SerialReqQueueHandle, &frame, 0U, 0U);
#endif

        /* 6. 移除已处理的帧 */
        a->len = (uint16_t)(a->len - total);
        if (a->len > 0U) {
            memmove(a->buf, &a->buf[total], (size_t)a->len);
        }
    }
}

/* ============================ 发送：组帧 -> 链路 ============================ */
void SerialProto_Send(LinkId_t link, uint8_t cmd, uint8_t seq,
                      const uint8_t *data, uint16_t len) {
    if ((uint32_t)link >= (uint32_t)LINK_NUM || len > PROTO_MAX_PAYLOAD) {
        return;
    }
    if (len > 0U && data == NULL) {
        return;
    }

    s_tx[0] = PROTO_SOF0;
    s_tx[1] = PROTO_SOF1;
    (void)ser_put_u16(&s_tx[2], len);
    s_tx[4] = cmd;
    s_tx[5] = seq;
    if (len > 0U) {
        memcpy(&s_tx[PROTO_HDR_SIZE], data, (size_t)len);
    }

    uint16_t crc = proto_crc16(&s_tx[2], (uint32_t)len + 4U);
    (void)ser_put_u16(&s_tx[PROTO_HDR_SIZE + len], crc);

#ifdef BOOTLOADER
    if (link == LINK_USB)
    {
        (void)Boot_Usb_Send(s_tx, (uint16_t)(len + PROTO_HDR_SIZE + PROTO_CRC_SIZE));
    }
#else
    (void)SerialLink_Send(link, s_tx, (uint16_t)(len + PROTO_HDR_SIZE + PROTO_CRC_SIZE));
#endif
}
