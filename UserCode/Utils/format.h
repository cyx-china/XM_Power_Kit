/**
 * @file    format.h
 * @brief  统一的定点数值格式化工具（全工程禁止使用 printf 的 %f）
 *
 * 背景：newlib-nano 的 `%f` 支持会把 _printf_float/_dtoa_r 整套浮点打印代码拉进镜像
 *       （约 4KB FLASH + 一整套 double 运算），而本工程所有浮点显示都只需要
 *       "几位整数 + 几位小数" 的定点输出，因此统一用整数运算实现。
 *
 * 用法示例：
 *   char buf[12];
 *   fmt_fixed(buf, sizeof buf, 12.3456f, 2, 0, 0);              // "12.35"
 *   fmt_fixed(buf, sizeof buf, 1.5f, 2, 5, FMT_ZERO_PAD);       // "01.50"
 *   fmt_fixed(buf, sizeof buf, -1.234f, 3, 6, FMT_SIGN|FMT_ZERO_PAD); // "-1.234"
 *   fmt_fixed_unit(buf, sizeof buf, 12.3f, 1, 5, FMT_ZERO_PAD, "KHz"); // "012.3KHz"
 */
#ifndef XM_POWER_KIT_FORMAT_H
#define XM_POWER_KIT_FORMAT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* fmt_fixed 标志位 */
#define FMT_SIGN        0x01u   /* 正数也输出 '+'（对应 "%+.2f"） */
#define FMT_ZERO_PAD    0x02u   /* 不足 min_width 时用 '0' 左填充（对应 "%05.2f"） */

/* ============================ 基础原语 ============================ */

/* 无符号整数补 0 追加到 *p（不写结束符），p 自动前移 */
void append_padded_uint(char **p, uint32_t val, uint8_t width);

/* 无符号整数补 0 到 width 位，写入 buf */
char *fmt_u32_padded(char *buf, size_t buf_size, uint32_t value, uint8_t width);

/* 定点格式化：四舍五入到 decimals 位小数
 * min_width : 最小总宽度（含符号与小数点），0 表示不补齐
 * flags     : FMT_SIGN / FMT_ZERO_PAD 的组合
 * 返回 buf */
char *fmt_fixed(char *buf, size_t buf_size, float value,
                uint8_t decimals, uint8_t min_width, uint8_t flags);

/* 定点格式化 + 单位后缀 */
char *fmt_fixed_unit(char *buf, size_t buf_size, float value,
                     uint8_t decimals, uint8_t min_width, uint8_t flags,
                     const char *unit);

/* 定点格式化，并额外提取小数点后第 decimals+1 ... decimals+nextra 位的数字
 * （用于万用表的 "末位数字单独放大显示"）。extra 至少 nextra+1 字节。 */
char *fmt_fixed_ex(char *buf, size_t buf_size, float value,
                   uint8_t decimals, uint8_t min_width, uint8_t flags,
                   char *extra, uint8_t nextra);

/* ============================ 业务格式化 ============================ */

/* 自动小数位：总有效位数 target_digits（2~5），按数值量级自动分配小数位（DPS 显示用） */
int fmt_fixed_digits(float value, uint8_t target_digits, char *buf);

/* 固定 XX.XX（0.01 ~ max_value），2 位整数 + 2 位小数（DPS 设定值显示用） */
int fmt_fixed_00_00(float value, float max_value, char *buf);

/* XXXX.XXXWh（0 ~ 9999.999），能量显示用，内部使用 double 保证长时间累积精度 */
int fmt_energy_wh(double value, char *buf);

/* 秒 -> "HHH:MM:SS"（0~999:59:59） */
int fmt_hms(uint32_t seconds, char *buf);

/* 0~100 -> "XXX%" */
int fmt_percent_u8(uint8_t value, char *buf);

#ifdef __cplusplus
}
#endif

#endif /* XM_POWER_KIT_FORMAT_H */
