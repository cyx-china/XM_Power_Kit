#include "DMM_V_Calibrate.h"
#include <inttypes.h>
#include <stdio.h>
#include "lcd_draw_api.h"
#include "SwitchManager.h"
#include "tim.h"
#include "UserDefineManage.h"

// 函数声明
void DMM_ADC_Init(void);
void CB_DMM_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);
int FormatFloatWithThousandth(float num, char *formatted_buf, char *thousandth_buf);
void DMM_ADC_DeInit(void);

// 全局变量声明
extern volatile uint16_t dmm_adc_raw_buf[600];
extern volatile float DMM_Voltage;                  // 电压测量实际值

// 静态缓存/参数存储变量
static char Dmm_V_Original_Msg[10] = {0};           // 原点校准值
static char Dmm_V_Positive_Msg[10] = {0};           // 正接校准系数
static char Dmm_V_Negative_Msg[10] = {0};           // 反接校准系数
static char DMM_V_Vol_formatted_buf[8] = {0};       // 电压格式化缓存
static char DMM_V_Vol_thousandth_buf[2] = {0};      // 电压千分位缓存

// 绘制万用表电压档校准初始页面
static void DrawDmmVCalibratePage(void)
{
    // ===== 底色绘制 ===== //
    lcd_draw_rect(0, 0, 319, 31, 0x1908, 1);
    lcd_draw_rect(0, 32, 319, 239, 0x18c6, 1);

    // ===== 顶栏绘制 ===== //
    lcd_draw_line(0, 31, 319-1, 31, 0x11ac);
    lcd_draw_string(105, 6, "电压表校准", &yahei20x20, 0x24be, 0x1908, 3);

    // ===== 进度提示绘制 ===== //
    lcd_draw_round_rect(18,47,108,73,8,0x048A,1);
    lcd_draw_string(28, 52, "原点校准", &yahei16x16, 0xFFFF, 0x048A, 2);
    lcd_draw_round_rect(115,47,205,73,8,0x632C,1);
    lcd_draw_string(125, 52, "正接校准", &yahei16x16, 0xDEFB, 0x632C, 2);
    lcd_draw_round_rect(212,47,302,73,8,0x632C,1);
    lcd_draw_string(222, 52, "反接校准", &yahei16x16, 0xDEFB, 0x632C, 2);

    // ===== 显示区域绘制 ===== //
    lcd_draw_round_rect(15,82,305,146,10,0x0000,1);
    lcd_draw_round_rect(15,82,305,146,10,0x632C,0);
    lcd_draw_string(41,90,"+00.00", &DIN_Medium32x48,0xeb0c,0x0000,2);
    lcd_draw_string(234,90,"0", &DIN_Medium32x48,0x632c,0x0000,2);

    // ===== 提示框绘制 ===== //
    lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
    lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
    lcd_draw_string(25,165,"请短接表笔，使数值接近0。", &yahei16x16,0xdf3c,0x29a7,1);

    // ===== 调节区绘制 ===== //
    lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
    lcd_draw_string(25,208,"输出原点:", &yahei16x16,0xFFFF,0x18c6,1);
    snprintf(Dmm_V_Original_Msg, sizeof(Dmm_V_Original_Msg), "%+03" PRId16, UserParam.DMM_Voltage_Original);
    lcd_draw_string(142,207,Dmm_V_Original_Msg, &yahei18x18,0xFFFF,0x18c6,2);

    // ===== 下一步按钮绘制 ===== //
    lcd_draw_round_rect(220,200,305,231,8,0x632C,1);
    lcd_draw_string(235,207,"下一步", &yahei18x18,0xFFFF,0x632C,1);
}

