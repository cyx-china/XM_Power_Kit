#include "lcd_draw_api.h"
#include "UserTask.h"
#include "os_handles.h"

static void DrawBasicElement(void);
void cal_main_page_handler(KeyEventMsg_t msg);
void dps_cal_handler(KeyEventMsg_t msg);
void dso_cal_handler(KeyEventMsg_t msg);
void awg_cal_handler(KeyEventMsg_t msg);
void dmm_v_cal_handler(KeyEventMsg_t msg);
void dmm_a_cal_handler(KeyEventMsg_t msg);
void dmm_r_cal_handler(KeyEventMsg_t msg);


KeyEventMsg_t CAL_Keymsg;               // 按键事件消息缓存（添加CAL_前缀）


/**
 * @brief 选项信息结构体
 */
typedef struct {
    uint16_t x1;uint16_t y1;            // 卡片左上角坐标
    uint16_t x2;uint16_t y2;            // 卡片右下角坐标
    uint16_t charx;uint16_t chary;      // 卡片文字左上角坐标
    char* name;
} CalItem_t;

// 卡片参数表
static const CalItem_t CalItems[6] = {
     {10,40,154,96,36,59,"电源校准"},
     {165,40,309,96,179,59,"示波器校准"},
     {10,107,154,163,24,126,"信号源校准"},
     {165,107,309,163,179,126,"万用表电压"},
     {10,174,154,230,24,193,"万用表电流"},
     {165,174,309,230,179,193,"万用表电阻"}
};

#define CardFtColor             0x1908      // 卡片底色
#define CardOutlineColor        0x21aa      // 卡片描边色
#define CardOutlineSelectColor  0xe22c      // 卡片选中描边色
#define CardTextFtColor         0xef7d      // 卡片文字颜色
#define CardTextBgColor         0x1908      // 卡片文字背景色
#define CardTextIntervalSize    4           // 卡片间隔大小

// 绘制基础界面
static void DrawBasicElement(void) {
    // 顶栏
    lcd_draw_rect(0, 0, 319, 31, 0x1908, 1);                        // 底色
    lcd_draw_line(0, 31, 319, 31, 0x11ac);                                 // 分割线
    lcd_draw_string(120,7,"设备校准",&yahei18x18,0x24be,0x1908,2);
    // 卡片栏
    lcd_draw_rect(0, 32, 319, 239, 0x18c6, 1);                      // 底色
    // 绘制6个卡片
    for (uint8_t i = 0;i < 6;i ++) {
        lcd_draw_round_rect(CalItems[i].x1,CalItems[i].y1,CalItems[i].x2,CalItems[i].y2,10,CardFtColor,1);
        lcd_draw_round_rect(CalItems[i].x1,CalItems[i].y1,CalItems[i].x2,CalItems[i].y2,10,CardOutlineColor,0);
        lcd_draw_string(CalItems[i].charx, CalItems[i].chary, CalItems[i].name, &yahei20x20,CardTextFtColor,
                        CardTextBgColor,CardTextIntervalSize);
    }
    // 默认选中第一个卡片
    lcd_draw_round_rect(CalItems[0].x1,CalItems[0].y1,CalItems[0].x2,CalItems[0].y2,10,CardOutlineSelectColor,0);

}

/**
 * @brief 校准页面枚举
 * @note  定义校准系统功能页面
 */
typedef enum {
    PAGE_MAIN = 0,      // 主界面（默认页面）
    PAGE_DPS_CAL,       // 数控电源校准
    PAGE_DSO_CAL,       // 示波器校准
    PAGE_AWG_CAL,       // 波形发生器校准
    PAGE_DMM_V_CAL,     // 万用表电压档校准
    PAGE_DMM_A_CAL,     // 万用表电流档校准
    PAGE_DMM_R_CAL,     // 万用表电阻档校准
    PAGE_NUM            // 页面总数
} AWG_AppPage_t;

AWG_AppPage_t CAL_current_page = PAGE_MAIN;     // 当前激活页面（添加CAL_前缀）
AWG_AppPage_t CAL_previous_page = PAGE_MAIN;    // 上一个页面（用于返回）（添加CAL_前缀）


/* ========================== 页面处理函数回调表 ========================== */
/**
 * @brief 页面处理函数表驱动
 * @note  索引对应AppPage_t枚举，快速映射页面与处理函数
 */
static void (*page_handlers[PAGE_NUM])(KeyEventMsg_t) = {
    [PAGE_MAIN] = cal_main_page_handler ,
    [PAGE_DPS_CAL] = dps_cal_handler,
    [PAGE_DSO_CAL] = dso_cal_handler,
    [PAGE_AWG_CAL] = awg_cal_handler,
    [PAGE_DMM_V_CAL] = dmm_v_cal_handler,
    [PAGE_DMM_A_CAL] = dmm_a_cal_handler,
    [PAGE_DMM_R_CAL] = dmm_r_cal_handler,
};

