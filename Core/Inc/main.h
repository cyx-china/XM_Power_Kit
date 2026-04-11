/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "cmsis_os2.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SW_10K_Pin GPIO_PIN_2
#define SW_10K_GPIO_Port GPIOE
#define SW_100K_Pin GPIO_PIN_3
#define SW_100K_GPIO_Port GPIOE
#define KEY_PWR_ON_OFF_Pin GPIO_PIN_4
#define KEY_PWR_ON_OFF_GPIO_Port GPIOE
#define BEEZER_Pin GPIO_PIN_5
#define BEEZER_GPIO_Port GPIOE
#define S2_Pin GPIO_PIN_13
#define S2_GPIO_Port GPIOC
#define S1_Pin GPIO_PIN_14
#define S1_GPIO_Port GPIOC
#define S0_Pin GPIO_PIN_15
#define S0_GPIO_Port GPIOC
#define SYS_LED_Pin GPIO_PIN_0
#define SYS_LED_GPIO_Port GPIOC
#define AT1_5_Pin GPIO_PIN_1
#define AT1_5_GPIO_Port GPIOC
#define DC_AC_Pin GPIO_PIN_2
#define DC_AC_GPIO_Port GPIOC
#define LCD_RST_Pin GPIO_PIN_3
#define LCD_RST_GPIO_Port GPIOC
#define KEY_MODE_Pin GPIO_PIN_12
#define KEY_MODE_GPIO_Port GPIOB
#define KEY_UP_Pin GPIO_PIN_13
#define KEY_UP_GPIO_Port GPIOB
#define KEY_DOWN_Pin GPIO_PIN_14
#define KEY_DOWN_GPIO_Port GPIOB
#define KEY_SET_Pin GPIO_PIN_15
#define KEY_SET_GPIO_Port GPIOB
#define ECD_B_Pin GPIO_PIN_12
#define ECD_B_GPIO_Port GPIOD
#define ECD_A_Pin GPIO_PIN_13
#define ECD_A_GPIO_Port GPIOD
#define ECD_S_Pin GPIO_PIN_6
#define ECD_S_GPIO_Port GPIOC
#define DMM_SWITCH_GND_Pin GPIO_PIN_7
#define DMM_SWITCH_GND_GPIO_Port GPIOC
#define LCD_PWM_T1C2_Pin GPIO_PIN_9
#define LCD_PWM_T1C2_GPIO_Port GPIOA
#define FAN_PWM_T1C3_Pin GPIO_PIN_10
#define FAN_PWM_T1C3_GPIO_Port GPIOA
#define SWITCH_I_Pin GPIO_PIN_15
#define SWITCH_I_GPIO_Port GPIOA
#define DC_SWITCH_GND_Pin GPIO_PIN_10
#define DC_SWITCH_GND_GPIO_Port GPIOC
#define SWITCH_DC_Pin GPIO_PIN_11
#define SWITCH_DC_GPIO_Port GPIOC
#define SWITCH_V_Pin GPIO_PIN_12
#define SWITCH_V_GPIO_Port GPIOC
#define P_EN_Pin GPIO_PIN_2
#define P_EN_GPIO_Port GPIOD
#define SW_RELEASE_Pin GPIO_PIN_3
#define SW_RELEASE_GPIO_Port GPIOD
#define FLASH_CS_Pin GPIO_PIN_6
#define FLASH_CS_GPIO_Port GPIOD
#define FLASH_SPI1_SCK_Pin GPIO_PIN_3
#define FLASH_SPI1_SCK_GPIO_Port GPIOB
#define FLASH_SPI1_MISO_Pin GPIO_PIN_4
#define FLASH_SPI1_MISO_GPIO_Port GPIOB
#define FLASH_SPI1_MOSI_Pin GPIO_PIN_5
#define FLASH_SPI1_MOSI_GPIO_Port GPIOB
#define SWITCH_R_C_Pin GPIO_PIN_0
#define SWITCH_R_C_GPIO_Port GPIOE
#define SW_200_Pin GPIO_PIN_1
#define SW_200_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
