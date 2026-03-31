#include "DMM_A_Calibrate.h"
#include <inttypes.h>
#include <stdio.h>
#include "lcd_draw_api.h"
#include "SwitchManager.h"
#include "tim.h"
#include "UserDefineManage.h"

// 函数声明
void DMM_ADC_Init(void);
void CB_DMM_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);
int FormatFloatWithTenThousandth(float num, char *formatted_buf, char *tenthousandth_buf);
void DMM_ADC_DeInit(void);

// 全局变量声明
extern volatile uint16_t dmm_adc_raw_buf[600];
extern volatile float DMM_Current;                  // 电流测量实际值

// 静态缓存/参数存储变量
static char Dmm_A_Original_Msg[10] = {0};           // 原点校准值
static char Dmm_A_coefficient_Msg[10] = {0};        // 放大校准系数
static char DMM_A_Cur_formatted_buf[8] = {0};       // 电压格式化缓存
static char DMM_A_Cur_tenthousandth_buf[2] = {0};   // 电压千分位缓存

// 绘制万用表电流档校准初始页面
static void DrawDmmACalibratePage(void)
{
    // ===== 底色绘制 ===== //
    lcd_draw_rect(0, 0, 319, 31, 0x1908, 1);
    lcd_draw_rect(0, 32, 319, 239, 0x18c6, 1);

    // ===== 顶栏绘制 ===== //
    lcd_draw_line(0, 31, 319-1, 31, 0x11ac);
    lcd_draw_string(105, 6, "电流表校准", &yahei20x20, 0x24be, 0x1908, 3);

    // ===== 进度提示绘制 ===== //
    lcd_draw_round_rect(18, 47, 108, 73, 8, 0x048A, 1);
    lcd_draw_string(28, 52, "原点校准", &yahei16x16, 0xFFFF, 0x048A, 2);
    lcd_draw_round_rect(212, 47, 302, 73, 8, 0x632C, 1);
    lcd_draw_string(222, 52, "系数校准", &yahei16x16, 0xDEFB, 0x632C, 2);

    // ===== 显示区域绘制 ===== //
    lcd_draw_round_rect(15, 82, 305, 146, 10, 0x0000, 1);
    lcd_draw_round_rect(15, 82, 305, 146, 10, 0x632C, 0);
    lcd_draw_string(41, 90, "+0.000", &DIN_Medium32x48, 0x5cbd, 0x0000, 2);
    lcd_draw_string(234, 90, "0", &DIN_Medium32x48, 0x632c, 0x0000, 2);

    // ===== 提示框绘制 ===== //
    lcd_draw_round_rect(15, 158, 305, 188, 8, 0x29a7, 1);
    lcd_draw_round_rect(15, 158, 305, 188, 8, 0x632C, 0);
    lcd_draw_string(25, 165, "请使数值接近0。", &yahei16x16, 0xdf3c, 0x29a7, 1);

    // ===== 调节区绘制 ===== //
    lcd_draw_round_rect(15, 200, 212, 231, 8, 0x07E0, 0);
    lcd_draw_string(25, 208, "输出原点:", &yahei16x16, 0xFFFF, 0x18c6, 1);
    snprintf(Dmm_A_Original_Msg, sizeof(Dmm_A_Original_Msg), "%+03" PRId16, UserParam.DMM_Current_Original);
    lcd_draw_string(142, 207, Dmm_A_Original_Msg, &yahei18x18, 0xFFFF, 0x18c6, 2);

    // ===== 下一步按钮绘制 ===== //
    lcd_draw_round_rect(220, 200, 305, 231, 8, 0x632C, 1);
    lcd_draw_string(235, 207, "下一步", &yahei18x18, 0xFFFF, 0x632C, 1);
}

void DMM_A_Calibrate_Enter(void)
{
    DrawDmmACalibratePage();

    DMM_ADC_Init();                          // 初始化ADC
    osDelay(100);                            // 延时确保界面绘制完成
    // 注册ADC转换完成回调
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_COMPLETE_CB_ID, CB_DMM_ADC_ConvCpltCallback);
    AdcTim_OFF();                            // 先关闭ADC触发

    // 启动ADC DMA采集（600个数据）
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)dmm_adc_raw_buf, 600);
    // 配置定时器8：20Khz触发（预分频0，自动重装7199）
    __HAL_TIM_SET_PRESCALER(&htim8, 0);
    __HAL_TIM_SET_AUTORELOAD(&htim8, 7200 - 1);
    AdcTim_ON();                             // 开启ADC触发

    DMM_Switch_Mode(Dmm_Mode_Current);       // 电流模式
}

