#include "DSO_Calibrate.h"

#include <inttypes.h>
#include <stdio.h>

#include "lcd_draw_api.h"
#include "main.h"
#include "SwitchManager.h"
#include "tim.h"
#include "UserDefineManage.h"
#include "wave_generator.h"


void DSO_ADC_Init(void);
static void DrawDsoCalibratePage(void);
static void CB_DSO_CAL_ADC_ConvCplt(ADC_HandleTypeDef *hadc);

float DSO_CAL_VOLTAGE = 0.0f;

volatile uint8_t DSO_AVG_ADC_VAL = 0;

static uint8_t DSO_CAL_ADC_BUFFER[128] = {0};
static char Current_Voltage_str[10] = {0};         // 当前电压值

bool Is_Busy = false;


typedef enum {
    DsoCal_Origin_Main = 0,
    DsoCal_Divider_Main,
    DsoCal_Magnify_Main,
    DsoCal_Page_Num
} DsoCalPage_e;

static DsoCalPage_e DsoCal_Page_Current = DsoCal_Origin_Main;   // 当前的页面

typedef enum {
    Amp_X1 = 0,
    Amp_X2,
    Amp_X4,
    Amp_X8,
    Amp_X16,
    Amp_X32,
    Amp_X64,
}AmpType_e;

static AmpType_e DsoCal_Amp_Current = Amp_X1;


static void DsoCal_Origin_Main_handler(KeyEventMsg_t msg);
static void DsoCal_Divider_Main_handler(KeyEventMsg_t msg);
static void DsoCal_Magnify_Main_handler(KeyEventMsg_t msg);

static void (*DsoCalPage_handlers[DsoCal_Page_Num])(KeyEventMsg_t) = {
    [DsoCal_Origin_Main] = DsoCal_Origin_Main_handler,
    [DsoCal_Divider_Main] = DsoCal_Divider_Main_handler,
    [DsoCal_Magnify_Main] = DsoCal_Magnify_Main_handler,

};



void DSO_Calibrate_Refresh(void) {
    snprintf(Current_Voltage_str, sizeof(Current_Voltage_str), "%+06.3f", DSO_CAL_VOLTAGE);
    lcd_draw_string(58,90,Current_Voltage_str, &DIN_Medium32x48,0xeb0c,0x0000,2);
}


void dso_cal_handler(KeyEventMsg_t msg) {
    if (Is_Busy) {
        return;
    }
    DsoCalPage_handlers[DsoCal_Page_Current](msg);
}

void DSO_Calibrate_Enter(void) {
    DrawDsoCalibratePage();
    
    OSC_Couple_Ctrl(Couple_DC);
    OSC_Attenuation_Ctrl(Attenuation_1);
    OSC_Magnify_Ctrl(Magnify_1);

    DSO_ADC_Init();
    HAL_ADC_RegisterCallback(&hadc1, HAL_ADC_CONVERSION_COMPLETE_CB_ID, CB_DSO_CAL_ADC_ConvCplt);   // 注册DMA回调
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)DSO_CAL_ADC_BUFFER, 128);                      // 启动ADC & DMA
    AdcTim_OFF();
    __HAL_TIM_SET_PRESCALER(&htim8, 149);   // 设定更新频率64Khz
    __HAL_TIM_SET_AUTORELOAD(&htim8, 14);
    AdcTim_ON();                            // 启动定时器（ADC触发源）
}


static void DrawDsoCalibratePage(void) {
    // ===== 底色绘制 ===== //
    lcd_draw_rect(0, 0, 319, 31, 0x1908, 1);
    lcd_draw_rect(0, 32, 319, 239, 0x18c6, 1);
    // ===== 顶栏绘制 ===== //
    lcd_draw_line(0, 31, 319-1, 31, 0x11ac);
    lcd_draw_string(105, 6, "示波器校准", &yahei20x20, 0x24be, 0x1908, 3);
    // ===== 进度提示绘制 ===== //
    lcd_draw_round_rect(18,47,108,73,8,0x048A,1);
    lcd_draw_string(28, 52, "原点校准", &yahei16x16, 0xFFFF, 0x048A, 2);
    lcd_draw_round_rect(115,47,205,73,8,0x632C,1);
    lcd_draw_string(125, 52, "分压校准", &yahei16x16, 0xDEFB, 0x632C, 2);
    lcd_draw_round_rect(212,47,302,73,8,0x632C,1);
    lcd_draw_string(222, 52, "放大校准", &yahei16x16, 0xDEFB, 0x632C, 2);
    // ===== 电压显示绘制 ===== //
    lcd_draw_round_rect(15,82,305,146,10,0x0000,1);
    lcd_draw_round_rect(15,82,305,146,10,0x632C,0);
    lcd_draw_string(58,90,"+0.000", &DIN_Medium32x48,0xeb0c,0x0000,2);
    lcd_draw_string(256,118,"V", &KaiTi16x20,0xeb0c,0x0000,0);
    //  ===== 提示框绘制 ===== //
    lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
    lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
    lcd_draw_string(25,165,"请移除表笔，按下按键自动校准", &yahei16x16,0xdf3c,0x29a7,1);
    // ====== 调节区绘制 ===== //
    lcd_draw_round_rect(15,200,212,231,8,0x632C,0);
    // ===== 下一步按钮绘制 ===== //
    lcd_draw_round_rect(220,200,305,231,8,0x632C,1);
    lcd_draw_round_rect(220,200,305,231,8,0x07E0,0);
    lcd_draw_string(235,207,"下一步", &yahei18x18,0xFFFF,0x632C,1);

}


