#include "Encoder.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "Keys.h"           // 为了 KeyEventMsg_t 和 IndevEventTypeDef
#include "lv_hal_indev.h"
#include "os_handles.h"
#include "stm32f4xx_hal.h"
#include "tim.h"
#include "UserTask.h"

static int32_t get_encoder_delta(void);
static bool is_encoder_button_pressed(void);

// 旋转相关
static int32_t last_cnt           = 0;
static int32_t accumulated_pulses = 0;
static int32_t last_sign          = 0;

// ────────────────────────────────────────────────
//          编码器按键独立状态机
// ────────────────────────────────────────────────

typedef enum {
    BTN_IDLE = 0,
    BTN_DEBOUNCE,
    BTN_PRESSED,
    BTN_LONG_HOLD
} EncoderBtnState_t;

typedef struct {
    EncoderBtnState_t state;
    uint8_t  level;             // 稳定后的电平
    uint8_t  last_raw;
    uint16_t press_cnt;         // 按下计时
    uint16_t long_hold_cnt;     // 长按连发计时
    bool     rotated_while_down;// 本次按下期间是否旋转过
} EncoderButton_t;

static EncoderButton_t enc_btn = {0};

static KeyEventMsg_t msg;

// ────────────────────────────────────────────────
//          辅助函数
// ────────────────────────────────────────────────

static int32_t get_encoder_delta(void)
{
    int32_t current = (int32_t)__HAL_TIM_GET_COUNTER(&htim4);
    int32_t delta   = current - last_cnt;

    if (delta > 32767)  delta -= 65536;
    if (delta < -32767) delta += 65536;

    last_cnt = current;
    return delta;
}

static bool is_encoder_button_pressed(void)
{
    return (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6) == GPIO_PIN_RESET);
}

// ────────────────────────────────────────────────
//          编码器按键扫描（类似 Keys 的逻辑）
// ────────────────────────────────────────────────

static void EncoderButton_Scan(void)
{
    uint8_t raw = is_encoder_button_pressed() ? 1 : 0;
    EncoderButton_t *b = &enc_btn;

    // 消抖计数
    if (raw == b->last_raw) {
        if (raw == 1) {
            if (b->press_cnt < 0xFFFF) b->press_cnt++;
        } else {
            b->press_cnt = 0;
            b->long_hold_cnt = 0;
        }
    } else {
        b->press_cnt = 0;
        b->long_hold_cnt = 0;
    }
    b->last_raw = raw;

    // 稳定后进入状态机
    if (b->press_cnt >= DEBOUNCE_CYCLES) {
        b->level = 1;

        if (b->state == BTN_IDLE) {
            b->state = BTN_PRESSED;
            b->press_cnt = 0;
            b->rotated_while_down = false;
        }

        if (b->state == BTN_PRESSED) {
            b->press_cnt++;

            if (b->press_cnt >= LONG_PRESS_CYCLES) {
                // 只在“未旋转过”的情况下发长按事件
                if (!b->rotated_while_down) {
                    msg.key = KEY_ENCODER;  // 保持兼容，或改成 BTN_ENCODER
                    msg.event = KEY_EVENT_LONG_PRESS;
                    osMessageQueuePut(KeyEventQueueHandle, &msg, 0, 0);
                }
                b->state = BTN_LONG_HOLD;
                b->long_hold_cnt = 0;
            }
        }

        if (b->state == BTN_LONG_HOLD) {
            b->long_hold_cnt++;

            if (b->long_hold_cnt >= LONG_HOLD_INTERVAL) {
                if (!b->rotated_while_down) {
                    msg.key = KEY_ENCODER;
                    msg.event = KEY_EVENT_LONG_HOLD;
                    osMessageQueuePut(KeyEventQueueHandle, &msg, 0, 0);
                }
                b->long_hold_cnt = 0;
            }
        }
    }
    // 释放
    else if (b->level == 1 && raw == 0) {
        if (b->state == BTN_PRESSED && b->press_cnt < LONG_PRESS_CYCLES) {
            if (!b->rotated_while_down) {
                msg.key = KEY_ENCODER;
                msg.event = KEY_EVENT_CLICK;
                osMessageQueuePut(KeyEventQueueHandle, &msg, 0, 0);
            }
        }

        // 重置
        b->state = BTN_IDLE;
        b->level = 0;
        b->press_cnt = 0;
        b->long_hold_cnt = 0;
        b->rotated_while_down = false;
    }
}

