/**
 ******************************************************************************
 * @file    boot_crc.c
 * @brief   与上位机 / 应用侧完全一致的 CRC32（多项式 0xEDB88320，初值 0xFFFFFFFF）
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
    return Boot_Crc32_Update(0xFFFFFFFFU, buf, len);
}
