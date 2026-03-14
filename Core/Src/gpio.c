/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, SW_10K_Pin|SW_100K_Pin|SW_200_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, S2_Pin|SYS_LED_Pin|DC_AC_Pin|SWITCH_DC_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, S1_Pin|S0_Pin|AT1_5_Pin|LCD_RST_Pin
                          |DMM_SWITCH_GND_Pin|DC_SWITCH_GND_Pin|SWITCH_V_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SWITCH_I_GPIO_Port, SWITCH_I_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, P_EN_Pin|SW_RELEASE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SWITCH_R_C_GPIO_Port, SWITCH_R_C_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : SW_10K_Pin SW_100K_Pin SWITCH_R_C_Pin SW_200_Pin */
  GPIO_InitStruct.Pin = SW_10K_Pin|SW_100K_Pin|SWITCH_R_C_Pin|SW_200_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : KEY_PWR_ON_OFF_Pin */
  GPIO_InitStruct.Pin = KEY_PWR_ON_OFF_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(KEY_PWR_ON_OFF_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : S2_Pin S1_Pin S0_Pin SYS_LED_Pin
                           AT1_5_Pin DC_AC_Pin LCD_RST_Pin DMM_SWITCH_GND_Pin
                           DC_SWITCH_GND_Pin SWITCH_DC_Pin SWITCH_V_Pin */
  GPIO_InitStruct.Pin = S2_Pin|S1_Pin|S0_Pin|SYS_LED_Pin
                          |AT1_5_Pin|DC_AC_Pin|LCD_RST_Pin|DMM_SWITCH_GND_Pin
                          |DC_SWITCH_GND_Pin|SWITCH_DC_Pin|SWITCH_V_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : KEY_MODE_Pin KEY_UP_Pin KEY_DOWN_Pin KEY_SET_Pin */
  GPIO_InitStruct.Pin = KEY_MODE_Pin|KEY_UP_Pin|KEY_DOWN_Pin|KEY_SET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : ECD_S_Pin */
  GPIO_InitStruct.Pin = ECD_S_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ECD_S_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SWITCH_I_Pin */
  GPIO_InitStruct.Pin = SWITCH_I_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SWITCH_I_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : P_EN_Pin SW_RELEASE_Pin FLASH_CS_Pin */
  GPIO_InitStruct.Pin = P_EN_Pin|SW_RELEASE_Pin|FLASH_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
