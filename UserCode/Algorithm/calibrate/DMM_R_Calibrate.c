#include "DMM_R_Calibrate.h"
#include <inttypes.h>
#include <stdio.h>
#include "lcd_draw_api.h"
#include "SwitchManager.h"
#include "tim.h"
#include "UserDefineManage.h"


void DMM_ADC_Init(void);
void DMM_ADC_DeInit(void);
void CB_DMM_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);
int FormatFloatWithTenThousandth(float num, char *formatted_buf, char *tenthousandth_buf);
static float DMM_Calc_R0_SinglePoint(float Vref, float Rx_std, float Vc_std);

// 枚举定义 - 电阻测量档位
typedef enum {
    DMM_RES_MODE_200 = 0,   // 200Ω档位
    DMM_RES_MODE_2K,        // 2KΩ档位
} DMM_Res_Mode_t;

extern volatile uint16_t dmm_adc_raw_buf[600];
extern volatile float DMM_Resistance_Voltage;       // 电阻测量原始电压值
extern volatile DMM_Res_Mode_t dmm_res_mode;

static char Dmm_R_R200_Original_Msg[10] = {0};       // 电压原点提示信息
static char Dmm_R_R2k_Original_Msg[10] = {0};       // 电压原点提示信息

static char DMM_R_Vol_formatted_buf[8] = {0};       // 电压格式化缓存
static char DMM_R_Vol_thousandth_buf[2] = {0};      // 电压千分位缓存

    static float Vol_Temp1, Vol_Temp2;              // 电压临时变量



// 电阻校准页面枚举
typedef enum
{
    DmmRCal_R200_Origin_Main = 0,   // 200欧姆 原点校准 主界面
    DmmRCal_R200_Origin_Edit,       // 200欧姆 原点校准 编辑界面
    DmmRCal_R2k_Origin_Main,        // 2k欧姆 原点校准 主界面
    DmmRCal_R2k_Origin_Edit,        // 2k欧姆 原点校准 编辑界面
    DmmRCal_R200_Voltage_Main,      // 200欧姆 空载电压 主界面
    DmmRCal_R2K_Voltage_Main,       // 2k欧姆 空载电压 主界面
    DmmRCal_R200_Fitting_Main,      // 200欧姆 函数拟合 主界面
    DmmRCal_R2K_Fitting_Main,       // 2k欧姆 函数拟合 主界面
    DmmRCal_Page_Num
} DmmRCalPage_e;

static DmmRCalPage_e DmmRCal_Page_Current = DmmRCal_R200_Origin_Main;   // 当前的页面


static void DmmRCal_R200_Origin_Main_handler(KeyEventMsg_t msg);
static void DmmRCal_R200_Origin_Edit_handler(KeyEventMsg_t msg);
static void DmmRCal_R2k_Origin_Main_handler(KeyEventMsg_t msg);
static void DmmRCal_R2k_Origin_Edit_handler(KeyEventMsg_t msg);
static void DmmRCal_R200_Voltage_Main_handler(KeyEventMsg_t msg);
static void DmmRCal_R2K_Voltage_Main_handler(KeyEventMsg_t msg);
static void DmmRCal_R200_Fitting_Main_handler(KeyEventMsg_t msg);
static void DmmRCal_R2K_Fitting_Main_handler(KeyEventMsg_t msg);

// 页面处理函数表
static void (*DmmRCalPage_handlers[DmmRCal_Page_Num])(KeyEventMsg_t) =
{
    [DmmRCal_R200_Origin_Main]  =   DmmRCal_R200_Origin_Main_handler,
    [DmmRCal_R200_Origin_Edit]  =   DmmRCal_R200_Origin_Edit_handler,
    [DmmRCal_R2k_Origin_Main]   =   DmmRCal_R2k_Origin_Main_handler,
    [DmmRCal_R2k_Origin_Edit]   =   DmmRCal_R2k_Origin_Edit_handler,
    [DmmRCal_R200_Voltage_Main] =   DmmRCal_R200_Voltage_Main_handler,
    [DmmRCal_R2K_Voltage_Main]  =   DmmRCal_R2K_Voltage_Main_handler,
    [DmmRCal_R200_Fitting_Main] =   DmmRCal_R200_Fitting_Main_handler,
    [DmmRCal_R2K_Fitting_Main]  =   DmmRCal_R2K_Fitting_Main_handler,
};

