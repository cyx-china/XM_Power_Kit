/**
 * @file    format.c
 * @brief  统一的定点数值格式化工具实现（不依赖 printf 浮点支持）
 */
#include "format.h"

#include <math.h>
#include <string.h>

/* 10 的幂表（0..6） */
static const uint32_t s_pow10[7] = {1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u};

void append_padded_uint(char **p, uint32_t val, uint8_t width)
{
    char tmp[10];
    uint8_t n = 0;

    do {
        tmp[n++] = (char) ('0' + (val % 10u));
        val /= 10u;
    } while (val != 0u && n < (uint8_t) sizeof(tmp));

    while (n < width) {
        tmp[n++] = '0';
    }
    while (n-- != 0u) {
        *(*p)++ = tmp[n];
    }
}

char *fmt_u32_padded(char *buf, size_t buf_size, uint32_t value, uint8_t width)
{
    char tmp[12];
    char *p = tmp;

    if ((buf == NULL) || (buf_size == 0u)) {
        return buf;
    }
    if (width > 10u) {
        width = 10u;
    }
    append_padded_uint(&p, value, width);
    *p = '\0';

    size_t len = (size_t) (p - tmp);
    if (len >= buf_size) {
        len = buf_size - 1u;
    }
    memcpy(buf, tmp, len);
    buf[len] = '\0';
    return buf;
}

/* 把 tmp 里的字符串安全拷进 buf 并结束 */
static char *copy_out(char *buf, size_t buf_size, const char *tmp)
{
    size_t len = strlen(tmp);

    if (len >= buf_size) {
        len = buf_size - 1u;
    }
    memcpy(buf, tmp, len);
    buf[len] = '\0';
    return buf;
}

/* 内部：把定点数字写进 out（不含结束符），返回写到的位置 */
static char *fixed_to_chars(char *out, float value, uint8_t decimals,
                            uint8_t min_width, uint8_t flags, uint32_t *frac_out)
{
    if (decimals > 6u) {
        decimals = 6u;
    }

    int neg = (value < 0.0f);
    float av = neg ? -value : value;
    uint32_t scale = s_pow10[decimals];
    uint32_t scaled = (uint32_t) (av * (float) scale + 0.5f);
    uint32_t int_part = scaled / scale;
    uint32_t frac_part = scaled % scale;

    char *p = out;
    if (neg) {
        *p++ = '-';
    } else if ((flags & FMT_SIGN) != 0u) {
        *p++ = '+';
    }

    /* 整数部分反转临时区 */
    char digits[10];
    uint8_t n = 0;
    uint32_t t = int_part;
    do {
        digits[n++] = (char) ('0' + (t % 10u));
        t /= 10u;
    } while ((t != 0u) && (n < (uint8_t) sizeof(digits)));

    /* 符号位也计入总宽度（与 printf 的 %0N.Mf 语义一致） */
    uint8_t sign_len = (uint8_t) ((neg || ((flags & FMT_SIGN) != 0u)) ? 1u : 0u);
    uint8_t total = (uint8_t) (sign_len + n + ((decimals > 0u) ? (uint8_t) (decimals + 1u) : 0u));
    if (((flags & FMT_ZERO_PAD) != 0u) && (min_width > total)) {
        uint8_t pad = (uint8_t) (min_width - total);
        while (pad-- != 0u) {
            *p++ = '0';
        }
    }

    while (n-- != 0u) {
        *p++ = digits[n];
    }

    if (decimals > 0u) {
        *p++ = '.';
        append_padded_uint(&p, frac_part, decimals);
    }

    if (frac_out != NULL) {
        *frac_out = frac_part;
    }
    return p;
}

char *fmt_fixed(char *buf, size_t buf_size, float value,
                uint8_t decimals, uint8_t min_width, uint8_t flags)
{
    char tmp[24];

    if ((buf == NULL) || (buf_size == 0u)) {
        return buf;
    }
    char *p = fixed_to_chars(tmp, value, decimals, min_width, flags, NULL);
    *p = '\0';
    return copy_out(buf, buf_size, tmp);
}

char *fmt_fixed_unit(char *buf, size_t buf_size, float value,
                     uint8_t decimals, uint8_t min_width, uint8_t flags,
                     const char *unit)
{
    char tmp[40];
    size_t len;

    if ((buf == NULL) || (buf_size == 0u)) {
        return buf;
    }
    char *p = fixed_to_chars(tmp, value, decimals, min_width, flags, NULL);
    if (unit != NULL) {
        while ((*unit != '\0') && ((size_t) (p - tmp) < (sizeof(tmp) - 1u))) {
            *p++ = *unit++;
        }
    }
    *p = '\0';
    len = (size_t) (p - tmp);
    if (len >= buf_size) {
        len = buf_size - 1u;
    }
    memcpy(buf, tmp, len);
    buf[len] = '\0';
    return buf;
}

