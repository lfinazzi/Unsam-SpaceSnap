/*
 * command.c - Instruction and command list and high level execution functions
 *
 *  Created on: Nov 25, 2025
 *      Author: finazzi
 */

#include <command.h>
#include "usart.h"
#include <string.h>
#include <stdio.h>


HAL_StatusTypeDef CMD_TakePicture(uint8_t *opcode) {
	uint8_t cam_number 		= opcode[0] % 0x01;			// 0000_0001 mask, TODO - check endianness and ordering of bytes
	uint8_t buffer_number 	= opcode[0] & 0x06;	    	// 0000_0110 mask
	uint8_t tries 		 	= opcode[1] & 0x0F;			// 0000_1111 mask
	uint8_t compression		= opcode[1] & 0x30;			// 0011_0000 mask
	uint8_t black_filtering = opcode[2] & 0x01;			// 0000_0001 mask
	uint8_t black_threshold = opcode[2] & 0xFE;	 		// 1111_1110 mask - 7b

	float threshold_float = (float)black_threshold * (float)(BLACK_THRESHOLD_UNITS);

	uint8_t current_tries    = 0;
	float   result 			 = 0.0f;
	uint8_t success 		 = 0;
	uint32_t compressed_size = 0;

	while (current_tries < tries) {
		// send take picture command to corresponding camera
		DCMICapture(cam_number, buffer_number);

		if(black_filtering == 0) { // no black filtering
			success = 1;
			break;
		}

		// Check how much black is on picture. If it doesn't pass filtering, take another picture. Try the corresponding amount of times
		ComputeBlackPercentage(&result, buffer_number);

		if (result <= threshold_float) { // TODO - make it so that this value can be changed
			success = 1;
			break;	// picture accepted
		}
		current_tries++;
	}

	if(success) {
		CompressToJPEG(buffer_number, compression, &compressed_size); 	// compresses and saves compressed image to current index addres in SRAM

		// Save a version of compressed picture to NVM


		tx_buffer[0] = 0xA0;										// execution successful
		tx_buffer[1] = (uint16_t)(timestamp & 0xFFFF0000 >> 16);	// timestamp MSB
		tx_buffer[2] = (uint16_t)(timestamp & 0x0000FFFF      );	// timestamp LSB
		// TODO - something else in return buffer?
		return HAL_OK;
	}
	
	tx_buffer[0] = 0xB0; 		// execution failed
	tx_buffer[1] = (uint16_t)(timestamp & 0xFFFF0000 >> 16);	// timestamp MSB
	tx_buffer[2] = (uint16_t)(timestamp & 0x0000FFFF      );	// timestamp LSB
	return HAL_ERROR;
}

// ===== Example Handlers =====
HAL_StatusTypeDef CMD_TakePictureForced(uint8_t *opcode) {
	return HAL_OK;
}

HAL_StatusTypeDef CMD_TransmitFrameCompressed(uint8_t *opcode) {
	return HAL_OK;

}

HAL_StatusTypeDef CMD_TransmitFrameRaw(uint8_t *opcode) {
	return HAL_OK;

}

HAL_StatusTypeDef CMD_GetStatus(uint8_t *opcode) {
	return HAL_OK;

}

HAL_StatusTypeDef CMD_BackupVolatileMemory(uint8_t *opcode) {
	return HAL_OK;

}

HAL_StatusTypeDef CMD_ResetPayload(uint8_t *opcode) {
	return HAL_OK;

}


// TODO - other functions to implement


// ===== Command Table =====
const command_t command_table[NUM_COMMANDS] = {

    { "TAKE_PICTURE", 0x33, CMD_TakePicture, 			  	 			"Capture an image (tries N times, may fail if no images are good enough) "
    										 	 	 	 	 	 	 	"and saves a copy to non-volatile buffer. Compresses the photo and saves it "
    										 	 	 	 	 	 	    "to volatile and non-volatile memory.", 0, 20000 },

    { "TAKE_PICTURE_FORCED", 0x34, CMD_TakePictureForced, 	 			"Same as TAKE_PICTURE but if no good pictures are captured after "
    													  	 	 	 	"N tries, it saves that last picture.", 0, 20000 },

    { "TRANSMIT_FRAME_COMPRESSED", 0x35, CMD_TransmitFrameCompressed, 	"Transmits a 110B frame of a compressed image with a certain index", 0, 20000 },

    { "TRANSMIT_FRAME_RAW", 0x36, CMD_TransmitFrameRaw, 			    "Transmits a 110B frame of a raw image in a certain buffer", 0, 20000 },


	{ "GET_STATUS", 0x64, CMD_GetStatus, 								"Payload status - TODO", 1, 20000 },

    { "BACKUP_VOL_MEMORY", 0x65, CMD_BackupVolatileMemory, 				"Makes a copy of volatile memory to non-volatile memory for backup "
    																	"in case of power down.", 1, 20000 },

    { "RESET_PAYLOAD", 0x66, CMD_ResetPayload, 						    "Software reset for UNSAM SpaceSnap", 1, 20000 },

};

const uint16_t COMMAND_COUNT = sizeof(command_table) / sizeof(command_table[0]);

// ===== Lookup Function =====
const command_t* GetCommand(uint8_t instruction_number)
{
    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        if (command_table[i].instruction_number == instruction_number) {
        	sprintf(log_message, "Received command %s", command_table[i].name);
        	Log(log_message);
            return &command_table[i];
        }
    }
    Log("Invalid command received, not executed.");
    return NULL;
}

// ===== Execute Function =====
HAL_StatusTypeDef ExecuteCommand(const command_t *command, uint8_t *opcode)
{
    if (!command) return HAL_ERROR;

    command->handler(opcode);
    if(command->takes_opcode){
    	char opcode_text[60];
    	snprintf(opcode_text, sizeof(opcode_text), "Command executed successfully with opcode %02x %02x %02x", opcode[0], opcode[1], opcode[2]);
    	Log(opcode_text);
    }
    else
    	Log("Command executed successfully");
    return HAL_OK;
}