typedef struct {
    MagnifyTypeDef magnify_val;     // 挡位枚举 (Magnify_1/Magnify_2...)
    AmpType_e amp_val;             // 电流挡位枚举 (Amp_X1/Amp_X2...)
    int16_t *original_param;   // 指向对应校准参数的指针
    const char *step_name;     // 挡位名称字符串 ("X1"/"X2"...)
} CalStep_t;

static const CalStep_t cal_steps[] = {
    {Magnify_1, Amp_X1, &UserParam.OSC_Original,    "X1"},
    {Magnify_2, Amp_X2, &UserParam.OSC_Original_X2, "X2"},
    {Magnify_4, Amp_X4, &UserParam.OSC_Original_X4, "X4"},
    {Magnify_8, Amp_X8, &UserParam.OSC_Original_X8, "X8"},
    {Magnify_16, Amp_X16, &UserParam.OSC_Original_X16, "X16"},
    {Magnify_32, Amp_X32, &UserParam.OSC_Original_X32, "X32"},
};
#define CAL_STEP_NUM (sizeof(cal_steps) / sizeof(cal_steps[0]))

static void CalibrateSingleStep(const CalStep_t *step) {
    char buf[64];
    uint8_t stable_cnt = 0;

    OSC_Magnify_Ctrl(step->magnify_val);
    DsoCal_Amp_Current = step->amp_val;

    lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
    lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
    snprintf(buf, sizeof(buf), "开始校准！校准%s挡", step->step_name);
    lcd_draw_string(25,165, buf, &yahei16x16,0xdf3c,0x29a7,1);

    while(1) {
        if (DSO_AVG_ADC_VAL == 128) {
            if (++stable_cnt >= 10) break; // 10 * 100ms = 1s
        } else {
            stable_cnt = 0;
            if (DSO_AVG_ADC_VAL > 128) *(step->original_param) -= 1;
            else if (DSO_AVG_ADC_VAL < 128) *(step->original_param) += 1;
        }

        snprintf(Current_Voltage_str, sizeof(Current_Voltage_str), "%+06.3f", DSO_CAL_VOLTAGE);
        lcd_draw_string(58,90,Current_Voltage_str, &DIN_Medium32x48,0xeb0c,0x0000,2);
        osDelay(100);
    }

    lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
    lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
    snprintf(buf, sizeof(buf), "%s档校准完成！", step->step_name);
    lcd_draw_string(25,165, buf, &yahei16x16,0xdf3c,0x29a7,1);
    osDelay(1000);
}


bool Is_Origin_Cal = false;
static void DsoCal_Origin_Main_handler(KeyEventMsg_t msg) {
    if ((msg.key == KEY_ENCODER || msg.key == KEY_SET) && msg.event == KEY_EVENT_CLICK) {
        if (!Is_Origin_Cal) {
            Is_Origin_Cal = true;
            Is_Busy = true;
            // 循环执行所有校准步骤
            for (int i = 0; i < CAL_STEP_NUM; i++) {
                CalibrateSingleStep(&cal_steps[i]);
            }

            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"校准完成！", &yahei16x16,0xdf3c,0x29a7,1);

            Is_Busy = false;
        }

        if (Is_Origin_Cal) {


            DsoCal_Page_Current = DsoCal_Divider_Main;      // 切换到Divider界面

            lcd_draw_round_rect(18,47,108,73,8,0x632C,1);
            lcd_draw_string(28, 52, "原点校准", &yahei16x16, 0xFFFF, 0x632C, 2);
            lcd_draw_round_rect(115,47,205,73,8,0x048A,1);
            lcd_draw_string(125, 52, "分压校准", &yahei16x16, 0xDEFB, 0x048A, 2);

            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"请短接示波器输入与信号源输出", &yahei16x16,0xdf3c,0x29a7,1);

            OSC_Magnify_Ctrl(Magnify_1);                          // 切换到X1挡位
            DsoCal_Amp_Current = Amp_X1;
            OSC_Attenuation_Ctrl(Attenuation_1);                  // 1倍衰减
        }
    }
    else return;

}


