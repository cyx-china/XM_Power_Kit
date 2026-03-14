/**
 * @file lv_port_indev.c
 *
 */

/*Copy this file as "lv_port_indev.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/

#include "lv_port_indev.h"
#include "lvgl.h"

/*********************
 *      FUNCTIONS
 *********************/

extern void encoder_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data);     // 声明编码器读取回调函数





/*********************
 *   STATIC VARIABLES
 *********************/

lv_indev_t * indev_encoder;  // 声明一个指向编码器输入设备的指针
lv_indev_t * indev_keypad;   // 声明一个指向按键输入设备的指针


void lv_port_indev_init(void)
{
 // 初始化旋转编码器输入设备
 static lv_indev_drv_t encoder_drv;          // 声明并初始化一个输入设备驱动结构体变量

 lv_indev_drv_init(&encoder_drv);            // 初始化输入设备驱动结构体
 encoder_drv.type = LV_INDEV_TYPE_ENCODER;   // 设置输入设备类型为编码器
 encoder_drv.read_cb = encoder_read_cb;         // 设置读取回调函数
 indev_encoder = lv_indev_drv_register(&encoder_drv);  // 注册输入设备驱动并获取输入设备指针
 // 绑定编码器设备到默认组
 lv_group_t * group = lv_group_create();
 lv_group_set_default(group);
 lv_indev_set_group(indev_encoder, group);

 // 初始化键盘输入设备
}


#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
