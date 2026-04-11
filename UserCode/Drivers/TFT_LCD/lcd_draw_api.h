#ifndef XM_POWER_KIT_LCD_DRAW_API_H
#define XM_POWER_KIT_LCD_DRAW_API_H

#include "cmsis_os2.h"
#include "font.h"
#include "images.h"
#include "os_handles.h"
#include "UserTask.h"

/**
 * @brief 清屏（封装后的上层接口，区别于底层GFX_ClearScreen）
 * @param color 清屏颜色（RGB565格式）
 * @retval osOK-成功，osError-失败
 */
osStatus_t lcd_draw_clear_screen(uint16_t color);

/**
 * @brief 绘制单个像素（封装后的上层接口）
 * @param x/y 像素坐标
 * @param color 像素颜色
 * @retval osOK-成功，osError-失败
 */
osStatus_t lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief 绘制直线（封装后的上层接口）
 * @param x1/y1 起点坐标；x2/y2 终点坐标；color 直线颜色
 * @retval osOK-成功，osError-失败
 */
osStatus_t lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/**
 * @brief 绘制虚线（封装后的上层接口）
 * @param x1/y1 起点；x2/y2 终点；color 颜色；interval 间隔；dot_num 点数
 * @retval osOK-成功，osError-失败
 */
osStatus_t lcd_draw_dotted_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                uint16_t color, uint16_t interval, uint16_t dot_num);

/**
 * @brief 绘制矩形（封装后的上层接口）
 * @param x1/y1 左上坐标；x2/y2 右下坐标；color 颜色；is_filled 0-空心/1-实心
 * @retval osOK-成功，osError-失败
 */
osStatus_t lcd_draw_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                         uint16_t color, uint8_t is_filled);

/**
 * @brief 绘制圆角矩形（封装后的上层接口）
 * @param x1/y1 左上；x2/y2 右下；r 圆角半径；color 颜色；is_filled 0/1
 * @retval osOK-成功，osError-失败
 */
osStatus_t lcd_draw_round_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                               uint16_t r, uint16_t color, uint8_t is_filled);

/**
 * @brief 绘制圆（封装后的上层接口）
 * @param x0/y0 圆心；r 半径；color 颜色；is_filled 0-空心/1-实心
 * @retval osOK-成功，osError-失败
 */
osStatus_t lcd_draw_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color, uint8_t is_filled);

/**
 * @brief 绘制椭圆（封装后的上层接口）
 * @param x0/y0 中心；a 长轴；b 短轴；color 颜色；is_filled 0/1
 * @retval osOK-成功，osError-失败
 */
osStatus_t lcd_draw_ellipse(uint16_t x0, uint16_t y0, uint16_t a, uint16_t b,
                            uint16_t color, uint8_t is_filled);

/**
 * @brief 绘制图片（封装后的上层接口）
 * @param x/y 绘制起点；image 图片信息指针
 * @retval osOK-成功，osError-失败
 */
osStatus_t lcd_draw_image(uint16_t x, uint16_t y, const ImageInfo* image);

/**
 * @brief 绘制字符串（封装后的上层接口）
 * @param x/y 绘制起点；str 字符串内容；font 字体信息；color 文字颜色；spacing 字符间距
 * @retval osOK-成功，osError-失败
 */
osStatus_t lcd_draw_string(uint16_t x, uint16_t y, const char* str,
                           const FontInfo* font, uint16_t ft_color,uint16_t bg_color, int8_t spacing);


osStatus_t lcd_draw_data(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1,uint16_t *data,uint16_t len);

osStatus_t lcd_draw_IsoscelesTriangle(uint16_t x,uint16_t y,Triangle_Direction_e direction,uint16_t hight,uint16_t color);

#endif //XM_POWER_KIT_LCD_DRAW_API_H