// 刷新电压校准页面显示
void DMM_V_Calibrate_Refresh(void)
{
    FormatFloatWithThousandth(DMM_Voltage, DMM_V_Vol_formatted_buf, DMM_V_Vol_thousandth_buf);
    lcd_draw_string(41,90,DMM_V_Vol_formatted_buf, &DIN_Medium32x48,0xeb0c,0x0000,2);
    lcd_draw_string(234,90,DMM_V_Vol_thousandth_buf, &DIN_Medium32x48,0x632c,0x0000,2);
}

// 电压校准页面枚举
typedef enum
{
    DmmVCal_Origin_Main = 0,
    DmmVCal_Origin_Edit,
    DmmVCal_Positive_Main,
    DmmVCal_Positive_Edit,
    DmmVCal_Negative_Main,
    DmmVCal_Negative_Edit,
    DmmVCal_Page_Num
} DmmVCalPage_e;

// 当前校准页面
static DmmVCalPage_e DmmVCal_Page_Current = DmmVCal_Origin_Main;

// 页面处理函数声明
static void DmmVCal_Origin_Main_handler(KeyEventMsg_t msg);
static void DmmVCal_Origin_Edit_handler(KeyEventMsg_t msg);
static void DmmVCal_Positive_Main_handler(KeyEventMsg_t msg);
static void DmmVCal_Positive_Edit_handler(KeyEventMsg_t msg);
static void DmmVCal_Negative_Main_handler(KeyEventMsg_t msg);
static void DmmVCal_Negative_Edit_handler(KeyEventMsg_t msg);

// 页面处理函数表
static void (*DmmVCalPage_handlers[DmmVCal_Page_Num])(KeyEventMsg_t) =
{
    [DmmVCal_Origin_Main] = DmmVCal_Origin_Main_handler,
    [DmmVCal_Origin_Edit] = DmmVCal_Origin_Edit_handler,
    [DmmVCal_Positive_Main] = DmmVCal_Positive_Main_handler,
    [DmmVCal_Positive_Edit] = DmmVCal_Positive_Edit_handler,
    [DmmVCal_Negative_Main] = DmmVCal_Negative_Main_handler,
    [DmmVCal_Negative_Edit] = DmmVCal_Negative_Edit_handler,
};

// 电压校准总处理函数
void dmm_v_cal_handler(KeyEventMsg_t msg)
{
    DmmVCalPage_handlers[DmmVCal_Page_Current](msg);
}

// 原点校准主页面 - 焦点选择
static uint8_t DmmVCal_Origin_Page_current = 0;
static void DmmVCal_Origin_Main_handler(KeyEventMsg_t msg)
{
    // 下键按下 & 编码器右转
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT)
    {
        DmmVCal_Origin_Page_current = (DmmVCal_Origin_Page_current < 1) ? (DmmVCal_Origin_Page_current + 1) : 0;
    }
    // 上键按下 & 编码器左转
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT)
    {
        DmmVCal_Origin_Page_current = (DmmVCal_Origin_Page_current > 0) ? (DmmVCal_Origin_Page_current - 1) : 1;
    }
    // 确认键按下 & 编码器按下
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK)
    {
        if (DmmVCal_Origin_Page_current == 0)   // 选中参数编辑框
        {
            DmmVCal_Page_Current = DmmVCal_Origin_Edit;               // 切换到原点编辑页面
            lcd_draw_line(162,225,191,225,0x87d3);                    // 绘制编辑下划线示意条
        }
        else if (DmmVCal_Origin_Page_current == 1)  // 选中下一步按钮
        {
            DmmVCal_Page_Current = DmmVCal_Positive_Main;             // 切换到正接校准主页面

            // 更新提示框文字
            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"测已知正电压，使显示值等于实际值", &yahei16x16,0xdf3c,0x29a7,1);

            // 更新编辑框提示文字
            lcd_draw_round_rect(15,200,212,231,8,0x18c6,1);
            lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
            lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
            lcd_draw_string(25,208,"正接系数:", &yahei16x16,0xFFFF,0x18c6,1);

            // 更新编辑框数据
            snprintf(Dmm_V_Positive_Msg, sizeof(Dmm_V_Positive_Msg), "%05.2f", UserParam.DMM_Voltage_Factor_R);
            lcd_draw_string(110,207,Dmm_V_Positive_Msg, &yahei18x18,0xFFFF,0x18c6,0);

            // 更新进度框
            lcd_draw_round_rect(18,47,108,73,8,0x632C,1);
            lcd_draw_string(28, 52, "原点校准", &yahei16x16, 0xFFFF, 0x632C, 2);
            lcd_draw_round_rect(115,47,205,73,8,0x048A,1);
            lcd_draw_string(125, 52, "正接校准", &yahei16x16, 0xDEFB, 0x048A, 2);

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
    if (DmmVCal_Origin_Page_current == 0)
    {
        lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
        lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
    }
    else
    {
        lcd_draw_round_rect(15,200,212,231,8,0x632C,0);
        lcd_draw_round_rect(220,200,305,231,8,0x07E0,0);
    }
}