void dmm_r_cal_handler(KeyEventMsg_t msg) {
    DmmRCalPage_handlers[DmmRCal_Page_Current](msg);
}


void DMM_R_Calibrate_Refresh(void) {
    FormatFloatWithTenThousandth(DMM_Resistance_Voltage, DMM_R_Vol_formatted_buf, DMM_R_Vol_thousandth_buf);
    lcd_draw_string(41, 90, DMM_R_Vol_formatted_buf, &DIN_Medium32x48, 0xeb0c, 0x0000, 2);
    lcd_draw_string(234, 90, DMM_R_Vol_thousandth_buf, &DIN_Medium32x48, 0x632c, 0x0000, 2);
}


// 绘制万用表电压档校准初始页面
static void DrawDmmRCalibratePage(void)
{
    // ===== 底色绘制 ===== //
    lcd_draw_rect(0, 0, 319, 31, 0x1908, 1);
    lcd_draw_rect(0, 32, 319, 239, 0x18c6, 1);

    // ===== 顶栏绘制 ===== //
    lcd_draw_line(0, 31, 319-1, 31, 0x11ac);
    lcd_draw_string(105, 6, "电阻表校准", &yahei20x20, 0x24be, 0x1908, 3);

    // ===== 进度提示绘制 ===== //
    lcd_draw_round_rect(18,47,108,73,8,0x048A,1);
    lcd_draw_string(28, 52, "原点校准", &yahei16x16, 0xFFFF, 0x048A, 2);
    lcd_draw_round_rect(115,47,205,73,8,0x632C,1);
    lcd_draw_string(125, 52, "电压校准", &yahei16x16, 0xDEFB, 0x632C, 2);
    lcd_draw_round_rect(212,47,302,73,8,0x632C,1);
    lcd_draw_string(222, 52, "测量校准", &yahei16x16, 0xDEFB, 0x632C, 2);

    // ===== 显示区域绘制 ===== //
    lcd_draw_round_rect(15,82,305,146,10,0x0000,1);
    lcd_draw_round_rect(15,82,305,146,10,0x632C,0);
    lcd_draw_string(41, 90, "+0.000", &DIN_Medium32x48, 0xeb0c, 0x0000, 2);
    lcd_draw_string(234, 90, "0", &DIN_Medium32x48, 0x632c, 0x0000, 2);

    // ===== 提示框绘制 ===== //
    lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
    lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
    lcd_draw_string(25,165,"请短接表笔，使数值接近0。", &yahei16x16,0xdf3c,0x29a7,1);

    // ===== 调节区绘制 ===== //
    lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
    lcd_draw_string(25,208,"电压原点:", &yahei16x16,0xFFFF,0x18c6,1);
    snprintf(Dmm_R_R200_Original_Msg, sizeof(Dmm_R_R200_Original_Msg), "%+03" PRId16, UserParam.DMM_Res_R200_Original);
    lcd_draw_string(142,207,Dmm_R_R200_Original_Msg, &yahei18x18,0xFFFF,0x18c6,2);

    // ===== 下一步按钮绘制 ===== //
    lcd_draw_round_rect(220,200,305,231,8,0x632C,1);
    lcd_draw_string(235,207,"下一步", &yahei18x18,0xFFFF,0x632C,1);
}


