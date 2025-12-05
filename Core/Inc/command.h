/*
 * command.h - Instruction and command list and high level execution functions
 *
 *  Created on: Nov 25, 2025
 *      Author: finazzi
 */

#ifndef __COMMAND_H__
#define __COMMAND_H__

#include <ls_comms.h>
#include <photo.h>
#include <stdint.h>
#include "ls_comms.h"

#define NUM_COMMANDS 	  (13U)		// this needs to be changed to reflect exact number of istructions or risk an illegal memory access - TODO

// Handler type for all commands
typedef HAL_StatusTypeDef (*command_handler_t)(uint8_t*);
extern char* log_message;
extern uint32_t timestamp;

// Command structure definition
typedef struct {
    const char *name;                 // Human-readable name
    uint8_t instruction_number;       // Numeric command ID
    command_handler_t handler;        // Function pointer to execute
    const char *description;          // Description
    int takes_opcode;		      	  // Does instruction take opcode?
    uint32_t timeout_ms;              // Expected max. execution time in ms
} command_t;


const extern command_t command_table[NUM_COMMANDS];

// Lookup and execution functions
const command_t* GetCommand(uint8_t instruction_number);
HAL_StatusTypeDef ExecuteCommand(const command_t *command, uint8_t *opcode);

/**********************************************************
 * Helper function to fill tx_buffer with all zeroes. This
 * is used for commands that don't return data
 **********************************************************/
void FillTxBufferWithZeroes(void);

/**********************************************************
 * High level function to take a picture from a certain
 * camera and save it an image buffer after checking it,
 * compress it, and save it in volatile and non-volatile
 * memory.
 *
 * opcode:
 * 1st Byte: camera number (0 or 1, 1b), buffer number (0, 1, 2, 2b) - [X, X, X, X, X, buffer_number[1], buffer_number[0], camera_number]
 * 2nd Byte: tries to attempt (1-15, 4b), compression (0, 1, 2, 3, 2b)  - [X, X, compression[1], compression[0], tries[3], tries[2], tries[1], tries[0]]
 * 3rd Byte: black filtering (1b), black_treshold (7b) - [thr[7], thr[6], thr[5], thr[4], thr[3], thr[2], thr[1], thr[0], filtering]
 **********************************************************/
HAL_StatusTypeDef CMD_TakePicture(uint8_t *opcode);


// high level command functions - TODO
HAL_StatusTypeDef CMD_TakePictureForced(uint8_t *opcode);
HAL_StatusTypeDef CMD_TransmitFrameCompressed(uint8_t *opcode);
HAL_StatusTypeDef CMD_TransmitFrameRaw(uint8_t *opcode);
HAL_StatusTypeDef CMD_GetStatus(uint8_t *opcode);
HAL_StatusTypeDef CMD_BackupVolatileMemory(uint8_t *opcode);
HAL_StatusTypeDef CMD_ResetPayload(uint8_t *opcode);


#endif // __COMMAND_H__