// 原点校准编辑页面处理
static void DmmVCal_Origin_Edit_handler(KeyEventMsg_t msg)
{
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT)
    {
        UserParam.DMM_Voltage_Original += 1;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT)
    {
        UserParam.DMM_Voltage_Original -= 1;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER)
    {
        DmmVCal_Page_Current = DmmVCal_Origin_Main;
        lcd_draw_line(162,225,191,225,0x18c6);
    }
    else
    {
        return;
    }

    StartBeezer(0);
    snprintf(Dmm_V_Original_Msg, sizeof(Dmm_V_Original_Msg), "%+03" PRId16, UserParam.DMM_Voltage_Original);
    lcd_draw_string(142,207,Dmm_V_Original_Msg, &yahei18x18,0xFFFF,0x18c6,2);
}

// 正接校准主页面 - 焦点选择
static uint8_t DmmVCal_Positive_Page_current = 0;
static void DmmVCal_Positive_Main_handler(KeyEventMsg_t msg)
{
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT)
    {
        DmmVCal_Positive_Page_current = (DmmVCal_Positive_Page_current < 1) ? (DmmVCal_Positive_Page_current + 1) : 0;
    }
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT)
    {
        DmmVCal_Positive_Page_current = (DmmVCal_Positive_Page_current > 0) ? (DmmVCal_Positive_Page_current - 1) : 1;
    }
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK)
    {
        if (DmmVCal_Positive_Page_current == 0)
        {
            DmmVCal_Page_Current = DmmVCal_Positive_Edit;
            lcd_draw_line(110,225,186,225,0x87d3);
        }
        else if (DmmVCal_Positive_Page_current == 1)
        {
            DmmVCal_Page_Current = DmmVCal_Negative_Main;

            // 更新提示框文字
            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"测已知负电压，使显示值等于实际值", &yahei16x16,0xdf3c,0x29a7,1);

            // 更新编辑框提示文字
            lcd_draw_round_rect(15,200,212,231,8,0x18c6,1);
            lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
            lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
            lcd_draw_string(25,208,"反接系数:", &yahei16x16,0xFFFF,0x18c6,1);

            // 更新编辑框数据
            snprintf(Dmm_V_Positive_Msg, sizeof(Dmm_V_Positive_Msg), "%05.2f", UserParam.DMM_Voltage_Factor_B);
            lcd_draw_string(110,207,Dmm_V_Positive_Msg, &yahei18x18,0xFFFF,0x18c6,0);

            // 更新进度框
            lcd_draw_round_rect(115,47,205,73,8,0x632C,1);
            lcd_draw_string(125, 52, "正接校准", &yahei16x16, 0xDEFB, 0x632C, 2);
            lcd_draw_round_rect(212,47,302,73,8,0x048A,1);
            lcd_draw_string(222, 52, "反接校准", &yahei16x16, 0xDEFB, 0x048A, 2);

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
    if (DmmVCal_Positive_Page_current == 0)
    {
        lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
        lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
    }
    else
    {
        lcd_draw_round_rect(15,200,212,231,8,0x632C,0);
        lcd_draw_round_rect(220,200,305,231,8,0x07E0,0);
    }
}

