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
#include "ls_comms.h"


HAL_StatusTypeDef CMD_TakePicture(uint8_t *opcode) {
	uint8_t cam_number 		= opcode[0] & 0x01;			// 0000_0001 mask, TODO - check endianness and ordering of bytes
	uint8_t buffer_number 	= opcode[0] & 0x06;	    	// 0000_0110 mask
	uint8_t tries 		 	= opcode[1] & 0x0F;			// 0000_1111 mask
	uint8_t compression		= opcode[1] & 0x30;			// 0011_0000 mask
	uint8_t black_filtering = opcode[2] & 0x01;			// 0000_0001 mask
	uint8_t black_threshold = opcode[2] & 0xFE;	 		// 1111_1110 mask - 7b
	// opcode[3] unused for this Command

	float threshold_float = (float)black_threshold * (float)(BLACK_THRESHOLD_UNITS);

	uint8_t current_tries    = 0;
	float   result 			 = 0.0f;
	uint8_t success 		 = 0;
	uint32_t compressed_size = 0;

	FillTxBufferWithZeroes();		// Fills Tx buffer with zeroes
	while (current_tries < tries) {
		// send take picture command to corresponding camera
		HAL_StatusTypeDef st = DCMICapture(cam_number, buffer_number, opcode);
		while(!frame_done) {
			if (st == HAL_ERROR){
				tx_buffer[1] = DCMI_CAPTURE_ERR;
				return st;
			}
			// wait for frame - TODO: Implement timeout
		}

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
		HAL_StatusTypeDef st = CompressToJPEG(buffer_number, compression, &compressed_size, opcode); 	// compresses and saves compressed image to current index addres in SRAM
		if(st == HAL_ERROR) {
			tx_buffer[1] = COMPRESSION_ERR;
			return st;
		}

		// Save a version of compressed picture to FRAM - TODO

		// TODO - something else in return buffer?
		return HAL_OK;
	}
	
	tx_buffer[1] = BLACK_FILTERING_ERR; 							// execution failed due to filtering
	// TODO - something else in return buffer?
	return HAL_ERROR;
}

HAL_StatusTypeDef CMD_TakePictureDelayed(uint8_t *opcode) {
	uint8_t cam_number 		= opcode[0] & 0x01;			// 0000_0001 mask, TODO - check endianness and ordering of bytes
	uint8_t buffer_number 	= opcode[0] & 0x06;	    	// 0000_0110 mask
	uint8_t tries 		 	= opcode[1] & 0x0F;			// 0000_1111 mask
	uint8_t compression		= opcode[1] & 0x30;			// 0011_0000 mask
	uint8_t black_filtering = opcode[2] & 0x01;			// 0000_0001 mask
	uint8_t black_threshold = opcode[2] & 0xFE;	 		// 1111_1110 mask - 7b
	uint8_t delay 			= opcode[3]       ;	 		// 8b - Delay in 5 minute increments for photo capture

	float threshold_float = (float)black_threshold * (float)(BLACK_THRESHOLD_UNITS);

	uint8_t current_tries    = 0;
	float   result 			 = 0.0f;
	uint8_t success 		 = 0;
	uint32_t compressed_size = 0;

	uint8_t real_tries = 9 * tries;				// Max. 256 tries of picture capture - This is executed while satellite is not over GS, so we might want many tries
	uint32_t delay_ms = 300 * delay * 1000;	    // Delay has a resolution of 5 minutes

	uint32_t start = HAL_GetTick();
	while (HAL_GetTick() - start < delay_ms) {}	// waits. See how we can implement a timeout of something to avoid lockout (add tx_buffer failed due to timeout). TODO

	FillTxBufferWithZeroes();		// Fills Tx buffer with zeroes
	while (current_tries < real_tries) {
		// send take picture command to corresponding camera
		HAL_StatusTypeDef st = DCMICapture(cam_number, buffer_number, opcode);
		while(!frame_done) {
			if (st == HAL_ERROR){
				tx_buffer[1] = DCMI_CAPTURE_ERR;
				return st;
			}
			// wait for frame - TODO: Implement timeout
		}

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
		HAL_StatusTypeDef st = CompressToJPEG(buffer_number, compression, &compressed_size, opcode); 	// compresses and saves compressed image to current index addres in SRAM
		if(st == HAL_ERROR) {
			tx_buffer[1] = COMPRESSION_ERR;
			return st;
		}

		// Save a version of compressed picture to FRAM - TODO

		// TODO - something else in return buffer?
		return HAL_OK;
	}

	tx_buffer[1] = BLACK_FILTERING_ERR; 							// execution failed due to filtering
	// TODO - something else in return buffer?
	return HAL_ERROR;
}