uint8_t DmmRCal_R200_Origin_Edit_Cursor = 0;
static void DmmRCal_R200_Origin_Main_handler(KeyEventMsg_t msg) {
        // 下键按下 & 编码器右转
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT)
    {
        DmmRCal_R200_Origin_Edit_Cursor = (DmmRCal_R200_Origin_Edit_Cursor < 1) ? (DmmRCal_R200_Origin_Edit_Cursor + 1) : 0;
    }
    // 上键按下 & 编码器左转
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT)
    {
        DmmRCal_R200_Origin_Edit_Cursor = (DmmRCal_R200_Origin_Edit_Cursor > 0) ? (DmmRCal_R200_Origin_Edit_Cursor - 1) : 1;
    }
    // 确认键按下 & 编码器按下
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK)
    {
        if (DmmRCal_R200_Origin_Edit_Cursor == 0)   // 选中参数编辑框
        {
            DmmRCal_Page_Current = DmmRCal_R200_Origin_Edit;                           // 切换到原点编辑页面
            lcd_draw_line(162,225,191,225,0x87d3);                    // 绘制编辑下划线示意条
        }
        else if (DmmRCal_R200_Origin_Edit_Cursor == 1) {
            DmmRCal_Page_Current = DmmRCal_R2k_Origin_Main;        // 切换到2K档原点校准

            snprintf(Dmm_R_R2k_Original_Msg, sizeof(Dmm_R_R2k_Original_Msg), "%+03" PRId16, UserParam.DMM_Res_R2K_Original);
            lcd_draw_string(142,207,Dmm_R_R2k_Original_Msg, &yahei18x18,0xFFFF,0x18c6,2);

            lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
            lcd_draw_round_rect(220,200,305,231,8,0x632C,0);

            // 硬件档位切换：关闭200Ω，打开2KΩ
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_RESET);

            dmm_res_mode = DMM_RES_MODE_2K;

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
    if (DmmRCal_R200_Origin_Edit_Cursor == 0)
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

static void DmmRCal_R200_Origin_Edit_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT)
    {
        UserParam.DMM_Res_R200_Original += 1;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT)
    {
        UserParam.DMM_Res_R200_Original -= 1;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER)
    {
        DmmRCal_Page_Current = DmmRCal_R200_Origin_Main;
        lcd_draw_line(162,225,191,225,0x18c6);
    }
    else
    {
        return;
    }

    StartBeezer(0);
    snprintf(Dmm_R_R200_Original_Msg, sizeof(Dmm_R_R200_Original_Msg), "%+03" PRId16, UserParam.DMM_Res_R200_Original);
    lcd_draw_string(142,207,Dmm_R_R200_Original_Msg, &yahei18x18,0xFFFF,0x18c6,2);
}



uint8_t DmmRCal_R2k_Origin_Edit_Cursor = 0;
static void DmmRCal_R2k_Origin_Main_handler(KeyEventMsg_t msg) {
    // 下键按下 & 编码器右转
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT)
    {
        DmmRCal_R2k_Origin_Edit_Cursor = (DmmRCal_R2k_Origin_Edit_Cursor < 1) ? (DmmRCal_R2k_Origin_Edit_Cursor + 1) : 0;
    }
    // 上键按下 & 编码器左转
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT) {
        DmmRCal_R2k_Origin_Edit_Cursor = (DmmRCal_R2k_Origin_Edit_Cursor > 0) ? (DmmRCal_R2k_Origin_Edit_Cursor - 1) : 1;
    }
    // 确认键按下 & 编码器按下
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (DmmRCal_R2k_Origin_Edit_Cursor == 0)   // 选中参数编辑框
        {
            DmmRCal_Page_Current = DmmRCal_R2k_Origin_Edit;                           // 切换到原点编辑页面
            lcd_draw_line(162,225,191,225,0x87d3);                    // 绘制编辑下划线示意条
        }
        else if (DmmRCal_R2k_Origin_Edit_Cursor == 1) {
            DmmRCal_Page_Current = DmmRCal_R200_Voltage_Main;        // 切换到200Ω档原点校准

            lcd_draw_round_rect(18,47,108,73,8,0x632C,1);
            lcd_draw_string(28, 52, "原点校准", &yahei16x16, 0xFFFF, 0x632C, 2);
            lcd_draw_round_rect(115,47,205,73,8,0x048A,1);
            lcd_draw_string(125, 52, "电压校准", &yahei16x16, 0xDEFB, 0x048A, 2);

            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"请移除负载，稳定后下一步。", &yahei16x16,0xdf3c,0x29a7,1);

            lcd_draw_round_rect(15,200,212,231,8,0x18c6,1);

            // 硬件档位切换：关闭2KΩ，打开200Ω
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_SET);

            dmm_res_mode = DMM_RES_MODE_200;

        }
    }
    else {return;}

    StartBeezer(0);

    // 更新选择焦点框
    if (DmmRCal_R2k_Origin_Edit_Cursor == 0)
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