bool Is_Divider_Cal = false;
static void DsoCal_Divider_Main_handler(KeyEventMsg_t msg) {
    if ((msg.key == KEY_ENCODER || msg.key == KEY_SET) && msg.event == KEY_EVENT_CLICK) {
        if (!Is_Divider_Cal) {
            Is_Divider_Cal = true;
            Is_Busy = true;

            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"开始校准分压！", &yahei16x16,0xdf3c,0x29a7,1);
            WaveGen_DC(1000,0,1.5f,0);  // 生成1.5V的
            osDelay(1000);

            snprintf(Current_Voltage_str, sizeof(Current_Voltage_str), "%+06.3f", DSO_CAL_VOLTAGE);
            lcd_draw_string(58,90,Current_Voltage_str, &DIN_Medium32x48,0xeb0c,0x0000,2);

            float voltage = DSO_CAL_VOLTAGE - 1.65f;    // 记录x1时的电压
            osDelay(1000);
            OSC_Attenuation_Ctrl(Attenuation_5);                  // 5倍衰减

            osDelay(1000);


            float voltage_div5 = DSO_CAL_VOLTAGE - 1.65f;    // 记录5倍衰减时的电压
            float real_div_ratio =  voltage/ voltage_div5   ;
            UserParam.OSC_Factor = real_div_ratio;

            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"分压校准完成！单击进入下一项", &yahei16x16,0xdf3c,0x29a7,1);

            Is_Busy = false;
            return;
        }
        if (Is_Divider_Cal) {

            DsoCal_Page_Current = DsoCal_Magnify_Main;      // 切换到Magnify界面

            lcd_draw_round_rect(115,47,205,73,8,0x632C,1);
            lcd_draw_string(125, 52, "分压校准", &yahei16x16, 0xDEFB, 0x632C, 2);
            lcd_draw_round_rect(212,47,302,73,8,0x048A,1);
            lcd_draw_string(222, 52, "放大校准", &yahei16x16, 0xDEFB, 0x048A, 2);

            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"请短接示波器输入与信号源输出", &yahei16x16,0xdf3c,0x29a7,1);

            OSC_Magnify_Ctrl(Magnify_1);                          // 切换到X1挡位
            DsoCal_Amp_Current = Amp_X1;
            OSC_Attenuation_Ctrl(Attenuation_1);                  // 1倍衰减

        }

    }
}


typedef struct {
    MagnifyTypeDef magnify_val;     // 挡位枚举
    AmpType_e amp_val;             // 电流挡位枚举
    float dac_voltage;         // DAC输出电压 (V)
    float *gain_param;         // 指向对应放大倍数参数的指针
    const char *step_name;     // 挡位名称字符串
} GainCalStep_t;

static const GainCalStep_t gain_cal_steps[] = {
    {Magnify_2,  Amp_X2,  3.94f, &UserParam.OSC_AMP_X2,  "X2"},
    {Magnify_4,  Amp_X4,  1.97f, &UserParam.OSC_AMP_X4,  "X4"},
    {Magnify_8,  Amp_X8,  0.98f, &UserParam.OSC_AMP_X8,  "X8"},
    {Magnify_16, Amp_X16, 0.49f, &UserParam.OSC_AMP_X16, "X16"},
    {Magnify_32, Amp_X32, 0.25f, &UserParam.OSC_AMP_X32, "X32"},
    {Magnify_64, Amp_X64, 0.12f, &UserParam.OSC_AMP_X64, "X64"},
};
#define GAIN_CAL_STEP_NUM (sizeof(gain_cal_steps) / sizeof(gain_cal_steps[0]))

static void SetDACVoltage(float voltage) {
    WaveGen_DC(1000, 0, voltage, 0);
}

