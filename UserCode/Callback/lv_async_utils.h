#ifndef LV_ASYNC_UTILS_H
#define LV_ASYNC_UTILS_H

#include "lvgl.h"

/**
 * @file lv_async_utils.h
 * @brief LVGL 异步更新工具（每个控件类型独立实现）
 *        所有从其他任务安全更新 UI 的接口都放在这里
 */

/* ────────────────────────────── Label 专用 ────────────────────────────── */

/**
 * @brief Label 异步更新参数结构体
 */
typedef struct {
    lv_obj_t   *target;       // 目标 label 对象
    const char *text;         // 要设置的文本（NULL 或 "" 表示不改文本）
    uint16_t    color;        // RGB565 颜色值，COLOR_SKIP 表示不改颜色
} lv_async_label_update_t;

#define LV_ASYNC_COLOR_SKIP  0x0001u

/**
 * @brief 异步设置 label 的文本和/或颜色（RGB565）
 * @param target 目标 label 对象
 * @param text 新文本，传 NULL 或空字符串表示不更新文本
 * @param color RGB565 颜色值，传 LV_ASYNC_COLOR_SKIP 表示不修改颜色
 * 
 * 示例：
 *   lv_async_set_label_text(ui_label_temp, "28°C", 0x07E0);         // 绿色文字
 *   lv_async_set_label_text(ui_label_status, "已连接", 0x07E0);     // 绿色文字
 *   lv_async_set_label_text(ui_label_status, NULL, 0xF800);         // 只改成红色
 *   lv_async_set_label_text(ui_label_temp, "不变色", LV_ASYNC_COLOR_SKIP);  // 只改文字
 */
void lv_async_set_label_text(lv_obj_t *target, const char *text, uint16_t color);

/* ────────────────────────────── 未来扩展位置 ────────────────────────────── */

// 例如后续要加其他控件时：
// typedef struct { ... } lv_async_bar_update_t;
// void lv_async_set_bar_value(lv_obj_t *target, int32_t value, ...);

#endif /* LV_ASYNC_UTILS_H */