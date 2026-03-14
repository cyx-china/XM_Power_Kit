#ifndef XM_POWER_KIT_SWITCHMANAGER_H
#define XM_POWER_KIT_SWITCHMANAGER_H

#include "stm32f4xx_hal.h"

// 系统小灯
#define SysLed_ON()         HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET)          // 开启系统LED
#define SysLed_OFF()        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET)        // 关闭系统LED

// 数控电源
#define DpsGnd_ON()         HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_SET)         // 开启数控电源输出地
#define DpsGnd_OFF()        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_RESET)       // 关闭数控电源输出地

#define DpsDc_ON()          HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_RESET)       // 开启数控电源输出正极
#define DpsDc_OFF()         HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_SET)         // 关闭数控电源输出正极

#define DpsRelease_ON()     HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET)          // 开启泄放电阻
#define DpsRelease_OFF()    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_RESET)        // 关闭泄放电阻

#define DpsPower_ON()       HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET)          // 开启输出
#define DpsPower_OFF()      HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET)        // 关闭输出

#define DpsTim_ON()         HAL_TIM_Base_Start(&htim8)                                  // 开启定时器(用于ADC)
#define DpsTim_OFF()        HAL_TIM_Base_Stop(&htim8)                                   // 关闭定时器

// 示波器
typedef enum {
    Couple_DC = 0,                                  // 直流耦合（保留信号直流分量）
    Couple_AC                                       // 交流耦合（滤除信号直流分量）
} CoupleTypeDef;

typedef enum {
    Attenuation_1 = 0,                              // 1倍衰减（信号无衰减）
    Attenuation_5                                   // 5倍衰减（信号幅值缩小5倍）
} AttenuationTypeDef;

typedef enum {
    Magnify_1 = 0,                                   // 1倍放大
    Magnify_2,                                       // 2倍放大
    Magnify_4,                                       // 4倍放大
    Magnify_8,                                       // 8倍放大
    Magnify_16,                                      // 16倍放大
    Magnify_32,                                      // 32倍放大
    Magnify_64,                                      // 64倍放大
    Magnify_128                                      // 128倍放大
} MagnifyTypeDef;

void OSC_Magnify_Ctrl(MagnifyTypeDef magnify_mode);                 // 配置耦合方式
void OSC_Attenuation_Ctrl(AttenuationTypeDef attenuation_mode);     // 配置衰减倍率
void OSC_Couple_Ctrl(CoupleTypeDef couple_mode);                    // 配置放大倍率

// 万用表









#endif //XM_POWER_KIT_SWITCHMANAGER_H