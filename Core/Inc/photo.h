/*
 * photo.h - photo buffer definition and image handling functions
 *
 *  Created on: Nov 27, 2025
 *      Author: finazzi
 */

#ifndef __PHOTO_H__
#define __PHOTO_H__

//#include <stdint.h> in jpeg.h
#include "dcmi.h"
#include "jpeg.h"
#include "command.h"

#define TJE_IMPLEMENTATION													// adds compression library

#define H 							  	 (640U)								// Horizontal resolution
#define L 							  	 (480U)								// Vertical resolution


// I2C device addresses (10-bit)
#ifndef CAM_A_I2C_ADDR
	#define CAM_A_I2C_ADDR  		 	 (0x90)  // replace with your sensor address (7-bit typical)
#endif

#ifndef CAM_B_I2C_ADDR
	#define CAM_B_I2C_ADDR  		 	 (0xBA)  // if used; adjust as required
#endif

#define I2C_TIMEOUT_MS  			  	 (50U)

// ------------------------ USS GPIO definitions ----------------------
#define CAM_A_GPIO_PIN_EN			  	 (2U)
#define CAM_B_GPIO_PIN_EN			  	 (3U)
#define CAM_GPIO_I2C_EN				  	 (11U)
#define CAM_GPIO_POW_EN				  	 (12U)

// ----------------------------- SRAM Memory ---------------------------
#define RAW_PHOTO_BASE_ADDRESS 		  	 (0x60000000U)							// NOR SRAM BANK 1
#define RAW_PHOTO_BYTE_SIZE 		  	 (2*H*L)								// in bytes
#define DCMI_NUM_TRANSFERS				 (RAW_PHOTO_BYTE_SIZE / 4U)
#define NUM_BUFFERS 				  	 (3U)
#define RAW_METADATA_SIZE 			     (10U)
#define RAW_PHOTO_SIZE					 (RAW_PHOTO_BYTE_SIZE) + (RAW_METADATA_SIZE)


typedef struct {					  // all 15b variables to avoid struct padding
	uint16_t designator;			  // global raw photo number taken
	uint16_t opcode[2]; 			  // opcodes sent to take picture 	- TODO: Define in MACRO
	uint32_t timestamp;			      // timestamp is uint32_t
	uint16_t data[L*H];               // Image data in YCbCr 4:2:2 format
} raw_photo_t;

typedef struct {
	uint8_t index;					  // index of compressed photo
	uint16_t *address;			  	  // memory address start for picture
	uint32_t size;				 	  // size of compressed photo
	uint32_t timestamp;				  // internal timestamp
	uint16_t opcode[2];				  // instruction + opcode, saved in 16b to avoid padding
} compressed_metadata_t;

#define MAX_COMPRESSED_PICS 			 (100U)
#define COMPRESSED_METADATA_SIZE		 (10U)
#define COMPRESSED_METADATA_BASE_ADDR 	 (RAW_PHOTO_BASE_ADDRESS) + ( NUM_BUFFERS*RAW_PHOTO_SIZE )
#define COMPRESSED_DATA_BASE_ADDR 	     (COMPRESSED_METADATA_BASE_ADDR) + (MAX_COMPRESSED_PICS * sizeof(compressed_metadata_t))

#define END_OF_MEMORY 				  	 (COMPRESSED_DATA_BASE_ADDR) + (0x7A120000U)   // Start: 0x6000_0000, Capacity: 2048M 16b addresses

// ----------------------------- FRAM Memory ---------------------------
#define START_ADDR_FRAM  				 (0x0)

// ------------------------- Calculation constants ---------------------
#define BLACK_THRESHOLD_UNITS 			 (0.0079f)						// Default Y threshold for identifying black pixels
#define DEFAULT_BLACK_THRESHOLD 		 (0.2f)						// Max allowed percentage of black pixels in an image


// Raw photo buffers
extern volatile raw_photo_t* raw_buffer_1;
extern volatile raw_photo_t* raw_buffer_2;
extern volatile raw_photo_t* raw_buffer_3;
extern volatile raw_photo_t* raw_buffers[NUM_BUFFERS];

// Metadata for compressed photo buffer
extern volatile compressed_metadata_t* compressed_metadata[MAX_COMPRESSED_PICS];
extern          uint16_t* compressed_photo_space;
extern          uint16_t* current_compressed_address;
extern 			uint8_t current_compressed_index;

extern volatile compressed_metadata_t* compressed_metadata_FRAM[MAX_COMPRESSED_PICS];
extern volatile uint16_t* compressed_photo_space_FRAM;

extern volatile raw_photo_t* p;					// helper pointer to raw photo
extern volatile uint16_t* p_raw;				// helper pointer to compressed memory space
extern volatile uint8_t frame_done;
extern volatile uint16_t raw_photo_number_global;


/**********************************************************
 * Function to initialize parameters for both cameras
 * in Unsam SpaceSnap
 **********************************************************/
HAL_StatusTypeDef Camera_Init(void);

/**********************************************************
 * Function to configure camera A or B with desired
 * parameter configurations. Needs to be called each time
 * Camera is turned on. Relevant configurations:
 * 		- YCbCr 4:2:2 output (16 bit per pixel)
 * 		- 8 bit output
 * 		TODO - Are there other configurations?
 **********************************************************/
HAL_StatusTypeDef CameraConfig(uint8_t camera);

/**********************************************************
 * Function to enable camera A and disable camera B
 **********************************************************/
void ActivateCameraA(void);

/**********************************************************
 * Function to enable camera B and disable camera A
 **********************************************************/
void ActivateCameraB(void);

/**********************************************************
 * Auxilliary function to write 16b value to 16b reg
 * through i2C
 **********************************************************/
HAL_StatusTypeDef cam_write_reg16_uint16(uint8_t camera, uint16_t reg16, uint16_t val16);

/**********************************************************
 * Auxilliary function to red from 16b reg to 16b buffer
 * through i2C
 **********************************************************/
HAL_StatusTypeDef cam_read_reg16_uint16(uint8_t camera, uint16_t reg16, uint16_t *out16);

/**********************************************************
 * Callback function for DMA transfer of raw image
 **********************************************************/
void HAL_DCMI_XferCpltCallback(DMA_HandleTypeDef *hdma);

/**********************************************************
 * Arms DCMI capture of a single frame before requesting
 * it to the sensor through i2C and saves that frame into
 * the corresponding buffer number and the corresponding
 * frame index in that buffer
 **********************************************************/
HAL_StatusTypeDef DCMICapture(uint8_t camera_number, uint8_t buffer_number, uint8_t *opcode);

/**********************************************************
 * Computes the percentage of black pixels in the image
 * stored in the raw photo buffer, given a Y threshold and
 * saves the result for later filtering. Uses Y value
 * (brightness) only from YCbCr 4:2:2 format.
 **********************************************************/
void ComputeBlackPercentage(float *result, uint8_t buffer);

/**********************************************************
 * Compresses raw image data from specified buffer to JPEG
 * format and stores it in compressed photo buffer area.
 * Returns the size of compressed data in bytes.
 * 
 * Parameters:
 *   - buffer_number: index of raw photo buffer (0-2)
 *   - quality: JPEG compression quality (1-3)
 *              1 = good, 2 = standard, 3 = poor
 *   - compressed_size: pointer to store resulting size
 **********************************************************/
HAL_StatusTypeDef CompressToJPEG(uint8_t buffer_number, uint8_t quality, uint32_t *compressed_size, uint8_t *opcode);

/**********************************************************
 * Allocates memory for raw photo buffers
 **********************************************************/
void init_camera_buffers(void);

#endif /* __PHOTO_H__ */



