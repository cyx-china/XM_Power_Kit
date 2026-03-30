/*
 * 此文件用于定义整个设备的开关控制逻辑.
 * 大部分都是使用宏定义，少数使用函数
 */
#include "SwitchManager.h"

#include "cmsis_os2.h"

// 示波器

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


// 定义一个结构体来存储引脚配置
typedef struct {
    GPIO_TypeDef *GPIOx;       // GPIO端口
    uint16_t GPIO_Pin;          // 引脚号
    GPIO_PinState OffState;     // "关闭"状态下的电平
} DMM_GpioConfig;

// 万用表模式切换函数
void DMM_Switch_Mode(DmmModeTypeDef mode) {
    static const DMM_GpioConfig pins[] = {
        {GPIOA, GPIO_PIN_15, GPIO_PIN_RESET}, // [A15] 电流档开关    (关闭: Low)
        {GPIOC, GPIO_PIN_7,  GPIO_PIN_RESET}, // [C7]  模拟地       (关闭: Low)
        {GPIOC, GPIO_PIN_10, GPIO_PIN_RESET}, // [C10] 数控电源地    (关闭: Low)
        {GPIOC, GPIO_PIN_11, GPIO_PIN_SET},   // [C11] 数控电源正    (关闭: High)
        {GPIOC, GPIO_PIN_12, GPIO_PIN_RESET}, // [C12] 电压档开关    (关闭: Low)
        {GPIOD, GPIO_PIN_3,  GPIO_PIN_RESET}, // [D3]  泄放电阻      (关闭: Low)
        {GPIOE, GPIO_PIN_0,  GPIO_PIN_RESET}, // [E0]  电阻档开关    (关闭: Low)
        {GPIOE, GPIO_PIN_1,  GPIO_PIN_SET},   // [E1]  200Ω基准     (关闭: High)
        {GPIOE, GPIO_PIN_2,  GPIO_PIN_SET}    // [E2]  2KΩ基准      (关闭: High)
    };

    // 复位所有引脚到"关闭"状态
    for (uint8_t i = 0; i < (uint8_t)(sizeof(pins) / sizeof(pins[0])); i++) {
        HAL_GPIO_WritePin(pins[i].GPIOx, pins[i].GPIO_Pin, pins[i].OffState);
    }
    osDelay(20);

    // 开启对应模式
    switch (mode) {
        case Dmm_Mode_Voltage:
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET);     // 开启泄放
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);    // 打开电压档
            break;

        case Dmm_Mode_Resistance:
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET);     // 开启泄放
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0, GPIO_PIN_SET);     // 打开电阻档
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);     // 打开模拟地
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);   // 打开200Ω
            break;

        case Dmm_Mode_Current:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);    // 打开电流档
            break;

        case Dmm_Mode_Diode:
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET);     // 开启泄放
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);     // 打开模拟地
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0, GPIO_PIN_SET);     // 打开电阻档
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_RESET);   // 打开2kΩ
            break;
    }
    osDelay(20);
}