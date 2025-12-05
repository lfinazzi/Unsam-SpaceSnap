/*
 * fram.h
 *
 *  Created on: Dec 5, 2025
 *      Author: finazzi, gagliardi
 */

#ifndef __FRAM_H__
#define __FRAM_H__


#include "stdint.h"
#include "stdbool.h"
#include "stm32f2xx_hal.h"
#include "photo.h"

/* Timeouts */
#ifndef FRAM_SPI_TIMEOUT_MS
	#define FRAM_SPI_TIMEOUT_MS 100
#endif

typedef struct {
	uint16_t photos_taken;
	uint32_t total_frames_sent;
	uint32_t time_on;
	uint8_t camera_regs_A[20];		// Camera registers to write
	uint8_t camera_reg_B[20];
	uint8_t camera_params_A[20];	// Parameters to write them with
	uint8_t camera_params_B[20];
} status_t;

/* ============================================================
 *                     INITIALIZATION
 * ============================================================ */

/**
 * @brief  Initialize the DWT cycle counter for microsecond delays.
 * @note   Must be called once before any FRAM operation that uses
 *         microsecond timing (uExtMem, internal timing delays).
 * @note   Requires that DWT exists (Cortex-M3/M4).
 */
void FRAM_InitDelay(void);

/* ============================================================
 *                      FRAM OPERATIONS
 * ============================================================ */

/**
 * @brief  Write a single byte into FRAM.
 * @param  address  24-bit FRAM address to write.
 * @param  dat      Byte to write.
 * @param  cs       Chip-select ID (ignored, kept for compatibility).
 * @param  delay    If true, adds ~100 µs timing delay before transfer.
 */
void wExtMem(uint32_t address, uint8_t dat, uint8_t cs, bool delay);

/**
 * @brief  Read a single byte from FRAM.
 * @param  address  24-bit FRAM address to read.
 * @param  cs       Chip-select ID (ignored).
 * @param  delay    Add timing delay before transfer.
 * @return uint16_t Contains the read byte in LSB.
 */
uint16_t rExtMem(uint32_t address, uint8_t cs, bool delay);

/**
 * @brief  Wake FRAM from sleep/standby by toggling CS low briefly.
 * @param  cs  Chip-select ID (ignored).
 * @note   Required before some sequences (migrated from TMS implementation).
 */
void uExtMem(uint8_t cs);

/**
 * @brief  Put the FRAM into deep sleep / low-power mode.
 * @param  cs   Chip-select ID (ignored).
 * @note   Device must be awakened with uExtMem() before next access.
 */
void sExtMem(uint8_t cs);

/**
 * @brief  Clear (erase to zero) a range of FRAM addresses.
 * @param  cs            Chip-select (ignored).
 * @param  firstLocation Start address (inclusive).
 * @param  lastLocation  End address (inclusive).
 */
void cExtMem(uint8_t cs, uint32_t firstLocation, uint32_t lastLocation);

/**
 * @brief  Iterate through a region of memory (optional for debugging).
 * @param  cs      Chip-select ID (ignored).
 * @param  first   First address of valid FRAM region.
 * @param  last    Last address of valid FRAM region.
 * @param  from    Start address to read.
 * @param  to      End address to read.
 * @note   Wraps around if from > to (kept from original TMS behavior).
 * @note   Currently does not print—reading only. Printing can be added.
 */
void pExtMem(uint8_t cs, uint32_t firstLocation, uint32_t lastLocation,
             uint32_t from, uint32_t to);

/**
 * @brief  Copy bytes from one FRAM chip to another.
 * @param  from_cs       Source CS (ignored here).
 * @param  to_cs         Destination CS (ignored here).
 * @param  n             Number of bytes to copy.
 * @param  fromAddress   Source address.
 * @param  toAddress     Destination address.
 * @note   Supported for compatibility; both CS map to same FRAM.
 */
void mExtMem(uint8_t from_cs, uint8_t to_cs, uint16_t n,
             uint32_t fromAddress, uint32_t toAddress);

/**
 * @brief  Write a sequence of bytes to FRAM sequentially.
 * @param  address  Starting FRAM address.
 * @param  dat      Pointer to data buffer.
 * @param  len      Number of bytes to write.
 * @param  cs       Chip-select ID (ignored).
 * @param  delay    If true, adds timing delay.
 * @return true     Always returns true (compatibility).
 * @note   Behavior mimics original TMS fram.c:
 *         skips writing zero bytes except index 4 unless modified.
 */
bool wExtMem_DataSet(uint32_t address, uint8_t* dat, uint8_t len,
                     uint8_t cs, bool delay);


/**********************************************************
 * Loads the currently saved status_t in FRAM memory to
 * system paramsters
 **********************************************************/
void Init_status(status_t *status);


/**********************************************************
 * Saves the current system settings to status_t global
 * variable
 **********************************************************/
void UpdateStatus(status_t *status);

#endif /* __FRAM_H__ */
