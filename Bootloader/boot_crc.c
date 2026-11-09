/**
 ******************************************************************************
 * @file    boot_crc.c
 * @brief   与上位机 / 应用侧完全一致的 CRC32/ISO-HDLC（多项式 0xEDB88320；
 *          内部寄存器初值 0xFFFFFFFF、结果取反 —— 与 zlib.crc32 / 常见工具的 CRC32 相同）
 ******************************************************************************
 */
#include "boot.h"

static uint32_t s_table[256];
static bool     s_ready = false;

void Boot_Crc32_Init(void)
{
    for (uint32_t i = 0U; i < 256U; i++)
    {
        uint32_t c = i;
        for (uint32_t k = 0U; k < 8U; k++)
        {
            c = (c & 1U) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
        }
        s_table[i] = c;
    }
    s_ready = true;
}

uint32_t Boot_Crc32_Update(uint32_t crc, const void *buf, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)buf;

    if (!s_ready)
    {
        Boot_Crc32_Init();
    }

    crc = ~crc;
    while (len-- > 0U)
    {
        crc = s_table[(crc ^ *p++) & 0xFFU] ^ (crc >> 8);
    }
    return ~crc;
}

uint32_t Boot_Crc32(const void *buf, uint32_t len)
{
    /* 首次调用必须传 0（Update 内部先取反得到寄存器初值 0xFFFFFFFF） */
    return Boot_Crc32_Update(0U, buf, len);
}

uint32_t Boot_Crc32_Legacy(const void *buf, uint32_t len)
{
    /* 早期固件把首次初值传成了 0xFFFFFFFF（内部寄存器实际从 0 开始），
       结果与标准值不同。这里保留同样的算法，用于兼容那时写下的升级包与记录。 */
    return Boot_Crc32_Update(0xFFFFFFFFU, buf, len);
}
