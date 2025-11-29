/*
 * ls_comms.h - Main callback functions for LS-02 communication interfaces
 *
 *  Created on: Nov 25, 2025
 *      Author: finazzi
 */

#ifndef __LS_COMMS_H__
#define __LS_COMMS_H__

#include "i2c.h"
#include "usart.h"

#define INSTRUCTION_SIZE 4								// 1B instruction code + 3B opcode
#define DATA_FRAME_SIZE 119								// maximum size in Bytes for Air MAC frame, 9B header, 110B data

extern volatile uint8_t new_command_received;			// new command received flag
extern uint8_t tx_buffer[DATA_FRAME_SIZE];				// tx data buffer
extern volatile uint8_t rx_buffer[INSTRUCTION_SIZE]; 	// rx instruction buffer, volatile because it can be modified externally


/**********************************************************
 * UART Rx interrupt callback. Uses UART1 for communication
 * with LabOSat-02
 **********************************************************/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

/**********************************************************
 * i2C Rx interrupt callback. Uses i2C3 for communication
 * with LabOSat-02
 **********************************************************/
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c);

/**********************************************************
 * Transmits stored buffer to LabOSat-02 through UART1
 * communication interface
 **********************************************************/
void TransmitBufferUART(void);

/**********************************************************
 * Transmits stored buffer to LabOSat-02 through i2C3
 * communication interface
 **********************************************************/
void TransmitBufferi2C(void);

/**********************************************************
 * Function for logging information to UART4
 **********************************************************/
void Log(char *message);

/**********************************************************
 * Aux. function to copy volatile rx_buffer into
 * non-volatile variable
 **********************************************************/
void CopyVolatile(uint8_t *target, volatile uint8_t *data);

/**********************************************************
 * Aux. function to transform system timestamp from uint32_t
 * to char* for logging printing
 **********************************************************/
void TransformTs(char *timestamp_string);

#endif /* __LS_COMMS_H__ */