void DMM_A_Calibrate_Refresh(void)
{
    FormatFloatWithTenThousandth(DMM_Current, DMM_A_Cur_formatted_buf, DMM_A_Cur_tenthousandth_buf);
    lcd_draw_string(41, 90, DMM_A_Cur_formatted_buf, &DIN_Medium32x48, 0x5cbd, 0x0000, 2);
    lcd_draw_string(234, 90, DMM_A_Cur_tenthousandth_buf, &DIN_Medium32x48, 0x632c, 0x0000, 2);
}

typedef enum
{
    DmmACal_Origin_Main = 0,
    DmmACal_Origin_Edit,
    DmmACal_Coefficient_Main,
    DmmACal_Coefficient_Edit,
    DmmACal_Page_Num
} DmmACalPage_e;

DmmACalPage_e DmmACal_current_page = DmmACal_Origin_Main;

// 页面处理函数声明
static void DmmACal_Origin_Main_handler(KeyEventMsg_t msg);
static void DmmACal_Origin_Edit_handler(KeyEventMsg_t msg);
static void DmmACal_Coefficient_Main_handler(KeyEventMsg_t msg);
static void DmmACal_Coefficient_Edit_handler(KeyEventMsg_t msg);

static void (*DmmACalPage_handlers[DmmACal_Page_Num])(KeyEventMsg_t) = {
    [DmmACal_Origin_Main] = DmmACal_Origin_Main_handler,
    [DmmACal_Origin_Edit] = DmmACal_Origin_Edit_handler,
    [DmmACal_Coefficient_Main] = DmmACal_Coefficient_Main_handler,
    [DmmACal_Coefficient_Edit] = DmmACal_Coefficient_Edit_handler,
};

void dmm_a_cal_handler(KeyEventMsg_t msg)
{
    DmmACalPage_handlers[DmmACal_current_page](msg);
}

uint8_t DmmACal_Origin_Edit_Cursor = 0;
static void DmmACal_Origin_Main_handler(KeyEventMsg_t msg)
{
    if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT || msg.event == ENCODER_EVENT_PRESS_LEFT)
    {
        DmmACal_Origin_Edit_Cursor = (DmmACal_Origin_Edit_Cursor < 1) ? (DmmACal_Origin_Edit_Cursor + 1) : 0;
    }
    else if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT || msg.event == ENCODER_EVENT_PRESS_RIGHT)
    {
        DmmACal_Origin_Edit_Cursor = (DmmACal_Origin_Edit_Cursor > 0) ? (DmmACal_Origin_Edit_Cursor - 1) : 1;
    }
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK)
    {
        if (DmmACal_Origin_Edit_Cursor == 0) // 选中参数编辑框
        {
            DmmACal_current_page = DmmACal_Origin_Edit;               // 切换到原点编辑页面
            lcd_draw_line(162, 225, 191, 225, 0x87d3);   // 绘制编辑下划线示意条
        }
        else if (DmmACal_Origin_Edit_Cursor == 1)
        {
            DmmACal_current_page = DmmACal_Coefficient_Main;
            // 更新提示框文字
            lcd_draw_round_rect(15, 158, 305, 188, 8, 0x29a7, 1);
            lcd_draw_round_rect(15, 158, 305, 188, 8, 0x632C, 0);
            lcd_draw_string(25, 165, "测已知电流，使显示值等于实际值", &yahei16x16, 0xdf3c, 0x29a7, 1);

            // 更新编辑框提示文字
            lcd_draw_round_rect(15, 200, 212, 231, 8, 0x18c6, 1);
            lcd_draw_round_rect(15, 200, 212, 231, 8, 0x07E0, 0);
            lcd_draw_round_rect(220, 200, 305, 231, 8, 0x632C, 0);
            lcd_draw_string(25, 208, "电流系数:", &yahei16x16, 0xFFFF, 0x18c6, 1);

            // 更新编辑框数据
            snprintf(Dmm_A_coefficient_Msg, sizeof(Dmm_A_coefficient_Msg), "%05.2f", UserParam.DMM_Current_Factor);
            lcd_draw_string(110, 207, Dmm_A_coefficient_Msg, &yahei18x18, 0xFFFF, 0x18c6, 0);

            lcd_draw_round_rect(18, 47, 108, 73, 8, 0x632C, 1);
            lcd_draw_string(28, 52, "原点校准", &yahei16x16, 0xDEFB, 0x632C, 2);
            lcd_draw_round_rect(212, 47, 302, 73, 8, 0x048A, 1);
            lcd_draw_string(222, 52, "系数校准", &yahei16x16, 0xFFFF, 0x048A, 2);

            StartBeezer(0);
            return;
        }
    }
    else
    {
        return;
    }

    StartBeezer(0);
    // 更新选择焦点框
    if (DmmACal_Origin_Edit_Cursor == 0)
    {
        lcd_draw_round_rect(15, 200, 212, 231, 8, 0x07E0, 0);
        lcd_draw_round_rect(220, 200, 305, 231, 8, 0x632C, 0);
    }
    else
    {
        lcd_draw_round_rect(15, 200, 212, 231, 8, 0x632C, 0);
        lcd_draw_round_rect(220, 200, 305, 231, 8, 0x07E0, 0);
    }
}

