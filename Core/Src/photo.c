/*
 * photo.c - photo buffer definition and image handling functions
 *
 *  Created on: Nov 27, 2025
 *      Author: finazzi
 */

#define TJE_IMPLEMENTATION // for jpeg.h

#include "photo.h"
#include "dcmi.h"
#include "fsmc.h"
#include "i2c.h"
#include "tim.h"
#include <stdio.h>
#include "ls_comms.h"

volatile raw_photo_t* p;					// helper pointer for raw photos
volatile uint16_t* p_raw;					// helper pointer for compressed memory space
volatile uint8_t frame_done = 0;

HAL_StatusTypeDef Camera_Init(void)
{
	HAL_GPIO_WritePin(GPIOA, CAM_GPIO_I2C_EN, GPIO_PIN_SET);			// Enable i2C transveicer
	HAL_GPIO_WritePin(GPIOA, CAM_GPIO_POW_EN, GPIO_PIN_SET);			// Enable Camera power domains

	return HAL_OK; // TODO
}

HAL_StatusTypeDef CameraConfig(uint8_t camera)	// TODO
{
	// camera addresses:
	// A: 0x90
	// B: 0xBA
	// transfers done 8b at a time, MSB FIRST
	// TODO - camera configuration. This needs to be called every time the camera is turned on!

	//HAL_StatusTypeDef st;

	return HAL_OK;
}

void ActivateCameraA(void)
{
	HAL_GPIO_WritePin(GPIOA, CAM_A_GPIO_PIN_EN, GPIO_PIN_SET);			// Activate Camera A
	HAL_GPIO_WritePin(GPIOA, CAM_B_GPIO_PIN_EN, GPIO_PIN_RESET);		// Ensure Camera B is off
	CameraConfig(0);													// Configures Camera A
}

void ActivateCameraB(void)
{
	HAL_GPIO_WritePin(GPIOA, CAM_B_GPIO_PIN_EN, GPIO_PIN_SET);			// Activate Camera B
	HAL_GPIO_WritePin(GPIOA, CAM_A_GPIO_PIN_EN, GPIO_PIN_RESET);		// Ensure Camera A is off
	CameraConfig(1);													// Configures Camera B
}