static void DmmRCal_R2k_Origin_Edit_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT) {
        UserParam.DMM_Res_R2K_Original += 1;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DMM_Res_R2K_Original -= 1;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        DmmRCal_Page_Current = DmmRCal_R2k_Origin_Main;
        lcd_draw_line(162,225,191,225,0x18c6);

    }
    else {return;}
    StartBeezer(0);
    snprintf(Dmm_R_R2k_Original_Msg, sizeof(Dmm_R_R2k_Original_Msg), "%+03" PRId16, UserParam.DMM_Res_R2K_Original);
    lcd_draw_string(142,207,Dmm_R_R2k_Original_Msg, &yahei18x18,0xFFFF,0x18c6,2);

}




static void DmmRCal_R200_Voltage_Main_handler(KeyEventMsg_t msg) {
    // 如果下键按下
    if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        StartBeezer(0);
        // 保存当前值
        UserParam.DMM_Res_R200_Voltage = DMM_Resistance_Voltage;
        // 切换到下一个界面
        DmmRCal_Page_Current = DmmRCal_R2K_Voltage_Main;

        // 硬件档位切换：关闭200Ω，打开2KΩ
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_RESET);

        dmm_res_mode = DMM_RES_MODE_2K;

    }
    else return;
}

static void DmmRCal_R2K_Voltage_Main_handler(KeyEventMsg_t msg){
    if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        StartBeezer(0);
        // 保存当前值
        UserParam.DMM_Res_R2K_Voltage = DMM_Resistance_Voltage;
        // 切换到下一个界面
        DmmRCal_Page_Current = DmmRCal_R200_Fitting_Main;

        lcd_draw_round_rect(115,47,205,73,8,0x632C,1);
        lcd_draw_string(125, 52, "电压校准", &yahei16x16, 0xDEFB, 0x632C, 2);
        lcd_draw_round_rect(212,47,302,73,8,0x048A,1);
        lcd_draw_string(222, 52, "测量校准", &yahei16x16, 0xDEFB, 0x048A, 2);

        lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
        lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
        lcd_draw_string(25,165,"接200Ω 0.1%，稳定后下一步。", &yahei16x16,0xdf3c,0x29a7,0);

        // 硬件档位切换：关闭2KΩ，打开200Ω
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_SET);

        dmm_res_mode = DMM_RES_MODE_200;
    }
}

static void DmmRCal_R200_Fitting_Main_handler(KeyEventMsg_t msg) {
    if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        StartBeezer(0);
        // 保存当前值
        Vol_Temp1 = DMM_Resistance_Voltage;

        // 计算并保存值
        UserParam.DMM_Res_R200 = DMM_Calc_R0_SinglePoint(UserParam.DMM_Res_R200_Voltage, 200.0f, Vol_Temp1);

        // 切换到下一个界面
        DmmRCal_Page_Current = DmmRCal_R2K_Fitting_Main;

        lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
        lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
        lcd_draw_string(25,165,"接2KΩ 0.1%，稳定后下一步。", &yahei16x16,0xdf3c,0x29a7,0);

        // 硬件档位切换：关闭200Ω，打开2KΩ
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_RESET);

        dmm_res_mode = DMM_RES_MODE_2K;
    }
}

static void DmmRCal_R2K_Fitting_Main_handler(KeyEventMsg_t msg) {
    if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        StartBeezer(0);
        // 保存当前值
        Vol_Temp2 = DMM_Resistance_Voltage;

        // 计算并保存值
        UserParam.DMM_Res_R2K = DMM_Calc_R0_SinglePoint(UserParam.DMM_Res_R2K_Voltage, 2000.0f, Vol_Temp2);

        // 将数据存入flash
        UserParam_SaveAllValues();

        // 退出校准

        DMM_Switch_Mode(Dmm_Mode_Voltage);                   // 切换到电压模式
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET); // 关闭电压档硬件
        DMM_ADC_DeInit(); // 反初始化ADC
        osDelay(100);
        Calibrate_ReturnToMain();   // 返回主界面
        dmm_res_mode = DMM_RES_MODE_200;
    }
}



static float DMM_Calc_R0_SinglePoint(float Vref, float Rx_std, float Vc_std) {
    float R0 = Rx_std * (Vref - Vc_std) / Vc_std;
    return R0;
}







void DMM_R_Calibrate_Enter(void) {
    DrawDmmRCalibratePage();

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

    DMM_Switch_Mode(Dmm_Mode_Resistance);       // 默认电阻模式200Ω挡
}