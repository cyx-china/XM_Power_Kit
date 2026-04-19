#ifndef XM_POWER_KIT_USERDEFINEMANAGE_H
#define XM_POWER_KIT_USERDEFINEMANAGE_H
#include <stdint.h>
#include "stm32f4xx_hal.h"

// 可自定义量枚举
typedef enum {
    //============= 数控电源APP ============== //
    //== 校准参数 ==//
    // 手动校准：
    DPS_Voltage_Original        = 0,    // 电压零点         (int16_t)   LSB         用于归零无输出时的显示
    DPS_Voltage_Factor,                 // 电压系数         (float)     Factor      用于确定分压比
    // 自动校准：
    DPS_Voltage_DAC_Coefficient,        // DAC系数         (float)     Coefficient  DAC值与电压函数的系数
    DPS_Voltage_DAC_Constant,           // DAC常数项        (float)     Constant     DAC值与电压函数的常数项
    // 手动校准
    DPS_Current_original,               // 电流零点         (int16_t)   LSB         用于归零无输出时的显示
    DPS_Current_Factor,                 // 电流系数         (float)     Factor      用于确定I = U * K中K的值;K = 1 / 电阻R / TP181放大倍率
    // PID参数
    DPS_Loop_P,                         // 电流环P值        (float)     P
    DPS_Loop_I,                         // 电流环I值        (float)     I
    DPS_Loop_D,                         // 电流环D值        (float)     D
    // 快速设置
    DPS_Fs_Voltage_1,                   // 第一组电压值      (float)     Voltage_1
    DPS_Fs_Current_1,                   // 第一组电流值      (float)     Current_1
    DPS_Fs_Voltage_2,                   // 第二组电压值      (float)     Voltage_2
    DPS_Fs_Current_2,                   // 第二组电流值      (float)     Current_2
    DPS_Fs_Voltage_3,                   // 第三组电压值      (float)     Voltage_3
    DPS_Fs_Current_3,                   // 第三组电流值      (float)     Current_3
    DPS_Fs_Voltage_4,                   // 第四组电压值      (float)     Voltage_4
    DPS_Fs_Current_4,                   // 第四组电流值      (float)     Current_4
    DPS_Fs_Voltage_5,                   // 第五组电压值      (float)     Voltage_5
    DPS_Fs_Current_5,                   // 第五组电流值      (float)     Current_5
    // 杂项
    DPS_Fan_Start,                      // 风扇启转温度      (float)     Temp
    //============= 示波器APP ============== //
    // 校准参数
    OSC_Original,                       // 示波器X1档电压零点 (int16_t)   LSB
    OSC_Original_X2,                    // 
    OSC_Original_X4,                    // 
    OSC_Original_X8,                    // 
    OSC_Original_X16,                   // 
    OSC_Original_X32,                   // 
    OSC_Original_X64,                   // 
    OSC_Original_X128,                  // 
    OSC_Factor,                         // 示波器输入分压比   (float)     Factor
    OSC_AMP_X2,                         // 程控放大器_X2     (float)     Amp
    OSC_AMP_X4,                         // 程控放大器_X4     (float)     Amp
    OSC_AMP_X8,                         // 程控放大器_X8     (float)     Amp
    OSC_AMP_X16,                        // 程控放大器_X16    (float)     Amp
    OSC_AMP_X32,                        // 程控放大器_X32    (float)     Amp
    OSC_AMP_X64,                        // 程控放大器_X64    (float)     Amp
    OSC_AMP_X128,                       // 程控放大器_X128   (float)     Amp
    //============= 信号发生器APP ============== //
    // 校准参数
    DDS_Original,                       // 信号发生器输出零点  (int16_t)   LSB
    DDS_Factor,                         // 信号发生器倍率     (float)     Factor
    //============== 万用表APP =============== //
    // 电压表校准参数
    DMM_Voltage_Original,               // 电压表零点         (int16_t)   LSB
    DMM_Voltage_Factor_B,               // 电压表比例系数(黑)  (float)     Factor
    DMM_Voltage_Factor_R,               // 电压表比例系数(红)  (float)     Factor
    // 电流表校准参数
    DMM_Current_original,               // 电流零点           (int16_t)   LSB
    DMM_Current_Factor,                 // 电流系数           (float)     Factor
    // 电阻表校准参数
    DMM_Res_R200_Original,              // 200Ω输出零点       (int16_t)   LSB
    DMM_Res_R200_Voltage,               // 200Ω空载电压       (float)     V
    DMM_Res_R200,                       // 200Ω基准阻值       (float)     R_Ω
    DMM_Res_R2K_Original,               // 2KΩ输出零点        (int16_t)   LSB
    DMM_Res_R2K_Voltage,                // 2KΩ空载电压        (float)     V
    DMM_Res_R2K,                        // 2KΩ基准阻值        (float)     R_Ω
    //============== 其他配置 =============== //
    // 屏幕设置
    Screen_Brightness,                  // 屏幕亮度           (uint16_t)  10~100
    Screen_Sleeptime,                   // 休眠时间           (uint16_t)  0~3600 s
    Screen_ShowFlip,                    // 显示翻转           (uint16_t)  0/1
    // 蜂鸣器设置
    Beezer_Volume,                      // 蜂鸣器音量          (uint16_t)  0~100
    Beezer_Time,                        // 蜂鸣时长           (uint16_t)  10 * n ms
    // 风扇设置
    Fan_Enable,                         // 是否启用风扇        (uint16_t)  0/1
    Fan_StartTemperature,                // 风扇启转温度        (uint16_t)  20~50
    Param_Number                        // 参数个数
} UserParamType_e;