// ===== Example Handlers =====
HAL_StatusTypeDef CMD_TransmitFrameCompressed(uint8_t *opcode) {
	uint8_t  index_number 	=  opcode[0];
	uint16_t frame_number 	= (opcode[2] << 8) | opcode[1];
	//opcode[3] unused for this Command

	compressed_metadata_t metadata = *compressed_metadata[index_number];
	uint32_t desired_address = (uint32_t)( (uint16_t*)(metadata.address) );
	uint32_t frame_index_start = (DATA_FRAME_SIZE - 10) * frame_number;
	uint32_t read_address = desired_address + frame_index_start;

	p_raw = (uint16_t*)(read_address);			// points to start of desired photo

	FillTxBufferWithZeroes();		// Fills Tx buffer with zeroes

	// Fill first 10 bytes with metadata - TODO: Check byte ordering to be consistent throughout. This would be LSB first, no? Seems inverted
	tx_buffer[0] = (uint8_t) (metadata.index);
	tx_buffer[1] = (uint8_t)((desired_address & 0x000000FF)      );
	tx_buffer[2] = (uint8_t)((desired_address & 0x0000FF00) >> 8 );
	tx_buffer[3] = (uint8_t)((desired_address & 0x00FF0000) >> 16);
	tx_buffer[4] = (uint8_t)((desired_address & 0xFF000000) >> 24);
	tx_buffer[5] = (uint8_t)((metadata.size & 0x000000FF)      );
	tx_buffer[6] = (uint8_t)((metadata.size & 0x0000FF00) >> 8 );
	tx_buffer[7] = (uint8_t)((metadata.size & 0x00FF0000) >> 16);
	tx_buffer[8] = (uint8_t)((metadata.size & 0xFF000000) >> 24);
	tx_buffer[9]  = (uint8_t)((metadata.timestamp & 0x000000FF)      );
	tx_buffer[10] = (uint8_t)((metadata.timestamp & 0x0000FF00) >> 8 );
	tx_buffer[11] = (uint8_t)((metadata.timestamp & 0x00FF0000) >> 16);
	tx_buffer[12] = (uint8_t)((metadata.timestamp & 0xFF000000) >> 24);

	// Fill remaining 100 Bytes with requested frame
	for(uint8_t i = 19; i < (DATA_FRAME_SIZE - 19); i+=2) {
		uint16_t value = *p_raw;
		tx_buffer[i] =   (value & 0x00FF);
		tx_buffer[i+1] = (value & 0xFF00) >> 8;
		p_raw += 1;	// increments read pointer
	}


	return HAL_OK;

}

HAL_StatusTypeDef CMD_TransmitFrameRaw(uint8_t *opcode) {
	uint8_t  buffer_number 	=  opcode[0];
	uint16_t frame_number 	= (opcode[2] << 8) | opcode[1];
	//opcode[3] unused for this Command

	p = raw_buffers[buffer_number];
	uint32_t frame_index_start = (DATA_FRAME_SIZE - 10) * frame_number;

	FillTxBufferWithZeroes();		// Fills Tx buffer with zeroes

	// Fill first 10 bytes with metadata - TODO: Check byte ordering to be consistent throughout. This would be LSB first, no? Seems inverted
	tx_buffer[0] = (uint8_t) (p->designator & 0x00FF)       ;
	tx_buffer[1] = (uint8_t)((p->designator & 0xFF00) >> 8);
	tx_buffer[2] = p->opcode[0];
	tx_buffer[3] = p->opcode[1];
	tx_buffer[4] = p->opcode[2];
	tx_buffer[5] = p->opcode[3];
	tx_buffer[6] = (uint8_t)((p->timestamp & 0x000000FF)      );
	tx_buffer[7] = (uint8_t)((p->timestamp & 0x0000FF00) >> 8 );
	tx_buffer[8] = (uint8_t)((p->timestamp & 0x00FF0000) >> 16);
	tx_buffer[9] = (uint8_t)((p->timestamp & 0xFF000000) >> 24);

	// Fill remaining 100 Bytes with requested frame
	for(uint8_t i = 19; i < (DATA_FRAME_SIZE - 19); i+=2) {
		tx_buffer[i] =   (p->data[frame_index_start + i] & 0x00FF);
		tx_buffer[i+1] = (p->data[frame_index_start + i] & 0xFF00) >> 8;
	}

	return HAL_OK;

}

HAL_StatusTypeDef CMD_MemoryState(uint8_t *opcode) {
	return HAL_OK;

}

