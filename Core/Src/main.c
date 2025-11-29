/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include <command.h>
#include <ls_comms.h>
#include "main.h"
#include "dcmi.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "fsmc.h"
#include <string.h>
//#include "stm32f2xx_hal_sram.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

#define COMM_UART	// defines communication with LS-02 as UART
//#define COMM_I2C	// defines communication with LS-02 as i2C

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

typedef enum {								// State machine for program flow
    STATE_IDLE = 0,
    STATE_EXECUTE_COMMAND,
	STATE_TRANSMIT_RESPONSE
} app_state_t;

uint32_t timestamp;
char *timestamp_string;
char *log_message; 							// placeholder for log messages

/* USER CODE END PV */


/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  for(size_t i = 0; i < DATA_FRAME_SIZE; i++)		// tx_buffer init
	  tx_buffer[i] = 0x00;

  app_state_t state = STATE_IDLE;							// program starts in IDLE state
  uint8_t current_instruction;								// current program instruction
  const command_t* current_command_pointer;					// pointer to current command
  uint8_t rx_buffer_copy[INSTRUCTION_SIZE];					// copy of rx buffer in program memory
  int ret = 0;												// return for ExecuteCommand()

  MX_FSMC_Init();											// initializes external SRAM
  MX_DCMI_Init();											// Initializes DCMI


  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  timestamp = HAL_GetTick(); 	// system timestamp in 1ms intervals - Updated by interrupt

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C3_Init();
  MX_USART1_UART_Init();
  MX_DCMI_Init();
  MX_FSMC_Init();
  MX_I2C2_Init();
  MX_SPI2_Init();
  MX_TIM11_Init();
  MX_UART4_Init();
  MX_UART5_Init();
  /* USER CODE BEGIN 2 */

  #if defined(COMM_UART) && !defined(COMM_I2C)
  	  HAL_UART_Receive_IT(&huart1, (uint8_t*)rx_buffer, INSTRUCTION_SIZE);
  #elif defined(COMM_I2C) && !defined(COMM_UART)
  	  HAL_I2C_Slave_Receive_IT(&hi2c3, (uint8_t *)rx_buffer, INSTRUCTION_SIZE);
  #else
  	  #warning "Invalid configuration detected in USS <--> LS-02 communication.\n"
  #endif

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  switch (state) {
		  case STATE_IDLE:
			  if (new_command_received) {
				  new_command_received = 0;
				  state = STATE_EXECUTE_COMMAND;
			  }
			  break;

		  case STATE_EXECUTE_COMMAND:
			  CopyVolatile(rx_buffer_copy, rx_buffer);														// copies rx_buffer into non-volatile
			  current_instruction = rx_buffer_copy[0];
			  current_command_pointer = GetCommand(current_instruction);

			  ret = ExecuteCommand(current_command_pointer, &rx_buffer_copy[1]);
			  if (ret) Log("Return success\n");
			  else Log("Return failure!\n");

			  state = STATE_TRANSMIT_RESPONSE;
			  break;

		  case STATE_TRANSMIT_RESPONSE:
			  #if defined(COMM_UART) && !defined(COMM_I2C)
				  TransmitBufferUART();
			  #elif defined(COMM_I2C) && !defined(COMM_UART)
				  TransmitBufferi2C();
			  #endif
			  state = STATE_IDLE;
			  break;

		  default:
			  state = STATE_IDLE;
			  break;
	  }


    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Initializes the RCC Oscillators according to the specified parameters
	* in the RCC_OscInitTypeDef structure.
	*/
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 192;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
	Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	*/
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
							  |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
	{
	Error_Handler();
	}
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