// ────────────────────────────────────────────────
//          公共函数
// ────────────────────────────────────────────────

bool get_encoder_data(int32_t *diff, bool *pressed)
{
    if (diff == NULL) return false;

    int32_t raw_delta = get_encoder_delta();
    accumulated_pulses += raw_delta;

    int32_t curr_sign = (accumulated_pulses > 0) ? 1 :
                        (accumulated_pulses < 0) ? -1 : 0;

    if (curr_sign != 0 && curr_sign != last_sign && last_sign != 0 &&
        abs(accumulated_pulses) < 6) {
        accumulated_pulses = 0;
    }
    last_sign = curr_sign;

    const int32_t DEADZONE = 3;
    int32_t abs_acc = abs(accumulated_pulses);

    if (abs_acc < DEADZONE) {
        *diff = 0;
    } else {
        *diff = accumulated_pulses / 4;
        accumulated_pulses -= (*diff * 4);
        last_sign = (*diff > 0) ? 1 : (*diff < 0) ? -1 : 0;
    }

    if (pressed) {
        *pressed = enc_btn.level;   // 使用我们自己维护的稳定状态
    }

    return (*diff != 0 || (pressed && *pressed));
}

void reset_encoder_state(void)
{
    last_cnt = 0;
    accumulated_pulses = 0;
    last_sign = 0;
    __HAL_TIM_SET_COUNTER(&htim4, 0);

    // 重置按键状态
    enc_btn.state = BTN_IDLE;
    enc_btn.level = 0;
    enc_btn.last_raw = 0;
    enc_btn.press_cnt = 0;
    enc_btn.long_hold_cnt = 0;
    enc_btn.rotated_while_down = false;
}

void Encoder_Init(void) {
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL); // 启动定时器
    reset_encoder_state();  // 重置编码器状态
}

// LVGL 回调
void encoder_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    static bool last_btn_state = 1;
    int32_t diff;
    bool pressed; // <- 这是个占位变量
    get_encoder_data(&diff, &pressed);

    bool is_pressed = is_encoder_button_pressed();  // LVGL基本不需要按键消抖，或者你消抖的时间小于LVGL按键扫描间隔
                                                         // 否则会吞事件，因此直接读电平传过去就行了
    data->enc_diff = (int16_t)diff;
    data->state = is_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (diff != 0) {
        StartBeezer(0);
        WakeUp();                // 唤醒
    }
    if (is_pressed == 0 && last_btn_state == 1)
    {
        StartBeezer(0);  // 按键按下 → 只响一次
        WakeUp();                // 唤醒
    }
    last_btn_state = is_pressed;
}

void Encoder_Scan(void)
{
    // 先扫描旋转
    int32_t diff;
    get_encoder_data(&diff, NULL);

    // 再扫描按键状态机
    EncoderButton_Scan();

    // 处理旋转事件
    if (diff != 0) {
        bool is_pressed = (enc_btn.level == 1);

        if (is_pressed) {
            enc_btn.rotated_while_down = true;

            msg.key = KEY_ENCODER;
            msg.event = (diff > 0) ? ENCODER_EVENT_PRESS_RIGHT : ENCODER_EVENT_PRESS_LEFT;
        } else {
            msg.key = KEY_ENCODER;
            msg.event = (diff > 0) ? ENCODER_EVENT_RIGHT : ENCODER_EVENT_LEFT;
        }

        osMessageQueuePut(KeyEventQueueHandle, &msg, 0, 0);
        WakeUp();   // 唤醒设备
    }
}