/**
 * @brief 主界面光标枚举
 * @note  主界面可聚焦的操作项
 */
typedef enum {
    Main_DPS = 0,   // 电源
    Main_DSO,       // 示波器
    Main_AWG,       // 信号源
    Main_Voltage,   // 万用表电压
    Main_Current,   // 万用表电流
    Main_Resistance,// 万用表电阻
} MainPageCursor_e;

MainPageCursor_e CAL_main_page_cursor_current = Main_DPS;   // 主界面当前光标位置
MainPageCursor_e CAL_main_page_cursor_past = Main_DPS;      // 主界面上一次光标位置

void Start_CalibrateTask(void *argument) {
    osThreadSuspend(CalibrateTaskHandle);       // 任务自挂启

    for (;;) {
        if (osMessageQueueGet(KeyEventQueueHandle, &CAL_Keymsg, NULL, 0) == osOK)  // 替换为CAL_Keymsg
            page_handlers[CAL_current_page](CAL_Keymsg);  // 替换为CAL_current_page和CAL_Keymsg

    }
}

/* ========================== 页面处理函数实现 ========================== */
void cal_main_page_handler(KeyEventMsg_t msg) {
    // 向上/左切换（对应原逻辑：DPS→Resistance→Current→Voltage→AWG→DSO→DPS）
    if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT || msg.event == ENCODER_EVENT_PRESS_LEFT) {
        CAL_main_page_cursor_past = CAL_main_page_cursor_current;
        CAL_main_page_cursor_current = (CAL_main_page_cursor_current > Main_DPS)
                                       ? (CAL_main_page_cursor_current - 1)
                                       : Main_Resistance;
    }
    // 向下/右切换（对应原逻辑：DPS→DSO→AWG→Voltage→Current→Resistance→DPS）
    else if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT || msg.event == ENCODER_EVENT_PRESS_RIGHT) {
        CAL_main_page_cursor_past = CAL_main_page_cursor_current;
        CAL_main_page_cursor_current = (CAL_main_page_cursor_current < Main_Resistance)
                                       ? (CAL_main_page_cursor_current + 1)
                                       : Main_DPS;
    }
    // 短按SET/编码器：进入选中项的编辑页面
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {

    }
    // 长按编码器：切换到LVGL APP
    else if (msg.key == KEY_ENCODER && msg.event == KEY_EVENT_LONG_PRESS) {
        AppListType APP = APP_LVGL;
        StartBeezer(0);
        osMessageQueuePut(AppSwitchQueueHandle, &APP, 0, 10);
        return;
    }
    // 无匹配按键事件：直接返回
    else return;

    // 更新显示
    // 清空上一个选项框
    lcd_draw_round_rect(CalItems[CAL_main_page_cursor_past].x1, CalItems[CAL_main_page_cursor_past].y1,  // 替换为CAL_前缀变量
                        CalItems[CAL_main_page_cursor_past].x2, CalItems[CAL_main_page_cursor_past].y2, 10,
                        CardOutlineColor, 0); // 替换为CAL_前缀变量

    // 绘制当前选项框
    lcd_draw_round_rect(CalItems[CAL_main_page_cursor_current].x1, CalItems[CAL_main_page_cursor_current].y1,  // 替换为CAL_前缀变量
                        CalItems[CAL_main_page_cursor_current].x2, CalItems[CAL_main_page_cursor_current].y2, 10,
                        CardOutlineSelectColor, 0); // 替换为CAL_前缀变量




}

void dps_cal_handler(KeyEventMsg_t msg) {

}

void dso_cal_handler(KeyEventMsg_t msg) {

}

void awg_cal_handler(KeyEventMsg_t msg) {

}

void dmm_v_cal_handler(KeyEventMsg_t msg) {

}

void dmm_a_cal_handler(KeyEventMsg_t msg) {

}

void dmm_r_cal_handler(KeyEventMsg_t msg) {

}

void Suspend_CalibrateTask(void) {
    Suspend_IndevDetectTask();              // 挂起输入设备检测任务
    osThreadSuspend(CalibrateTaskHandle);
}

void Resume_CalibrateTask(void) {
    DrawBasicElement();                      // 绘制基础UI界面
    osDelay(150);                       // 延时确保UI绘制完成
    osThreadResume(CalibrateTaskHandle);     // 恢复DPS核心任务
    Resume_IndevDetectTask();                // 恢复输入设备检测任务
}