// 正接校准编辑页面处理
static void DmmVCal_Positive_Edit_handler(KeyEventMsg_t msg)
{
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT)
    {
        UserParam.DMM_Voltage_Factor_R += 0.01f;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT)
    {
        UserParam.DMM_Voltage_Factor_R -= 0.01f;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER)
    {
        DmmVCal_Page_Current = DmmVCal_Positive_Main;
        lcd_draw_line(110,225,186,225,0x18c6);
    }
    else
    {
        return;
    }

    StartBeezer(0);
    snprintf(Dmm_V_Positive_Msg, sizeof(Dmm_V_Positive_Msg), "%05.2f", UserParam.DMM_Voltage_Factor_R);
    lcd_draw_string(110,207,Dmm_V_Positive_Msg, &yahei18x18,0xFFFF,0x18c6,0);
}

// 反接校准主页面 - 焦点选择
static uint8_t DmmVCal_Negative_Page_current = 0;
static void DmmVCal_Negative_Main_handler(KeyEventMsg_t msg)
{
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT)
    {
        DmmVCal_Negative_Page_current = (DmmVCal_Negative_Page_current < 1) ? (DmmVCal_Negative_Page_current + 1) : 0;
    }
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT)
    {
        DmmVCal_Negative_Page_current = (DmmVCal_Negative_Page_current > 0) ? (DmmVCal_Negative_Page_current - 1) : 1;
    }
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK)
    {
        if (DmmVCal_Negative_Page_current == 0)
        {
            DmmVCal_Page_Current = DmmVCal_Negative_Edit;
            lcd_draw_line(110,225,186,225,0x87d3);
        }
        else if (DmmVCal_Negative_Page_current == 1)
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
    if (DmmVCal_Negative_Page_current == 0)
    {
        lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
        lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
    }
    else
    {
        lcd_draw_round_rect(15,200,212,231,8,0x632C,0);
        lcd_draw_round_rect(220,200,305,231,8,0x07E0,0);
    }
}

// 反接校准编辑页面处理
static void DmmVCal_Negative_Edit_handler(KeyEventMsg_t msg)
{
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT)
    {
        UserParam.DMM_Voltage_Factor_B += 0.01f;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT)
    {
        UserParam.DMM_Voltage_Factor_B -= 0.01f;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER)
    {
        DmmVCal_Page_Current = DmmVCal_Negative_Main;
        lcd_draw_line(110,225,186,225,0x18c6);
    }
    else
    {
        return;
    }

    StartBeezer(0);
    snprintf(Dmm_V_Negative_Msg, sizeof(Dmm_V_Negative_Msg), "%05.2f", UserParam.DMM_Voltage_Factor_B);
    lcd_draw_string(110,207,Dmm_V_Negative_Msg, &yahei18x18,0xFFFF,0x18c6,0);
}

// 进入电压校准流程
void DMM_V_Calibrate_Enter(void)
{
    DrawDmmVCalibratePage();

    DMM_ADC_Init();                          // 初始化ADC
    osDelay(100);                            // 延时确保界面绘制完成
    // 注册ADC转换完成回调
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_COMPLETE_CB_ID, CB_DMM_ADC_ConvCpltCallback);
    AdcTim_OFF();                            // 先关闭ADC触发

    // 启动ADC DMA采集（600个数据）
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *) dmm_adc_raw_buf, 600);
    // 配置定时器8：20Khz触发（预分频0，自动重装7199）
    __HAL_TIM_SET_PRESCALER(&htim8, 0);
    __HAL_TIM_SET_AUTORELOAD(&htim8, 7200 - 1);
    AdcTim_ON();                             // 开启ADC触发

    DMM_Switch_Mode(Dmm_Mode_Voltage);       // 默认电压模式
}