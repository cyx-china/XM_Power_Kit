#ifndef XM_POWER_KIT_LCD_ORIENTATION_H
#define XM_POWER_KIT_LCD_ORIENTATION_H

#include <stdint.h>

/*
 * ST7789 的 MADCTL(0x36) 方向常量，主程序与 bootloader 共用一份，避免两边算错。
 *
 *   MY(0x80) 行地址方向、MX(0x40) 列地址方向、MV(0x20) 行列交换
 *
 * 本机 DIS_DIR = 3，基准值 0x60（MY=0, MX=1, MV=1）为 320x240 横向正向。
 *   · 180° 旋转：同时翻转 MY 与 MX（0x60 -> 0xA0）
 *   · 左右镜像：只翻转 MX（0x60 -> 0x20）
 * 两者都打开时即上下镜像（0xE0）。
 */

#define LCD_MADCTL_DIR0             0x00U   /* 竖显     MY=0 MX=0 MV=0 */
#define LCD_MADCTL_DIR1             0xA0U   /* 横显 90  MY=1 MX=0 MV=1 */
#define LCD_MADCTL_DIR2             0xC0U   /* 竖显 180 MY=1 MX=1 MV=0 */
#define LCD_MADCTL_DIR3             0x60U   /* 横显 270 MY=0 MX=1 MV=1（本机使用） */

#define LCD_MADCTL_ROTATE180_MASK   0xC0U   /* MY | MX：180° 旋转 */
#define LCD_MADCTL_MIRROR_H_MASK    0x40U   /* MX：左右镜像（MV=1 时对应屏幕水平方向） */

/* 把 DIS_DIR 编译期方向换算成 MADCTL 基准值 */
#define LCD_MADCTL_FOR_DIR(dir)     (((dir) == 0) ? LCD_MADCTL_DIR0 : \
                                     ((dir) == 1) ? LCD_MADCTL_DIR1 : \
                                     ((dir) == 2) ? LCD_MADCTL_DIR2 : LCD_MADCTL_DIR3)

/* 基准方向 + 180° 旋转 + 左右镜像，组合出最终的 MADCTL 值 */
static inline uint8_t LCD_Orientation_Madctl(uint8_t base, uint8_t rotate180, uint8_t flip_horizontal)
{
    uint8_t value = base;

    if (rotate180 != 0U) {
        value ^= LCD_MADCTL_ROTATE180_MASK;
    }
    if (flip_horizontal != 0U) {
        value ^= LCD_MADCTL_MIRROR_H_MASK;
    }
    return value;
}

#endif /* XM_POWER_KIT_LCD_ORIENTATION_H */
