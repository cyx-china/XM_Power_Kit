/**
******************************************************************************
  * @file           : UserTask.h
  * @brief          : Task文件夹下所有.c源文件的头文件
  * @date           : 2026/1/31
  * @license        : CC-BY-NC-SA 4.0
  * @note           : 本文件用于声明所有 freertos 任务用到的定义、变量、函数等
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  *
  * It is released under the CC-BY-NC-SA 4.0 open sourse license.
  ******************************************************************************
  */

#ifndef XM_POWER_KIT_USERTASK_H
#define XM_POWER_KIT_USERTASK_H
#include "font.h"
#include "images.h"
#include "lv_area.h"
#include "lv_color.h"
#include "MCP4725.h"
#include "lv_svg.h"
#include "stm32f4xx_hal.h"
#include "Keys.h"

/*================ PublicExtern ================*/
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;
extern DMA_HandleTypeDef hdma_adc1;
extern DMA_HandleTypeDef hdma_adc2;
extern DMA_HandleTypeDef hdma_adc3;

extern char Sys_Version[30];
/*==================== InitTask ===================*/
typedef enum {
    INIT_OK = 0,        // 初始化成功
    FLASH_ERROR,        // FLASH初始化错误
    FATFS_ERROR,        // 文件系统初始化错误
    INA226_ERROR,       // INA226初始化错误
    TMP102_DC_ERROR,    //TMP102_DCDC初始化错误
    MCP4725_DC_ERROR,   // 数控电源MCP4725初始化错误
    MCP4725_OSC_ERROR,  // 示波器MCP4725初始化错误
    USER_PARAM_ERROR,   // 用户自定义参数值初始化失败
  } InitErrorCode;

extern MCP4725_HandleTypeDef hmcp4725_DC;
extern MCP4725_HandleTypeDef hmcp4725_OSC;

/*================== SensorTask =================*/
extern volatile float Input_Voltage;    // 输入电压
extern volatile float Input_Current;    // 输入电流
extern volatile float Input_Power;      // 输入功率

extern volatile float DCDC_Temperature;    // DCDC 温度

extern volatile uint8_t Fan_Duty_Cycle;    // 风扇占空比
/*================= LcdFlushTask ================*/
typedef enum {
    REFRESH_TYPE_LVGL = 0,    // LVGL刷新
    REFRESH_TYPE_MANUAL       // 手动绘图刷新
} RefreshType;

typedef enum {
    CLEAR_SCREEN = 0,    // 清屏
    DRAW_PIXEL,          // 绘制单个像素
    DRAW_LINE,           // 绘制直线
    DRAW_DOTTED_LINE,    // 绘制虚线
    DRAW_RECT,           // 绘制矩形
    DRAW_ROUND_RECT,     // 绘制圆角矩形
    DRAW_CIRCLE,         // 绘制圆
    DRAW_ELLIPSE,        // 绘制椭圆
    DRAW_IMAGE,          // 绘制图片
    DRAW_STRING,         // 绘制字符串
} LcdFlushType;

// 这里扣扣索索省点内存
typedef struct {
    LcdFlushType flush_type;    // 绘制类型
    uint32_t start_point;       // 起始点坐标 (x << 16) | y
    uint32_t end_point;         // 刷新结束点, 同上
    uint16_t color;             // 颜色(清屏/画线/图形/前景色)
    uint8_t is_filled;          // 是否填充
    uint16_t param1;            // 半径r/椭圆长轴a/虚线间隔interval
    uint16_t param2;            // 椭圆短轴b/虚线点数dot_num/字符串间距spacing
    const ImageInfo* image;     // 图片信息指针
    const FontInfo* font;       // 字体信息指针
    const char* str;            // 字符串指针
} GFX_DrawCommand_t;

// 用于刷新消息的结构体
typedef struct {
    RefreshType RefreshType;    // 刷新类型（LVGL/手动）
    // LVGL所需参数
    const lv_area_t * area;     // 刷新区域结构体指针
    lv_color_t * color_p;       // 数据指针

    // 手动调用刷新所需参数
    const GFX_DrawCommand_t* draw_command;  // 绘制命令指针

    // 消息的指针，方便消费端释放
    void* msg_mem;              // refresh_msg_t自身的内存指针
    void* cmd_mem;              // GFX_DrawCommand_t的内存指针
} refresh_msg_t;

/*================= PageSelectTask ================*/
// 定义app列表枚举
typedef enum {
    APP_NONE = 0,   // 中间状态/错误状态
    APP_LVGL,       // LVGL
    APP_POWER,      // 数控电源
    APP_DSO,        // 示波器
    APP_AWG,        // 任意波发生器
    APP_DMM,        // 万用表
    APP_SENSOR,     // 传感器
    APP_CAL         // 校准
}AppListType;

extern AppListType RunningApp;

/*================ IndevDetectTask ===============*/
void StartBeezer(uint16_t time_ms);

void Suspend_IndevDetectTask(void);
void Resume_IndevDetectTask(void);

/*================== LvglCoreTask =================*/
extern volatile bool IsLvglRunning;

void Suspend_LvglCoreTask(void);
void Resume_LvglCoreTask(void);

/*================== DpsCoreTask ==================*/
extern volatile bool PowerMode;

void Suspend_DpsCoreTask(void);
void Resume_DpsCoreTask(void);

/*================= CalibrateTask =================*/
// 任务挂起/恢复
void Suspend_CalibrateTask(void);
void Resume_CalibrateTask(void);

// 返回主界面（供校准模块调用）
void Calibrate_ReturnToMain(void);

/*=================== AwgTask =====================*/
void Suspend_AwgTask(void);
void Resume_AwgTask(void);


/*=================== DmmTask =====================*/
void Resume_DmmTask(void);
void Suspend_DmmTask(void);

#endif //XM_POWER_KIT_USERTASK_H