HAL_StatusTypeDef CMD_EraseRawBuffer(uint8_t *opcode) {
	return HAL_OK;

}

HAL_StatusTypeDef CMD_EraseCompressedBuffer(uint8_t *opcode) {
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

HAL_StatusTypeDef CMD_DumpMemorySRAM(uint8_t *opcode) {
	return HAL_OK;

}

HAL_StatusTypeDef CMD_DumpMemoryFRAM(uint8_t *opcode) {
	return HAL_OK;

}

HAL_StatusTypeDef CMD_DumpPhoto(uint8_t *opcode) {
	return HAL_OK;

}


// TODO - other functions to implement


// ===== Command Table =====
const command_t command_table[NUM_COMMANDS] = {

    { "TAKE_PICTURE", 0x33, CMD_TakePicture, 			  	 			"Capture an image (tries N times, and can filter for good pictures) "
    										 	 	 	 	 	 	 	"and saves a copy to non-volatile buffer. Compresses the photo and saves it "
    										 	 	 	 	 	 	    "to volatile and non-volatile memory.", 1, 20000 },

	{ "TAKE_PICTURE_DELAYED", 0x34, CMD_TakePictureDelayed,  	 	    "Capture an image after a delay (tries N times, and can filter for good pictures) "
																		"and saves a copy to non-volatile buffer. Compresses the photo and saves it "
																		"to volatile and non-volatile memory.", 1, 24 * 60 * 1000 }, 	// TODO - See timeout for this one

    { "TRANSMIT_FRAME_COMPRESSED", 0x35, CMD_TransmitFrameCompressed, 	"Transmits a 110B frame of a compressed image with a certain index", 1, 20000 },

    { "TRANSMIT_FRAME_RAW", 0x36, CMD_TransmitFrameRaw, 			    "Transmits a 110B frame of a raw image in a certain buffer", 1, 20000 },

	{ "CURRENT_MEMORY_STATE", 0x37, CMD_MemoryState, 			    	"Transmits compressed image metadata to know how many images are saved and "
																		"how much free memory there is left in SpaceSnap", 0, 20000 },

	{ "ERASE_RAW_BUFFER", 0x38, CMD_EraseRawBuffer, 			    	"Erases desired raw photo buffer", 1, 20000 },

	{ "ERASE_COMPRESSED_BUFFER", 0x39, CMD_EraseCompressedBuffer, 		"Erases entire compressed photo buffer", 0, 20000 },

    // TODO - One function for each camera parameter we want to change!
	// Maybe commands to turn camera on/off?

	{ "GET_STATUS", 0x64, CMD_GetStatus, 								"Payload status - TODO", 0, 20000 },

    { "BACKUP_VOL_MEMORY", 0x65, CMD_BackupVolatileMemory, 				"Makes a copy of volatile memory to non-volatile memory for backup "
    																	"in case of power down.", 0, 20000 },

    { "RESET_PAYLOAD", 0x66, CMD_ResetPayload, 						    "Software reset for UNSAM SpaceSnap", 0, 20000 },

	{ "DUMP_MEMORY_SRAM", 0x67, CMD_DumpMemorySRAM, 					"Dumps SRAM memory through UART4", 0, 45000 },

	{ "DUMP_MEMORY_FRAM", 0x68, CMD_DumpMemoryFRAM, 					"Dumps FRAM memory through UART4", 0, 45000 },

	{ "DUMP_PHOTO", 0x69, CMD_DumpPhoto, 								"Takes and dumps photo data through UART4", 1, 20000 },

	// TODO - Any other command?

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
HAL_StatusTypeDef ExecuteCommand(const command_t *command, uint8_t *opcode)		// TODO - implement timeout
{
    if (!command) return HAL_ERROR;

    HAL_StatusTypeDef st = command->handler(opcode);
    if (st == HAL_ERROR) {
    	tx_buffer[0] = COMMAND_FAILURE;
    	return HAL_ERROR;
    }

    // TODO - Maybe implement the timeout here. How?

    if(command->takes_opcode){
    	char opcode_text[60];
    	snprintf(opcode_text, sizeof(opcode_text), "Command executed successfully with opcode %02x %02x %02x %02x", opcode[0], opcode[1], opcode[2], opcode[3]);
    	Log(opcode_text);
    }
    else {
    	Log("Command executed successfully");
    }

	tx_buffer[0] = COMMAND_SUCCESS;
    return HAL_OK;
}

void FillTxBufferWithZeroes(void)
{
	for(uint8_t i = 0; i < DATA_FRAME_SIZE; i++) {
		tx_buffer[i] = 0;
	}
}

