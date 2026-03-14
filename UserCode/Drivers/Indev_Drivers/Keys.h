#ifndef XM_POWER_KIT_KEYS_H
#define XM_POWER_KIT_KEYS_H

#define KEY_NUM 5
#include <stdint.h>

// 外设定义
typedef enum {
    KEY_MODE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_SET,
    KEY_PWR,
    KEY_ENCODER
} IndevTypeDef;

// 事件类型
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_CLICK,          // 单击事件
    KEY_EVENT_LONG_PRESS,     // 长按首次触发
    KEY_EVENT_LONG_HOLD,      // 长按连发（每 LONG_HOLD_INTERVAL 触发一次）
    ENCODER_EVENT_LEFT,       // 编码器左转（未按下）
    ENCODER_EVENT_RIGHT,      // 编码器右转（未按下）
    ENCODER_EVENT_PRESS_LEFT, // 编码器左转（按下期间）
    ENCODER_EVENT_PRESS_RIGHT,// 编码器右转（按下期间）
} IndevEventTypeDef;

// 状态枚举
typedef enum {
    KEY_STATE_IDLE = 0,
    KEY_STATE_DEBOUNCE_PRESS,     // 消抖中 - 按下
    KEY_STATE_PRESSED,            // 已确认按下
    KEY_STATE_LONG_HOLD           // 长按连发阶段
} KeyStateTypeDef;

// 按键结构体
typedef struct {
    KeyStateTypeDef state;          // 当前状态
    uint8_t level;                  // 当前电平 1=按下 0=释放
    uint8_t last_level;
    uint16_t press_cnt;             // 按下计时（扫描周期:10ms/单位）
    uint16_t long_hold_cnt;         // 长按连发计时
} KeyStruct_t;

typedef struct {
    IndevTypeDef key;                 // 发送事件的按键
    IndevEventTypeDef event;          // 按键事件类型
} KeyEventMsg_t;

// 参数（单位：扫描周期个数，例如 10ms 扫描一次）
#define DEBOUNCE_CYCLES       2      // 20ms 消抖
#define CLICK_MAX_CYCLES      30     // < 300ms 算单击
#define LONG_PRESS_CYCLES     100     // >= 1000ms 进入长按
#define LONG_HOLD_INTERVAL    20     // 每 200ms 触发一次 LONG_HOLD

// 函数声明
void Key_Init(void);
void Key_Scan(void);                // 10ms 调用一次
void Key_Reset(void);

extern KeyStruct_t keys[KEY_NUM];

#endif //XM_POWER_KIT_KEYS_H