static void DmmACal_Origin_Edit_handler(KeyEventMsg_t msg)
{
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT)
    {
        UserParam.DMM_Current_Original += 1;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT)
    {
        UserParam.DMM_Current_Original -= 1;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER)
    {
        DmmACal_current_page = DmmACal_Origin_Main;
        lcd_draw_line(162, 225, 191, 225, 0x18c6);
    }
    else
    {
        return;
    }

    StartBeezer(0);
    snprintf(Dmm_A_Original_Msg, sizeof(Dmm_A_Original_Msg), "%+03" PRId16, UserParam.DMM_Current_Original);
    lcd_draw_string(142, 207, Dmm_A_Original_Msg, &yahei18x18, 0xFFFF, 0x18c6, 2);
}

uint8_t DmmACal_Coefficient_Edit_Cursor = 0;
static void DmmACal_Coefficient_Main_handler(KeyEventMsg_t msg)
{
    if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT || msg.event == ENCODER_EVENT_PRESS_LEFT)
    {
        DmmACal_Coefficient_Edit_Cursor = (DmmACal_Coefficient_Edit_Cursor < 1) ? (DmmACal_Coefficient_Edit_Cursor + 1) : 0;
    }
    else if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT || msg.event == ENCODER_EVENT_PRESS_RIGHT)
    {
        DmmACal_Coefficient_Edit_Cursor = (DmmACal_Coefficient_Edit_Cursor > 0) ? (DmmACal_Coefficient_Edit_Cursor - 1) : 1;
    }
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK)
    {
        if (DmmACal_Coefficient_Edit_Cursor == 0) // 选中参数编辑框
        {
            DmmACal_current_page = DmmACal_Coefficient_Edit;               // 切换到系数编辑页面
            lcd_draw_line(110, 225, 186, 225, 0x87d3);
        }
        else if (DmmACal_Coefficient_Edit_Cursor == 1)
        {
            // 完成校准，保存配置，清理并退出
            UserParam_SaveAllValues();  // 保存配置
            Calibrate_ReturnToMain();   // 返回主界面
            StartBeezer(0);

            DMM_Switch_Mode(Dmm_Mode_Voltage);                   // 切换到电压模式
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET); // 关闭电压档硬件

            DMM_ADC_DeInit(); // 反初始化ADC
            return;
        }
    }
    else
    {
        return;
    }

    StartBeezer(0);
    // 更新选择焦点框
    if (DmmACal_Coefficient_Edit_Cursor == 0)
    {
        lcd_draw_round_rect(15, 200, 212, 231, 8, 0x07E0, 0);
        lcd_draw_round_rect(220, 200, 305, 231, 8, 0x632C, 0);
    }
    else
    {
        lcd_draw_round_rect(15, 200, 212, 231, 8, 0x632C, 0);
        lcd_draw_round_rect(220, 200, 305, 231, 8, 0x07E0, 0);
    }
}

static void DmmACal_Coefficient_Edit_handler(KeyEventMsg_t msg)
{
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT)
    {
        UserParam.DMM_Current_Factor += 0.01f;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT)
    {
        UserParam.DMM_Current_Factor -= 0.01f;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER)
    {
        DmmACal_current_page = DmmACal_Coefficient_Main;
        lcd_draw_line(110, 225, 186, 225, 0x18c6);
    }
    else
    {
        return;
    }

    StartBeezer(0);
    snprintf(Dmm_A_coefficient_Msg, sizeof(Dmm_A_coefficient_Msg), "%05.2f", UserParam.DMM_Current_Factor);
    lcd_draw_string(110, 207, Dmm_A_coefficient_Msg, &yahei18x18, 0xFFFF, 0x18c6, 0);
}