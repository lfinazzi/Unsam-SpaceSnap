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

volatile uint8_t frame_done = 0;
volatile uint32_t compressed_photo_buffer_address_V = COMPRESSED_PHOTO_BASE_ADDRESS;
volatile uint32_t compressed_photo_buffer_address_NV = COMPRESSED_PHOTO_BASE_ADDRESS_NV;

void Camera_Init(void)
{
	HAL_GPIO_WritePin(GPIOA, CAM_GPIO_I2C_EN, GPIO_PIN_SET);			// Enable i2C transveicer
	HAL_GPIO_WritePin(GPIOA, CAM_GPIO_POW_EN, GPIO_PIN_SET);			// Enable Camera power domains
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

HAL_StatusTypeDef poll_command_done(uint8_t camera, uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    uint16_t cmd;
    while (HAL_GetTick() - t0 < timeout_ms) {
        if (cam_read_reg16_uint16(camera, REG_COMMAND_REGISTER, &cmd) != HAL_OK) return HAL_ERROR;
        if ((cmd & 0x8000U) == 0) { // doorbell (bit15) cleared
            return HAL_OK;
        }
        HAL_Delay(5);
    }
    return HAL_TIMEOUT;
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
	frame_done = 1;
}

HAL_StatusTypeDef DCMICapture(uint8_t camera_number, uint8_t buffer_number)
{
	//HAL_StatusTypeDef st;
	frame_done = 0;

	// DCMI capture - frame_done = 1

	frame_done = 0;	// TODO - add a footer with image metadata

	return HAL_OK;
}

void ComputeBlackPercentage(float *result, uint8_t buffer)
{
    uint32_t total_pixels = (uint32_t)(L * H);
    uint32_t black_pixels = 0;

    // Pointer to image in external SRAM
    volatile uint16_t *p = (volatile uint16_t *)(IMAGE_BASE_ADDR + buffer * RAW_PHOTO_BYTE_SIZE);

    // In YCbCr 4:2:2, 4 bytes = 2 pixels:  Y0 Cb  Y1 Cr
    // So for each pixel:
    //   Y = (p & 0xFF00) >> 8	- TODO: check that Y is MSB and format of camera output

    for (uint32_t i = 0; i < total_pixels; i+=2) {	// increments by 2 because each pixel is saved in two B memory addresses
        uint16_t y = (p[i] & 0xFF00) >> 8;		// First pixel Y

        if (y < DEFAULT_Y_THRESHOLD) black_pixels++;

    }

    *result = (float)black_pixels * 100.0f / (float)total_pixels;
}

HAL_StatusTypeDef CompressToJPEG(uint8_t buffer_number, uint8_t quality, uint32_t *compressed_size)
{
	// Validate input parameters
	if (buffer_number >= NUM_BUFFERS || quality < 1 || quality > 3) {
		return HAL_ERROR;
	}

	// Pointer to raw image in external SRAM (YCbCr 4:2:2 format)
	volatile uint32_t *raw_data = (volatile uint32_t *)(RAW_PHOTO_BASE_ADDRESS + buffer_number * RAW_PHOTO_BYTE_SIZE);

	// Destination address for compressed data
	volatile uint32_t *compressed_dest = (volatile uint32_t *)compressed_photo_buffer_address_V;

	// Calculate available buffer size for compression
	// (Total SRAM - space used by raw buffers)
	uint32_t available_buffer_size = 0x100000U - (NUM_BUFFERS * RAW_PHOTO_BYTE_SIZE);	// TODO - check this calculation

	// Call JPEG encoder
	// Note: raw_data is in YCbCr 4:2:2 format, which tje_encode_to_memory expects
	int result = tje_encode_to_memory(
		(uint8_t *)compressed_dest,			// how does this work? This is a uint32_t! Type cast unsafe. TODO
		available_buffer_size,
		compressed_size,
		quality,
		H,  // width = 640
		L,  // height = 480
		3,  // num_components = 3 for YCbCr
		(const unsigned char *)raw_data
	);

	if (result == 0) {
		// Compression failed
		*compressed_size = 0;
		return HAL_ERROR;
	}

	// Update compressed photo buffer address for next compression
	compressed_photo_buffer_address_V += *compressed_size;

	return HAL_OK;
}
