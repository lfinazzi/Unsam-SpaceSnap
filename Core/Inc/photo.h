/*
 * photo.h - photo buffer definition and image handling functions
 *
 *  Created on: Nov 27, 2025
 *      Author: finazzi
 */

#ifndef __PHOTO_H__
#define __PHOTO_H__

#include <stdint.h>
#include "dcmi.h"

#define H 							  (640U)								// Horizontal resolution
#define L 							  (480U)								// Vertical resolution

// --------- Register / constant definitions (from Developer Guide) ----------
#define REG_COMMAND_REGISTER          0x0040U   // SYSCTL command register (doorbell)
#define REG_CMD_HANDLER_PARAMS_POOL0  0xFC00U   // command handler params pool (parameter 0)
#define REG_CMD_HANDLER_PARAMS_POOL1  0xFC02U

// CamControl variables/regs (from Table 15)
#define REG_CAM_FRAME_SCAN_CONTROL    0xC858U   // cam_frame_scan_control
#define REG_CAM_OUTPUT_FORMAT         0xC96CU   // cam_output_format

// Zoom/pan & windowing (example)
#define REG_CAM_SENSOR_CFG_X_START    0xC802U
#define REG_CAM_SENSOR_CFG_X_END      0xC806U
#define REG_CAM_SENSOR_CFG_Y_START    0xC800U
#define REG_CAM_SENSOR_CFG_Y_END      0xC804U

// Helpful presets shown in DevGuide (enable parallel progressive)
#define VAR_NTSC_PRESET_REG           0x9426U   // preset variable region used in examples
#define VAR_PAL_PRESET_REG            0x9826U

// SYSCTL slew / pad control (examples; adapt if you want other slew)
#define REG_SLEW_DOUT                 0x0030U   // SYSCTL / pad slew (example register shown in DG)
#define REG_PIXCLK_POLARITY           0x0016U   // example: PIXCLK polarity control bit(s)

// I2C device addresses (10-bit)
#ifndef CAM_A_I2C_ADDR
	#define CAM_A_I2C_ADDR  		  (0x90)  // replace with your sensor address (7-bit typical)
#endif

#ifndef CAM_B_I2C_ADDR
	#define CAM_B_I2C_ADDR  		  (0xBA)  // if used; adjust as required
#endif

#define I2C_TIMEOUT_MS  			  (50U)
#define DOORBELL_TIMEOUT_MS			  (100U)

// Command register bits (per Host Command Interface)
#define COMMAND_DOORBELL_BIT   		  (1U<<15)   // bit 15 = doorbell when set by host
#define COMMAND_HOSTCMD_MASK   		  0x00FFU    // lower bits contain command result/status (check DG)

// ------------------------ USS GPIO definitions ----------------------
#define CAM_A_GPIO_PIN_EN			  (2U)
#define CAM_B_GPIO_PIN_EN			  (3U)
#define CAM_GPIO_I2C_EN				  (11U)
#define CAM_GPIO_POW_EN				  (12U)

// ----------------------------- SRAM Memory ---------------------------
#define RAW_PHOTO_BASE_ADDRESS 		  (0x60000000U)						// NOR SRAM BANK 1
#define RAW_PHOTO_BYTE_SIZE 		  (2*H*L)								// in bytes
#define NUM_TRANSFERS				  (RAW_PHOTO_BYTE_SIZE / 4U)
#define NUM_BUFFERS 				  (3U)
#define COMPRESSED_PHOTO_BASE_ADDRESS (RAW_PHOTO_BASE_ADDRESS) + (NUM_BUFFERS*RAW_PHOTO_BYTE_SIZE) 			// 16b data - TODO: add metadata

#define COMPRESSED_PHOTO_BASE_ADDRESS_NV  (0x0)

#define DEFAULT_Y_THRESHOLD 			  (40U)							// Default Y threshold for identifying black pixels
#define MAX_ALLOWED_BLACK_PERCENTAGE 	  (50f)						// Max allowed percentage of black pixels in an image

extern volatile uint8_t frame_done;
extern volatile uint32_t compressed_photo_buffer_address_V;		// current address for saving compressed pictures into SRAM memory (volatile)
extern volatile uint32_t compressed_photo_buffer_address_NV;	// current address for saving compressed pictures into FRAM memory (non-volatile)

// Will probably skip the usage of these structs in final implementation
typedef struct {	uint16_t data[L*H];               // Image data in YCbCr 4:2:2 format
	uint8_t  cam_number;			  // Number of camera with which picture was taken
	uint32_t photo_timestamp;		  // photo timestamp
	uint8_t  compression; 			  // compression quality (0 raw, 1 good, 2 standard, 3 poor)

	uint8_t  index; 				  // index in volatile memory
} raw_photo_t;

typedef struct {
	raw_photo_t raw_buffer[3];
} raw_photo_buffer_t;

typedef struct {
	uint16_t *data;	                  // Image data in YCbCr 4:2:2 format
	uint8_t  cam_number;			  // Number of camera with which picture was taken
	uint32_t photo_timestamp;		  // photo timestamp
	uint8_t  compression; 			  // compression quality (0 raw, 1 good, 2 standard, 3 poor)
} compressed_photo_t;

typedef struct {
	compressed_photo_t *photos;
	uint8_t current_index;
} compressed_photo_buffer_t;


/**********************************************************
 * Function to initialize parameters for both cameras
 * in Unsam SpaceSnap
 **********************************************************/
void Camera_Init(void);

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
 * Auxilliary function to poll command register doorbell
 * bit cleared (bit 15 is doorbell in cmd register example)
 **********************************************************/
HAL_StatusTypeDef poll_command_done(uint8_t camera, uint32_t timeout_ms);

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
HAL_StatusTypeDef DCMICapture(uint8_t camera_number, uint8_t buffer_number);

/**********************************************************
 * Computes the percentage of black pixels in the image
 * stored in the raw photo buffer, given a Y threshold and
 * saves the result for later filtering. Uses Y value
 * (brightness) only from YCbCr 4:2:2 format.
 **********************************************************/
void ComputeBlackPercentage(float *result, uint8_t buffer);

#endif /* __PHOTO_H__ */



