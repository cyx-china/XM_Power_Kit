/*
 * 此文件用于定义整个设备的开关控制逻辑.
 * 大部分都是使用宏定义，少数使用函数
 */
#include "SwitchManager.h"


void OSC_Couple_Ctrl(CoupleTypeDef couple_mode) {
    if (couple_mode == Couple_DC) {
        GPIOC->ODR &= ~GPIO_PIN_2;  // PC2 = 0（直流耦合）
    } else {
        GPIOC->ODR |= GPIO_PIN_2;   // PC2 = 1（交流耦合）
    }
}

void OSC_Attenuation_Ctrl(AttenuationTypeDef attenuation_mode) {
    if (attenuation_mode == Attenuation_1) {
        GPIOC->ODR |= GPIO_PIN_1;   // PC1 = 1（1倍衰减）
    } else {
        GPIOC->ODR &= ~GPIO_PIN_1;  // PC1 = 0（5倍衰减）
    }
}

void OSC_Magnify_Ctrl(MagnifyTypeDef magnify_mode) {

    GPIOC->ODR &= ~(GPIO_PIN_14 | GPIO_PIN_15);
    GPIOC->ODR |= GPIO_PIN_13 ;
    // 根据放大倍数设置对应引脚状态
    switch (magnify_mode) {
        case Magnify_1:
            break;
        case Magnify_2:
            GPIOC->ODR |= GPIO_PIN_13 | GPIO_PIN_14;  // PC13=1, PC14=1, PC15=0
            break;
        case Magnify_4:
            GPIOC->ODR |= GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;  // 全1
            break;
        case Magnify_8:
            GPIOC->ODR |= GPIO_PIN_13 | GPIO_PIN_15;  // PC13=1, PC14=0, PC15=1
            break;
        case Magnify_16:
            GPIOC->ODR |= GPIO_PIN_14 | GPIO_PIN_15;  // PC13=0, PC14=1, PC15=1
            break;
        case Magnify_32:
            GPIOC->ODR &= ~(GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);   // PC13=0, PC14=0, PC15=0
            break;
        case Magnify_64:
            GPIOC->ODR |= GPIO_PIN_15;  // PC13=0, PC14=0, PC15=1
            break;
        case Magnify_128:
            GPIOC->ODR |= GPIO_PIN_13 | GPIO_PIN_15;  // PC13=1, PC14=0, PC15=1
            break;
        default:
            break;
    }
}