static float CalculateActualGain(uint16_t adc_avg, float dac_voltage) {
    // ADC码值转电压
    float v_adc = (float)adc_avg * 3.3f / 255.0f;
    // 去除1.65V偏置
    float v_amp = v_adc - 1.65f;
    // 分压后的输入电压（固定5倍分压）
    float v_in = dac_voltage / 5.0f;
    // 计算实际放大倍数
    return v_amp / v_in;
}

static void CalibrateSingleGainStep(const GainCalStep_t *step) {
    char buf[64];

    // 1. 设置DAC输出
    SetDACVoltage(step->dac_voltage);

    // 2. 切换放大挡位
    OSC_Magnify_Ctrl(step->magnify_val);
    DsoCal_Amp_Current = step->amp_val;

    // 3. 显示「开始校准」
    lcd_draw_round_rect(15, 158, 305, 188, 8, 0x29a7, 1);
    lcd_draw_round_rect(15, 158, 305, 188, 8, 0x632C, 0);
    snprintf(buf, sizeof(buf), "开始校准！%s挡", step->step_name);
    lcd_draw_string(25, 165, buf, &yahei16x16, 0xdf3c, 0x29a7, 1);

    // 4. 等待信号稳定
    osDelay(100);

    // 6. 计算实际放大倍数
    float actual_gain = CalculateActualGain(DSO_AVG_ADC_VAL, step->dac_voltage);

    // 7. 保存到参数区
    *(step->gain_param) = actual_gain;

    // 8. 显示电压和放大倍数
    snprintf(Current_Voltage_str, sizeof(Current_Voltage_str), "%+06.3f", (float)DSO_AVG_ADC_VAL * 3.3f / 255.0f);
    lcd_draw_string(58, 90, Current_Voltage_str, &DIN_Medium32x48, 0xeb0c, 0x0000, 2);

    // 9. 显示「校准完成」
    lcd_draw_round_rect(15, 158, 305, 188, 8, 0x29a7, 1);
    lcd_draw_round_rect(15, 158, 305, 188, 8, 0x632C, 0);
    snprintf(buf, sizeof(buf), "%s档校准完成！", step->step_name);
    lcd_draw_string(25, 165, buf, &yahei16x16, 0xdf3c, 0x29a7, 1);
    osDelay(1000);
}

bool Is_Magnify_Cal = false;
static void DsoCal_Magnify_Main_handler(KeyEventMsg_t msg) {
    if ((msg.key == KEY_ENCODER || msg.key == KEY_SET) && msg.event == KEY_EVENT_CLICK) {
        if (!Is_Magnify_Cal) {
            Is_Magnify_Cal = true;
            Is_Busy = true;

            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"开始校准放大！", &yahei16x16,0xdf3c,0x29a7,1);
            osDelay(1000);

            for (int i = 0; i < GAIN_CAL_STEP_NUM; i++) {
                CalibrateSingleGainStep(&gain_cal_steps[i]);
            }

            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"校准完成！", &yahei16x16,0xdf3c,0x29a7,1);

            Is_Busy = false;
            return;
        }
        if (Is_Magnify_Cal) {
            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"正在保存", &yahei16x16,0xdf3c,0x29a7,1);
            osDelay(500);

            UserParam_SaveAllValues();

            lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
            lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
            lcd_draw_string(25,165,"保存成功！", &yahei16x16,0xdf3c,0x29a7,1);
            osDelay(100);
            Calibrate_ReturnToMain();   // 返回主界面
        }

    }
}

























static void CB_DSO_CAL_ADC_ConvCplt(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance != ADC1) return;
    AdcTim_OFF();

    int16_t bias = 0;
    switch (DsoCal_Amp_Current) {
        case Amp_X1: bias = UserParam.OSC_Original;
            break;
        case Amp_X2: bias = UserParam.OSC_Original_X2;
            break;
        case Amp_X4: bias = UserParam.OSC_Original_X4;
            break;
        case Amp_X8: bias = UserParam.OSC_Original_X8;
            break;
        case Amp_X16: bias = UserParam.OSC_Original_X16;
            break;
        case Amp_X32: bias = UserParam.OSC_Original_X32;
            break;
        case Amp_X64: bias = UserParam.OSC_Original_X64;
            break;
        default: bias = 0;
    }

    uint32_t sum = 0;
    for (int i = 0; i < 128; i++) {
        sum += (DSO_CAL_ADC_BUFFER[i] + bias);
    }
    DSO_AVG_ADC_VAL = (sum >> 7);
    DSO_CAL_VOLTAGE = (float)(sum >> 7 ) * 3.3f / 255.0f;

    AdcTim_ON();
}

