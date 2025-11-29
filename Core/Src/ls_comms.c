/*
 * ls_comms.c - Main callback functions for LS-02 communication interfaces
 *
 *  Created on: Nov 25, 2025
 *      Author: Finazzi
 */

#include <command.h>
#include <ls_comms.h>
#include <string.h>
#include <stdio.h>

volatile uint8_t new_command_received = 0;			// command received when rx_buffer is full
uint8_t tx_buffer[DATA_FRAME_SIZE];					// instruction rx buffer
volatile uint8_t rx_buffer[INSTRUCTION_SIZE];		// response tx buffer
extern uint32_t timestamp;
extern char timestamp_string[10];

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1) {
		new_command_received = 1;
		/* Re-arm reception */
		HAL_UART_Receive_IT(&huart1, (uint8_t *)rx_buffer, INSTRUCTION_SIZE);
	}
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	if (hi2c->Instance == I2C3) {
		new_command_received = 1;
		/* Re-arm */
		HAL_I2C_Slave_Receive_IT(&hi2c3, (uint8_t *)rx_buffer, INSTRUCTION_SIZE);
	}
}

void TransmitBufferUART()
{
	HAL_UART_Transmit_IT(&huart1, (uint8_t *)tx_buffer, DATA_FRAME_SIZE);
}

void TransmitBufferi2C()
{
	HAL_I2C_Slave_Transmit_IT(&hi2c3, (uint8_t *)tx_buffer, DATA_FRAME_SIZE);
}

void Log(char *message)
{
	TransformTs(timestamp_string);
	HAL_UART_Transmit_IT(&huart4, (uint8_t *)"[", sizeof("["));
	HAL_UART_Transmit_IT(&huart4, (uint8_t *)timestamp_string, sizeof(timestamp_string));
	HAL_UART_Transmit_IT(&huart4, (uint8_t *)"] ", sizeof("] "));
	HAL_UART_Transmit_IT(&huart4, (uint8_t *)message, sizeof(message));
	HAL_UART_Transmit_IT(&huart4, (uint8_t *)"\n", sizeof("\n"));
}

void CopyVolatile(uint8_t *target, volatile uint8_t *data)
{
	size_t N = sizeof(data);
	for(size_t i = 0; i < N; i++)
		target[i] = data[i];
}

void TransformTs(char *timestamp_string)
{
	sprintf(timestamp_string, "%lu", timestamp);		// logs the time in seconds since startup
}