char *fmt_fixed_ex(char *buf, size_t buf_size, float value,
                   uint8_t decimals, uint8_t min_width, uint8_t flags,
                   char *extra, uint8_t nextra)
{
    char tmp[24];
    uint32_t frac_part = 0u;

    if ((buf == NULL) || (buf_size == 0u)) {
        return buf;
    }
    if (decimals > 5u) {
        decimals = 5u;
    }

    char *p = fixed_to_chars(tmp, value, decimals, min_width, flags, &frac_part);
    *p = '\0';
    copy_out(buf, buf_size, tmp);

    if ((extra != NULL) && (nextra > 0u)) {
        if (nextra > 3u) {
            nextra = 3u;
        }
        (void) frac_part;
        /* 用原始值精确重算末位（float 尾数足够表示 0~1e5 的整数部分） */
        double av = fabs((double) value);
        double mul = (double) s_pow10[decimals];
        for (uint8_t i = 0; i < nextra; i++) {
            mul *= 10.0;
            int d = (int) (av * mul + 1e-9 * mul) % 10;
            if (d < 0) {
                d = -d;
            }
            extra[i] = (char) ('0' + d);
        }
        extra[nextra] = '\0';
    }
    return buf;
}

int fmt_fixed_digits(float value, uint8_t target_digits, char *buf)
{
    /* 边界值保护 */
    if (target_digits < 2u) {
        target_digits = 2u;
    }
    if (target_digits > 5u) {
        target_digits = 5u;
    }
    if (value < 0.0f) {
        value = -value;
    }

    /* 数值上限保护 */
    static const float maxv[6] = {0.0f, 0.0f, 99.9f, 999.0f, 9999.0f, 99999.0f};
    if (value > maxv[target_digits]) {
        value = maxv[target_digits];
    }

    char *p = buf;
    uint8_t dec_digits;

    /* 根据数值大小确定小数位数 */
    if (value >= 100.0f) {
        dec_digits = (target_digits >= 4u) ? (uint8_t) (target_digits - 3u) : 0u;
    } else if (value >= 10.0f) {
        dec_digits = (uint8_t) (target_digits - 2u);
    } else {
        dec_digits = (uint8_t) (target_digits - 1u);
    }
    if (dec_digits < 1u) {
        dec_digits = 1u;
    }
    if (dec_digits > 3u) {
        dec_digits = 3u;
    }

    uint32_t scale = s_pow10[dec_digits];
    uint32_t ival = (uint32_t) (value * (float) scale + 0.5f);
    uint32_t int_part = ival / scale;
    uint32_t frac_part = ival % scale;

    /* 进位处理：数值超过阈值时减少小数位 */
    uint32_t carry_threshold = (value >= 100.0f) ? 1000u : ((value >= 10.0f) ? 100u : 10u);
    if ((int_part >= carry_threshold) && (dec_digits > 1u)) {
        dec_digits--;
        frac_part /= 10u;
    }

    /* 写入整数部分 */
    if (int_part == 0u) {
        *p++ = '0';
    } else {
        char digits[8];
        uint8_t n = 0;
        uint32_t t = int_part;
        do {
            digits[n++] = (char) ('0' + (t % 10u));
            t /= 10u;
        } while (t != 0u);
        while (n-- != 0u) {
            *p++ = digits[n];
        }
    }

    *p++ = '.';
    append_padded_uint(&p, frac_part, dec_digits);
    *p = '\0';

    return (int) (p - buf);
}

int fmt_fixed_00_00(float value, float max_value, char *buf)
{
    if (value < 0.01f) {
        value = 0.01f;
    }
    if (value > max_value) {
        value = max_value;
    }

    uint32_t ival = (uint32_t) (value * 100.0f + 0.5f);
    char *p = buf;

    append_padded_uint(&p, ival / 100u, 2u);
    *p++ = '.';
    append_padded_uint(&p, ival % 100u, 2u);
    *p = '\0';

    return 5;
}

int fmt_energy_wh(double value, char *buf)
{
    if (value < 0.0) {
        value = -value;
    }
    if (value > 9999.999) {
        value = 9999.999;
    }

    /* 用 double 运算：9999 量级上 float 的最小步进已达 1mWh */
    uint32_t ival = (uint32_t) (value * 1000.0 + 0.5);
    char *p = buf;

    append_padded_uint(&p, ival / 1000u, 4u);
    *p++ = '.';
    append_padded_uint(&p, ival % 1000u, 3u);
    *p++ = 'W';
    *p++ = 'h';
    *p = '\0';

    return 9;
}

int fmt_hms(uint32_t seconds, char *buf)
{
    if (buf == NULL) {
        return -1;
    }

    uint32_t h = seconds / 3600u;
    uint32_t m = (seconds % 3600u) / 60u;
    uint32_t s = seconds % 60u;

    if (h > 999u) {
        h = 999u;
        m = 59u;
        s = 59u;
    }

    char *p = buf;
    append_padded_uint(&p, h, 3u);
    *p++ = ':';
    append_padded_uint(&p, m, 2u);
    *p++ = ':';
    append_padded_uint(&p, s, 2u);
    *p = '\0';

    return 8;
}

int fmt_percent_u8(uint8_t value, char *buf)
{
    if (buf == NULL) {
        return -1;
    }

    char *p = buf;
    append_padded_uint(&p, value, 3u);
    *p++ = '%';
    *p = '\0';

    return 4;
}
