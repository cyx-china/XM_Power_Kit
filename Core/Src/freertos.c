/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>

#include "ff.h"
#include "Flash.h"
#include "i2c.h"
#include "INA226_Driver.h"
#include "Keys.h"
#include "UserTask.h"
#include "ST7789.h"
#include "SwitchManager.h"
#include "TMP102.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
uint8_t ucHeap[ configTOTAL_HEAP_SIZE ] __attribute__((section(".ccmram_rtos"), aligned(4)));

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for InitTask */
osThreadId_t InitTaskHandle;
const osThreadAttr_t InitTask_attributes = {
  .name = "InitTask",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityRealtime7,
};
/* Definitions for SensorTask */
osThreadId_t SensorTaskHandle;
const osThreadAttr_t SensorTask_attributes = {
  .name = "SensorTask",
  .stack_size = 192 * 4,
  .priority = (osPriority_t) osPriorityRealtime,
};
/* Definitions for LcdFlushTask */
osThreadId_t LcdFlushTaskHandle;
const osThreadAttr_t LcdFlushTask_attributes = {
  .name = "LcdFlushTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for PageSelectTask */
osThreadId_t PageSelectTaskHandle;
const osThreadAttr_t PageSelectTask_attributes = {
  .name = "PageSelectTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for LvglCoreTask */
osThreadId_t LvglCoreTaskHandle;
const osThreadAttr_t LvglCoreTask_attributes = {
  .name = "LvglCoreTask",
  .stack_size = 768 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for DpsCoreTask */
osThreadId_t DpsCoreTaskHandle;
const osThreadAttr_t DpsCoreTask_attributes = {
  .name = "DpsCoreTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for IndevDetectTask */
osThreadId_t IndevDetectTaskHandle;
const osThreadAttr_t IndevDetectTask_attributes = {
  .name = "IndevDetectTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal4,
};
/* Definitions for PIDTask */
osThreadId_t PIDTaskHandle;
const osThreadAttr_t PIDTask_attributes = {
  .name = "PIDTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal1,
};
/* Definitions for CalibrateTask */
osThreadId_t CalibrateTaskHandle;
const osThreadAttr_t CalibrateTask_attributes = {
  .name = "CalibrateTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for AwgTask */
osThreadId_t AwgTaskHandle;
const osThreadAttr_t AwgTask_attributes = {
  .name = "AwgTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for DmmCoreTask */
osThreadId_t DmmCoreTaskHandle;
const osThreadAttr_t DmmCoreTask_attributes = {
  .name = "DmmCoreTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for AppSwitchQueue */
osMessageQueueId_t AppSwitchQueueHandle;
const osMessageQueueAttr_t AppSwitchQueue_attributes = {
  .name = "AppSwitchQueue"
};
/* Definitions for LcdMsgQueue */
osMessageQueueId_t LcdMsgQueueHandle;
const osMessageQueueAttr_t LcdMsgQueue_attributes = {
  .name = "LcdMsgQueue"
};
/* Definitions for KeyEventQueue */
osMessageQueueId_t KeyEventQueueHandle;
const osMessageQueueAttr_t KeyEventQueue_attributes = {
  .name = "KeyEventQueue"
};
/* Definitions for BeezerQueue */
osMessageQueueId_t BeezerQueueHandle;
const osMessageQueueAttr_t BeezerQueue_attributes = {
  .name = "BeezerQueue"
};
/* Definitions for IIC1_Mutex */
osMutexId_t IIC1_MutexHandle;
const osMutexAttr_t IIC1_Mutex_attributes = {
  .name = "IIC1_Mutex"
};
/* Definitions for IIC3_Mutex */
osMutexId_t IIC3_MutexHandle;
const osMutexAttr_t IIC3_Mutex_attributes = {
  .name = "IIC3_Mutex"
};
/* Definitions for Flash_Mutex */
osMutexId_t Flash_MutexHandle;
const osMutexAttr_t Flash_Mutex_attributes = {
  .name = "Flash_Mutex"
};
/* Definitions for FLASH_TX_Cplt_Sem */
osSemaphoreId_t FLASH_TX_Cplt_SemHandle;
const osSemaphoreAttr_t FLASH_TX_Cplt_Sem_attributes = {
  .name = "FLASH_TX_Cplt_Sem"
};
/* Definitions for FLASH_RX_Cplt_Sem */
osSemaphoreId_t FLASH_RX_Cplt_SemHandle;
const osSemaphoreAttr_t FLASH_RX_Cplt_Sem_attributes = {
  .name = "FLASH_RX_Cplt_Sem"
};
/* Definitions for LCD_DMA_Cplt_Sem */
osSemaphoreId_t LCD_DMA_Cplt_SemHandle;
const osSemaphoreAttr_t LCD_DMA_Cplt_Sem_attributes = {
  .name = "LCD_DMA_Cplt_Sem"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void Start_InitTask(void *argument);
extern void Start_SensorTask(void *argument);
extern void Start_LcdFlushTask(void *argument);
extern void Start_PageSelectTask(void *argument);
extern void Start_LvglCoreTask(void *argument);
extern void Start_DpsCoreTask(void *argument);
extern void Start_IndevDetectTask(void *argument);
extern void Start_PIDTask(void *argument);
extern void Start_CalibrateTask(void *argument);
extern void Start_AwgTask(void *argument);
extern void Start_DmmCoreTask(void *argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void configureTimerForRunTimeStats(void);
unsigned long getRunTimeCounterValue(void);
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
__weak void configureTimerForRunTimeStats(void)
{

}

__weak unsigned long getRunTimeCounterValue(void)
{
return 0;
}
/* USER CODE END 1 */

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
  while (1) {
    SysLed_ON();
    HAL_Delay(100);
    SysLed_OFF();
    HAL_Delay(100);
  }
}
/* USER CODE END 4 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of IIC1_Mutex */
  IIC1_MutexHandle = osMutexNew(&IIC1_Mutex_attributes);

  /* creation of IIC3_Mutex */
  IIC3_MutexHandle = osMutexNew(&IIC3_Mutex_attributes);

  /* creation of Flash_Mutex */
  Flash_MutexHandle = osMutexNew(&Flash_Mutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of FLASH_TX_Cplt_Sem */
  FLASH_TX_Cplt_SemHandle = osSemaphoreNew(1, 0, &FLASH_TX_Cplt_Sem_attributes);

  /* creation of FLASH_RX_Cplt_Sem */
  FLASH_RX_Cplt_SemHandle = osSemaphoreNew(1, 0, &FLASH_RX_Cplt_Sem_attributes);

  /* creation of LCD_DMA_Cplt_Sem */
  LCD_DMA_Cplt_SemHandle = osSemaphoreNew(1, 0, &LCD_DMA_Cplt_Sem_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of AppSwitchQueue */
  AppSwitchQueueHandle = osMessageQueueNew (1, sizeof(AppListType), &AppSwitchQueue_attributes);

  /* creation of LcdMsgQueue */
  LcdMsgQueueHandle = osMessageQueueNew (48, sizeof(refresh_msg_t*), &LcdMsgQueue_attributes);

  /* creation of KeyEventQueue */
  KeyEventQueueHandle = osMessageQueueNew (8, sizeof(KeyEventMsg_t), &KeyEventQueue_attributes);

  /* creation of BeezerQueue */
  BeezerQueueHandle = osMessageQueueNew (8, sizeof(uint16_t), &BeezerQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of InitTask */
  InitTaskHandle = osThreadNew(Start_InitTask, NULL, &InitTask_attributes);

  /* creation of SensorTask */
  SensorTaskHandle = osThreadNew(Start_SensorTask, NULL, &SensorTask_attributes);

  /* creation of LcdFlushTask */
  LcdFlushTaskHandle = osThreadNew(Start_LcdFlushTask, NULL, &LcdFlushTask_attributes);

  /* creation of PageSelectTask */
  PageSelectTaskHandle = osThreadNew(Start_PageSelectTask, NULL, &PageSelectTask_attributes);

  /* creation of LvglCoreTask */
  LvglCoreTaskHandle = osThreadNew(Start_LvglCoreTask, NULL, &LvglCoreTask_attributes);

  /* creation of DpsCoreTask */
  DpsCoreTaskHandle = osThreadNew(Start_DpsCoreTask, NULL, &DpsCoreTask_attributes);

  /* creation of IndevDetectTask */
  IndevDetectTaskHandle = osThreadNew(Start_IndevDetectTask, NULL, &IndevDetectTask_attributes);

  /* creation of PIDTask */
  PIDTaskHandle = osThreadNew(Start_PIDTask, NULL, &PIDTask_attributes);

  /* creation of CalibrateTask */
  CalibrateTaskHandle = osThreadNew(Start_CalibrateTask, NULL, &CalibrateTask_attributes);

  /* creation of AwgTask */
  AwgTaskHandle = osThreadNew(Start_AwgTask, NULL, &AwgTask_attributes);

  /* creation of DmmCoreTask */
  DmmCoreTaskHandle = osThreadNew(Start_DmmCoreTask, NULL, &DmmCoreTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_Start_InitTask */
/**
  * @brief  Function implementing the InitTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_Start_InitTask */
__weak void Start_InitTask(void *argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN Start_InitTask */
  /* Infinite loop */
  for(;;){
    // 此函数为弱定义，实际定义于Task文件夹内的InitTask中
  }
  /* USER CODE END Start_InitTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

