/*
 * 此文件用于定义整个设备的开关控制逻辑.
 * 大部分都是使用宏定义，少数使用函数
 */
#include "SwitchManager.h"

#include "cmsis_os2.h"
#include "tim.h"

// ======================= PWM 输出 ======================= //
// 按定时器实际重装载值换算比较值：percent=100 时输出 arr+1（恒为高电平，即真正的 100%）
static void PWM_SetDuty(uint32_t channel, uint8_t percent) {
    if (percent > 100) {
        percent = 100;
    }
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
    __HAL_TIM_SET_COMPARE(&htim1, channel, ((arr + 1U) * (uint32_t)percent) / 100U);
}

/*
 * 背光渐变：改亮度只记目标值，实际比较值由 PWM_BrightnessService() 每 BL_FADE_STEP_MS 走 1%。
 * 该服务在 TIM6 的 1ms 时基回调里调用，全程约 200ms，避免开关屏时亮度突跳。
 */
#define BL_FADE_STEP_MS     2U

static uint8_t s_bl_target  = 0;        // 目标亮度（0~100）
static uint8_t s_bl_current = 0;        // 当前实际亮度（0~100）
static uint8_t s_bl_divider = 0;        // 分频计数

void PWM_SetBrightness(uint8_t percent) {
    if (percent > 100U) {
        percent = 100U;
    }
    s_bl_target = percent;
}

void PWM_BrightnessService(void) {
    if (s_bl_current == s_bl_target) {
        s_bl_divider = 0;
        return;
    }
    if (++s_bl_divider < BL_FADE_STEP_MS) {
        return;
    }
    s_bl_divider = 0;
    if (s_bl_current < s_bl_target) {
        s_bl_current++;
    } else {
        s_bl_current--;
    }
    PWM_SetDuty(TIM_CHANNEL_2, s_bl_current);
}

void PWM_SetFanDuty(uint8_t percent) {
    PWM_SetDuty(TIM_CHANNEL_3, percent);
}

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
    // 第一步：先清零3个控制引脚的对应位，避免状态残留
    GPIOC->ODR &= ~(GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);

    // 根据放大倍数设置对应引脚状态
    switch (magnify_mode) {
        case Magnify_1:
            GPIOC->ODR |= GPIO_PIN_13;  // 100
            break;
        case Magnify_2:
            GPIOC->ODR |= GPIO_PIN_13 | GPIO_PIN_14;  // 110
            break;
        case Magnify_4:
            GPIOC->ODR |= GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;  // 111
            break;
        case Magnify_8:
            GPIOC->ODR |= GPIO_PIN_13 | GPIO_PIN_15;  // 101
            break;
        case Magnify_16:
            GPIOC->ODR |= GPIO_PIN_14 | GPIO_PIN_15;  // 011
            break;
        case Magnify_32:
            // 已提前清零，无需额外操作 000
            break;
        case Magnify_64:
            GPIOC->ODR |= GPIO_PIN_15;  // 001
            break;
        case Magnify_128:
            GPIOC->ODR |= GPIO_PIN_14;  // 010
            break;
        default:
            // 异常默认使用×1倍率
            GPIOC->ODR |= GPIO_PIN_13;
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

/**
 * @brief  切换电阻档的基准电阻（自动量程用）
 * @param  range: DMM_RES_RANGE_2K（200Ω 基准）/ DMM_RES_RANGE_30K（2KΩ 基准）
 * @note   两档都先关断再开目标档，避免切换瞬间两个基准电阻并联；
 *         换档后需要一段稳定时间，否则 ADC 读到的是切换过程。
 */
void DMM_Res_Range_Ctrl(DMM_Res_RangeTypeDef range) {
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);     // 关 200Ω
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_SET);     // 关 2KΩ

    if (range == DMM_RES_RANGE_2K) {
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_RESET);
    }
    osDelay(20);
}