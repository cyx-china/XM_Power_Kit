/**
******************************************************************************
  * @file           : InitTask.c
  * @brief          : 系统初始化任务，包括USB设备、FLASH、FATFS文件系统、传感器（INA226、TMP102）、外设（MCP4725、LCD、编码器、按键）
  *                   以及用户参数的初始化。初始化成功后启动应用切换任务，初始化失败时通过UART输出错误信息。
  * @date           : 2026/2/21
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  *
  * This system initialization task module is independently developed by the author.
  * It is released under the CC-BY-NC-SA 4.0 open source license.
  * And the author's right of attribution is reserved.
  ******************************************************************************
  */

#include "Encoder.h"
#include "os_handles.h"
#include "usart.h"
#include "usb_device.h"
#include "UserTask.h"
#include "ff.h"
#include "Flash.h"
#include "i2c.h"
#include "INA226_Driver.h"
#include "MCP4725.h"
#include "ST7789.h"
#include "SwitchManager.h"
#include "tim.h"
#include "TMP102.h"
#include "Manager/UserDefineManage.h"

FATFS fs;   FIL fil;    FRESULT res;    UINT br;    // Fatfs所需的全局变量

INA226_HandleTypeDef hina226_input;                 // INA226设备句柄，定义在InputCollectTask.h中

TMP102_HandleTypeDef htmp102_dcdc;                  // TMP102设备句柄，定义在TempDetectTask.h中

MCP4725_HandleTypeDef hmcp4725_DC;
MCP4725_HandleTypeDef hmcp4725_OSC;


static InitErrorCode Sys_Init(void);


void Start_InitTask(void *argument)
{
  // 初始化USB设备驱动
  MX_USB_DEVICE_Init();

  // 开始系统初始化
  InitErrorCode result = Sys_Init();
  if (result == INIT_OK) {
    osThreadResume(PageSelectTaskHandle);       // 释放 App 切换任务
    osThreadExit();                             // 任务自删除
  }

  // 初始化失败，进入错误处理循环，每2s通过串口发送一次错误信息
  LCD_Clear(0xf800);  // 清屏红色
  char message[40];

  for(;;){
    switch (result) {
      case FLASH_ERROR:
        sprintf(message, "System Init Error: FLASH_ERROR\r\n");
        HAL_UART_Transmit(&huart3, (uint8_t*)message, strlen(message), 100);
        break;
      case FATFS_ERROR:
        sprintf(message, "System Init Error: FATFS_ERROR\r\n");
        HAL_UART_Transmit(&huart3, (uint8_t*)message, strlen(message), 100);
        break;
      case INA226_ERROR:
        sprintf(message, "System Init Error: INA226_ERROR\r\n");
        HAL_UART_Transmit(&huart3, (uint8_t*)message, strlen(message), 100);
        break;
      case TMP102_DC_ERROR:
        sprintf(message, "System Init Error: TMP102_DC_ERROR\r\n");
        HAL_UART_Transmit(&huart3, (uint8_t*)message, strlen(message), 100);
        break;
      case MCP4725_DC_ERROR:
        sprintf(message, "System Init Error: MCP4725_DC_ERROR\r\n");
        HAL_UART_Transmit(&huart3, (uint8_t*)message, strlen(message), 100);
      case MCP4725_OSC_ERROR:
        sprintf(message, "System Init Error: MCP4725_OSC_ERROR\r\n");
        HAL_UART_Transmit(&huart3, (uint8_t*)message, strlen(message), 100);
      case USER_PARAM_ERROR:
        sprintf(message, "System Init Error: USER_PARAM_ERROR\r\n");
        HAL_UART_Transmit(&huart3, (uint8_t*)message, strlen(message), 100);
      default: {break;}
    }
    HAL_Delay(2000);  // 此处要用占用CPU的延迟
  }
}


static InitErrorCode Sys_Init(void) {
  // 初始化显示设备
  LCD_Init();
  LCD_Clear(0x0000); // 黑色清屏
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2); // 开启显示

  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 0); // 设置风扇占空比为0
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3); // 开启风扇

  // 初始化按键 & 旋转编码器
  Key_Init();
  Encoder_Init();

  // 初始化FLASH
  if (FLASH_Init() != HAL_OK) {
    return FLASH_ERROR; // FLASH初始化失败
  }

  // 挂载文件系统
  res = f_mount(&fs, "0:", 1);
  if (res != FR_OK) {
    return FATFS_ERROR; // 挂载失败
  }

  // 初始化 输入INA226  // 最大16A，分流电阻0.05Ω
  if (INA226_Init(&hina226_input, &hi2c1, INPUT_ADDR, 16.0f, 0.005f) != HAL_OK) {
    return INA226_ERROR; // INA226初始化失败
  }

  // 初始化TMP102
  htmp102_dcdc.hi2c = &hi2c1;
  htmp102_dcdc.dev_addr   = 0x48 << 1;        // 0x90

  if (TMP102_Init(&htmp102_dcdc) != HAL_OK) {return TMP102_DC_ERROR; }

  // 初始化MCP4725
  if (MCP4725_Init(&hmcp4725_DC,&hi2c3,MCP4725_ADDR) != HAL_OK){return MCP4725_DC_ERROR;}
  if (MCP4725_Init(&hmcp4725_OSC,&hi2c1,MCP4725_ADDR) != HAL_OK){return MCP4725_OSC_ERROR;}

  // 读取用户自定义数据
  if (UserParam_LoadAllValues() != HAL_OK){return USER_PARAM_ERROR;}

  // 设置屏幕亮度
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 4999 * UserParam.Screen_Brightness / 100);

  DpsRelease_ON();





  return INIT_OK; // 初始化成功
}