#define PARAM_PATH              "0:/uparam.bin"        // 配置文件的路径
#define PARAM_TOTAL_BYTES       (Param_Number * 4)     // 配置文件总字节数（每个参数占4字节）

// 可自定义量结构体
typedef struct {
    //============= 数控电源APP ============== //
    // 校准参数
    int16_t  DPS_Voltage_Original;        // 电压零点         (int16_t)   LSB
    float    DPS_Voltage_Factor;          // 电压系数         (float)     Factor
    float    DPS_Voltage_DAC_Coefficient; // DAC系数         (float)     Coefficient
    float    DPS_Voltage_DAC_Constant;    // DAC常数项        (float)     Constant
    int16_t  DPS_Current_Original;        // 电流零点         (int16_t)   LSB
    float    DPS_Current_Factor;          // 电流系数         (float)     Factor
    // PID参数
    float    DPS_Loop_P;                  // 电流环P值        (float)     P
    float    DPS_Loop_I;                  // 电流环I值        (float)     I
    float    DPS_Loop_D;                  // 电流环D值        (float)     D
    // 快速设置
    float    DPS_Fs_Voltage_1;            // 第一组电压值      (float)     Voltage_1
    float    DPS_Fs_Current_1;            // 第一组电流值      (float)     Current_1
    float    DPS_Fs_Voltage_2;            // 第二组电压值      (float)     Voltage_2
    float    DPS_Fs_Current_2;            // 第二组电流值      (float)     Current_2
    float    DPS_Fs_Voltage_3;            // 第三组电压值      (float)     Voltage_3
    float    DPS_Fs_Current_3;            // 第三组电流值      (float)     Current_3
    float    DPS_Fs_Voltage_4;            // 第四组电压值      (float)     Voltage_4
    float    DPS_Fs_Current_4;            // 第四组电流值      (float)     Current_4
    float    DPS_Fs_Voltage_5;            // 第五组电压值      (float)     Voltage_5
    float    DPS_Fs_Current_5;            // 第五组电流值      (float)     Current_5
    // 杂项
    uint16_t DPS_Fan_Start;               // 风扇启转温度      (uint16_t)  Temp
    //============= 示波器APP ============== //
    // 校准参数
    int16_t  OSC_Original;                // 示波器X1档电压零点 (int16_t)   LSB
    int16_t  OSC_Original_X2;             // 示波器X2档电压零点 (int16_t)   LSB 
    int16_t  OSC_Original_X4;             // 示波器X4档电压零点 (int16_t)   LSB 
    int16_t  OSC_Original_X8;             // 示波器X8档电压零点 (int16_t)   LSB 
    int16_t  OSC_Original_X16;            // 示波器X16档电压零点 (int16_t)  LSB 
    int16_t  OSC_Original_X32;            // 示波器X32档电压零点 (int16_t)  LSB 
    int16_t  OSC_Original_X64;            // 示波器X64档电压零点 (int16_t)  LSB 
    int16_t  OSC_Original_X128;           // 示波器X128档电压零点 (int16_t) LSB 
    float    OSC_Factor;                  // 示波器输入分压比   (float)     Factor
    float    OSC_AMP_X2;                  // 程控放大器_X2     (float)     Amp
    float    OSC_AMP_X4;                  // 程控放大器_X4     (float)     Amp
    float    OSC_AMP_X8;                  // 程控放大器_X8     (float)     Amp
    float    OSC_AMP_X16;                 // 程控放大器_X16    (float)     Amp
    float    OSC_AMP_X32;                 // 程控放大器_X32    (float)     Amp
    float    OSC_AMP_X64;                 // 程控放大器_X64    (float)     Amp
    float    OSC_AMP_X128;                // 程控放大器_X128   (float)     Amp
    //============= 信号发生器APP ============== //
    // 校准参数
    int16_t  DDS_Original;                // 信号发生器输出零点  (int16_t)   LSB
    float    DDS_Factor;                  // 信号发生器倍率     (float)     Factor
    //============== 万用表APP =============== //
    // 电压表校准参数
    int16_t  DMM_Voltage_Original;        // 电压表零点         (int16_t)   LSB
    float    DMM_Voltage_Factor_B;        // 电压表比例系数(黑)  (float)     Factor
    float    DMM_Voltage_Factor_R;        // 电压表比例系数(红)  (float)     Factor
    // 电流表校准参数
    int16_t  DMM_Current_Original;        // 电流零点           (int16_t)   LSB
    float    DMM_Current_Factor;          // 电流系数           (float)     Factor
    // 电阻表校准参数
    int16_t  DMM_Res_R200_Original;       // 200Ω输出零点       (int16_t)   LSB
    float    DMM_Res_R200_Voltage;        // 200Ω空载电压       (float)     V
    float    DMM_Res_R200;                // 200Ω基准阻值       (float)     R_Ω
    int16_t  DMM_Res_R2K_Original;        // 2KΩ输出零点        (int16_t)   LSB
    float    DMM_Res_R2K_Voltage;         // 2KΩ空载电压        (float)     V
    float    DMM_Res_R2K;                 // 2KΩ基准阻值        (float)     R_Ω
    //============== 其他配置 =============== //
    uint16_t Screen_Brightness;           // 屏幕初始亮度        (uint16_t)  10~100
    uint16_t Screen_Sleeptime;            // 休眠时间           (uint16_t)  0~3600 s
    uint16_t Screen_ShowFlip;             // 显示翻转           (uint16_t)  0/1
    uint16_t Beezer_Volume;               // 蜂鸣器音量          (uint16_t)  0~100
    uint16_t Beezer_Time;                 // 蜂鸣时长           (uint16_t)  10 * n ms
    uint16_t Fan_Enable;                  // 是否启用风扇        (uint16_t)  0/1
    uint16_t Fan_StartTemperture;         // 风扇启转温度        (uint16_t)  20~50
} UserParamType_t;

extern UserParamType_t UserParam;

HAL_StatusTypeDef UserParam_ResetToDefault(void);
HAL_StatusTypeDef UserParam_UpdateSingle(UserParamType_e id, const void *value);
HAL_StatusTypeDef UserParam_LoadAllValues(void);
HAL_StatusTypeDef UserParam_SaveAllValues(void);

HAL_StatusTypeDef Process_Default_Data(uint8_t *buf);
HAL_StatusTypeDef Process_Current_Data(uint8_t* buf);
HAL_StatusTypeDef Write_Current_Data(uint8_t *buf, uint32_t len);
#endif //XM_POWER_KIT_USERDEFINEMANAGE_H