HAL_StatusTypeDef cam_write_reg16_uint16(uint8_t camera, uint16_t reg16, uint16_t val16)
{
	uint8_t buf[4];
	buf[0] = (uint8_t)( (reg16 >> 8) & 0xFF );	// reg  MSB
	buf[1] = (uint8_t)( (reg16     ) & 0xFF );	// reg  LSB
	buf[2] = (uint8_t)( (val16 >> 8) & 0xFF );	// data MSB
	buf[3] = (uint8_t)( (val16     ) & 0xFF );	// data LSB

	uint16_t addr = (camera == 0) ? CAM_A_I2C_ADDR : (camera == 1) ? CAM_B_I2C_ADDR : 0;
	if (!addr) return HAL_ERROR;

	return HAL_I2C_Master_Transmit(&hi2c2, addr, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef cam_read_reg16_uint16(uint8_t camera, uint16_t reg16, uint16_t *out16)
{
	uint8_t reg_buf[2];
	reg_buf[0] = (uint8_t)( (reg16 >> 8) & 0xFF );
	reg_buf[1] = (uint8_t)( (reg16     ) & 0xFF );

	HAL_StatusTypeDef st;
	uint16_t addr = (camera == 0) ? CAM_A_I2C_ADDR : (camera == 1) ? CAM_B_I2C_ADDR : 0;
	if (!addr) return HAL_ERROR;

	// sends the 16b register address (no stop) and then performs a 16b read
	st = HAL_I2C_Master_Transmit(&hi2c2, addr, reg_buf, 2, I2C_TIMEOUT_MS);
	if(!HAL_OK) return st;

	uint8_t val_buf[2];
	st = HAL_I2C_Master_Receive(&hi2c2, addr, val_buf, 2, I2C_TIMEOUT_MS);
	if(!HAL_OK) return st;

	*out16 = (uint16_t)( val_buf[0] << 8 | val_buf[1] );
	return HAL_OK;
}

void HAL_DCMI_XferCpltCallback(DMA_HandleTypeDef *hdma)
{
	// DMA finished exact expected transfer
	HAL_DCMI_Stop(&hdcmi);
	HAL_TIM_PWM_Stop(&htim11, TIM_CHANNEL_1);		// Stops EXT_CLK for sensor
	frame_done = 1;   								// signal to main loop
}

HAL_StatusTypeDef DCMICapture(uint8_t camera_number, uint8_t buffer_number, uint8_t *opcode)
{
	frame_done = 0;

	p = raw_buffers[buffer_number];

	HAL_TIM_PWM_Start(&htim11, TIM_CHANNEL_1);		// Starts EXT_CLK for sensor

	// Flush previous flags
	__HAL_DCMI_DISABLE_IT(&hdcmi, DCMI_IT_FRAME);
	__HAL_DCMI_CLEAR_FLAG(&hdcmi, DCMI_FLAG_FRAMERI);

	// Enable frame interrupt
	__HAL_DCMI_ENABLE_IT(&hdcmi, DCMI_IT_FRAME);

	// Start DMA transfer in CONTINUOUS mode
	// DMA copies from DCMI_DR -> frame buffer
	if (HAL_DCMI_Start_DMA(&hdcmi,
						   DCMI_MODE_SNAPSHOT,					// We don't want video, just photo
						   (uint32_t)&(p->data),				// address of destination
						   (H * L) / 2) != HAL_OK) 				// Transfers two pixels at a time (transfer total: 32b)
	{
		tx_buffer[0] = DCMI_CAPTURE_ERR;						// Execution failed due to DCMI capture problem
		return HAL_ERROR;
	}


	while(!frame_done) {
		// wait for capture to end - TODO: Timeout?
	}

	// saves metadata after saving photo
	p->designator = raw_photo_number_global;
	raw_photo_number_global++;					// increments the counter by one. TODO - Save this change in FRAM

	uint16_t opcode0 = (opcode[1] << 8) | opcode[0];
	uint16_t opcode1 = (opcode[3] << 8) | opcode[2];
	p->timestamp = timestamp;
	p->opcode[0] = opcode0;	// LSB
	p->opcode[1] = opcode1;	// MSB

	return HAL_OK;
}

void ComputeBlackPercentage(float *result, uint8_t buffer)
{
    uint32_t total_pixels = (uint32_t)(L * H);
    uint32_t black_pixels = 0;

    // Pointer to image in external SRAM
    p = raw_buffers[buffer];

    // In YCbCr 4:2:2, 4 bytes = 2 pixels:  Y0 Cb  Y1 Cr
    // So for each pixel:
    //   Y = (p & 0xFF00) >> 8	- TODO: check that Y is MSB and format of camera output

    for (uint32_t i = 0; i < total_pixels; i++) {			// increments by 2 because each pixel is saved in 2B memory addresses
        uint16_t y = (p->data[i] & 0xFF00) >> 8;		    // address mask (MSB)

        if (y < DEFAULT_BLACK_THRESHOLD) black_pixels++;
        // TODO - could improve speed if break after it already reaches max allowed black pixels

    }

    *result = (float)black_pixels / (float)total_pixels;
}

HAL_StatusTypeDef CompressToJPEG(uint8_t buffer_number, uint8_t quality, uint32_t *compressed_size, uint8_t *opcode)
{
	// Validate input parameters
	if (buffer_number >= NUM_BUFFERS || quality < 1 || quality > 3) {
		return HAL_ERROR;
	}

	// Pointer to raw image in external SRAM (YCbCr 4:2:2 format)
	p = raw_buffers[buffer_number];

	// Calculate available buffer size for compression
	// (Total SRAM from compressed photos - space already used)
	uint32_t available_buffer_size = 2 * (END_OF_MEMORY - (uint32_t)&current_compressed_address); 	// this is in bytes

	// TODO - If available_buffer_size is less than compressed size, we need to handle this. We need to return to start of space (FIFO)

	// Call JPEG encoder
	// Note: raw_data is in YCbCr 4:2:2 format, which tje_encode_to_memory expects
	int result = tje_encode_to_memory(
		(uint8_t*)current_compressed_address,   // TODO: IMPORTANT! Check this casting
		available_buffer_size,
		compressed_size,
		quality,
		H,  // width = 640
		L,  // height = 480
		3,  // num_components = 3 for YCbCr
		(const unsigned char *)p->data			// TODO: Check this casting
	);

	if (result == 0) {
		// Compression failed
		*compressed_size = 0;
		return HAL_ERROR;
	}

	uint16_t opcode0 = (opcode[1] << 8) | opcode[0];
	uint16_t opcode1 = (opcode[3] << 8) | opcode[2];

	// saves metadata before saving raw image
	compressed_metadata[current_compressed_index]->index     = current_compressed_index;
	compressed_metadata[current_compressed_index]->address   = current_compressed_address;
	compressed_metadata[current_compressed_index]->size      = *compressed_size;
	compressed_metadata[current_compressed_index]->timestamp = timestamp;
	compressed_metadata[current_compressed_index]->opcode[0] = opcode0;	// LSB
	compressed_metadata[current_compressed_index]->opcode[1] = opcode1;	// MSB

	// Update compressed photo buffer address for next compression
	current_compressed_address += *compressed_size; 		// Saves compressed photo in current write address
	current_compressed_index += 1;							// increments the memory pointer by one

	return HAL_OK;
}

void init_camera_buffers(void)
{
	// Raw photo buffers
    raw_buffer_1 = (raw_photo_t*)(RAW_PHOTO_BASE_ADDRESS);
    raw_buffer_2 = (raw_photo_t*)(RAW_PHOTO_BASE_ADDRESS + RAW_PHOTO_SIZE);
    raw_buffer_3 = (raw_photo_t*)(RAW_PHOTO_BASE_ADDRESS + 2*RAW_PHOTO_SIZE);

    raw_buffers[0] = raw_buffer_1;
    raw_buffers[1] = raw_buffer_2;
    raw_buffers[2] = raw_buffer_3;

    // compressed metadata buffer - SRAM
    for (int i = 0; i < MAX_COMPRESSED_PICS; i++) {
		compressed_metadata[i] = (compressed_metadata_t*)(COMPRESSED_METADATA_BASE_ADDR + i * sizeof(compressed_metadata_t));
	}

    compressed_photo_space = (uint16_t*)( COMPRESSED_METADATA_BASE_ADDR + MAX_COMPRESSED_PICS * sizeof(compressed_metadata_t) );
    current_compressed_address = &compressed_photo_space[0];
    current_compressed_index = 0;

    // TX buffer for Payload response to LS-02
    for(size_t i = 0; i < DATA_FRAME_SIZE; i++)		// tx_buffer init
    	tx_buffer[i] = 0;

    // TODO - See if FRAM needs to be initialized or just treated as an SPI interface

}

