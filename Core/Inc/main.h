/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "stm32f2xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
#define OSC_IN_Pin GPIO_PIN_0
#define OSC_IN_GPIO_Port GPIOH
#define OSC_OUT_Pin GPIO_PIN_1
#define OSC_OUT_GPIO_Port GPIOH
#define RESET_BAR_Pin GPIO_PIN_0
#define RESET_BAR_GPIO_Port GPIOC
#define DEB_UART_TX_Pin GPIO_PIN_0
#define DEB_UART_TX_GPIO_Port GPIOA
#define DEB_UART_RX_Pin GPIO_PIN_1
#define DEB_UART_RX_GPIO_Port GPIOA
#define IMGA_ENA_Pin GPIO_PIN_2
#define IMGA_ENA_GPIO_Port GPIOA
#define IMGB_ENA_Pin GPIO_PIN_3
#define IMGB_ENA_GPIO_Port GPIOA
#define RS485_RE_Pin GPIO_PIN_0
#define RS485_RE_GPIO_Port GPIOB
#define RS485_DE_Pin GPIO_PIN_1
#define RS485_DE_GPIO_Port GPIOB
#define SCLK_Pin GPIO_PIN_10
#define SCLK_GPIO_Port GPIOB
#define SDATA_Pin GPIO_PIN_11
#define SDATA_GPIO_Port GPIOB
#define CS_N_Pin GPIO_PIN_12
#define CS_N_GPIO_Port GPIOB
#define SCK_Pin GPIO_PIN_13
#define SCK_GPIO_Port GPIOB
#define MISO_Pin GPIO_PIN_14
#define MISO_GPIO_Port GPIOB
#define MOSI_Pin GPIO_PIN_15
#define MOSI_GPIO_Port GPIOB
#define SDATA_LS_Pin GPIO_PIN_9
#define SDATA_LS_GPIO_Port GPIOC
#define SCLK_LS_Pin GPIO_PIN_8
#define SCLK_LS_GPIO_Port GPIOA
#define UART1_TX_Pin GPIO_PIN_9
#define UART1_TX_GPIO_Port GPIOA
#define UART1_RX_Pin GPIO_PIN_10
#define UART1_RX_GPIO_Port GPIOA
#define IMG_I2C_ENA_Pin GPIO_PIN_11
#define IMG_I2C_ENA_GPIO_Port GPIOA
#define IMG_ENA_Pin GPIO_PIN_12
#define IMG_ENA_GPIO_Port GPIOA
#define UART_TX_Pin GPIO_PIN_12
#define UART_TX_GPIO_Port GPIOC
#define UART_RX_Pin GPIO_PIN_2
#define UART_RX_GPIO_Port GPIOD
#define MEMO_OE_Pin GPIO_PIN_4
#define MEMO_OE_GPIO_Port GPIOD
#define MEMO_WE_Pin GPIO_PIN_5
#define MEMO_WE_GPIO_Port GPIOD
#define MEMO_CS_Pin GPIO_PIN_10
#define MEMO_CS_GPIO_Port GPIOG
#define MEMO_UB_Pin GPIO_PIN_12
#define MEMO_UB_GPIO_Port GPIOG
#define MEMO_LB_Pin GPIO_PIN_13
#define MEMO_LB_GPIO_Port GPIOG

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
