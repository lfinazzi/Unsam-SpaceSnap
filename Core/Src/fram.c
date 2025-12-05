/*
 * fram.c
 *
 *  Created on: Dec 5, 2025
 *      Author: finazzi, gagliardi
 */

#include "fram.h"
#include <string.h>
#include "assert.h"

/* FRAM commands */
#define FRAM_CMD_WREN   0x06
#define FRAM_CMD_WRITE  0x02
#define FRAM_CMD_READ   0x03
#define FRAM_CMD_SLEEP  0xB9

/* External SPI handle (CubeMX generated for SPI2) */
extern SPI_HandleTypeDef hspi2;

/* CS pin: PB12 */
#define FRAM_CS_PORT GPIOB
#define FRAM_CS_PIN  GPIO_PIN_12

/* Helper timeout */
#define FRAM_TIMEOUT_MS FRAM_SPI_TIMEOUT_MS

/* --- microsecond delay using DWT (if available) --- */
void FRAM_InitDelay(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = (SystemCoreClock / 1000000UL) * us;
    while ((DWT->CYCCNT - start) < ticks) { __NOP(); }
}

/* CS helpers using PB12 */
static inline void FRAM_SelectCS(void)
{
    HAL_GPIO_WritePin(FRAM_CS_PORT, FRAM_CS_PIN, GPIO_PIN_RESET);
}

static inline void FRAM_ReleaseCS(void)
{
    HAL_GPIO_WritePin(FRAM_CS_PORT, FRAM_CS_PIN, GPIO_PIN_SET);
}

/* optional small delay */
static inline void fram_optional_delay(bool delay)
{
    if (delay) delay_us(100); /* same as previous default */
}

/* --- write single byte to FRAM at address --- */
void wExtMem(uint32_t address, uint8_t dat, uint8_t cs, bool delay)
{
    uint8_t tx[5];

    tx[0] = FRAM_CMD_WRITE;
    tx[1] = (address >> 16) & 0xFF;
    tx[2] = (address >> 8) & 0xFF;
    tx[3] = address & 0xFF;
    tx[4] = dat;

    /* WREN */
    uint8_t wren = FRAM_CMD_WREN;
    FRAM_SelectCS();
    fram_optional_delay(delay);
    HAL_SPI_Transmit(&hspi2, &wren, 1, FRAM_TIMEOUT_MS);
    FRAM_ReleaseCS();

    /* WRITE */
    FRAM_SelectCS();
    fram_optional_delay(delay);
    HAL_SPI_Transmit(&hspi2, tx, sizeof(tx), FRAM_TIMEOUT_MS);
    FRAM_ReleaseCS();
}

/* --- read single byte from FRAM at address --- */
uint16_t rExtMem(uint32_t address, uint8_t cs, bool delay)
{
    uint8_t cmd[4];
    uint8_t rx;

    cmd[0] = FRAM_CMD_READ;
    cmd[1] = (address >> 16) & 0xFF;
    cmd[2] = (address >> 8) & 0xFF;
    cmd[3] = address & 0xFF;

    FRAM_SelectCS();
    fram_optional_delay(delay);
    HAL_SPI_Transmit(&hspi2, cmd, 4, FRAM_TIMEOUT_MS);
    HAL_SPI_Receive(&hspi2, &rx, 1, FRAM_TIMEOUT_MS);
    FRAM_ReleaseCS();

    return (uint16_t)rx;
}

/* --- toggle CS low briefly to wake FRAM (uExtMem) --- */
void uExtMem(uint8_t cs)
{
    FRAM_ReleaseCS();
    delay_us(50);
    FRAM_SelectCS();
    delay_us(200);
    FRAM_ReleaseCS();
}

/* --- send sleep command to FRAM --- */
void sExtMem(uint8_t cs)
{
    uint8_t sleep = FRAM_CMD_SLEEP;
    FRAM_SelectCS();
    HAL_SPI_Transmit(&hspi2, &sleep, 1, FRAM_TIMEOUT_MS);
    FRAM_ReleaseCS();
}

/* --- clear memory range (write 0) --- */
void cExtMem(uint8_t cs, uint32_t firstLocation, uint32_t lastLocation)
{
    uint32_t addr;
    uExtMem(cs);
    for (addr = firstLocation; addr <= lastLocation; addr++) {
        wExtMem(addr, 0x00, cs, false);
    }
    sExtMem(cs);
}

/* --- read/print region (pExtMem) - currently reads only; add UART prints if needed --- */
void pExtMem(uint8_t cs, uint32_t firstLocation, uint32_t lastLocation, uint32_t from, uint32_t to)
{
    uint32_t currentAddress;
    uExtMem(cs);

    if (from <= to) {
        for (currentAddress = from; currentAddress <= to && currentAddress <= lastLocation; currentAddress++) {
            (void) rExtMem(currentAddress, cs, false);
            HAL_Delay(1);
        }
    } else {
        for (currentAddress = from; currentAddress <= lastLocation; currentAddress++) {
            (void) rExtMem(currentAddress, cs, false);
            HAL_Delay(1);
        }
        for (currentAddress = firstLocation; currentAddress <= to; currentAddress++) {
            (void) rExtMem(currentAddress, cs, false);
            HAL_Delay(1);
        }
    }
    sExtMem(cs);
}

/* --- move data from one FRAM to another (if you have two, but this driver only controls one CS pin) --- */
void mExtMem(uint8_t from_cs, uint8_t to_cs, uint16_t n, uint32_t fromAddress, uint32_t toAddress)
{
    uint16_t i = 0;
    uint8_t data;
    uExtMem(from_cs);
    uExtMem(to_cs);

    while (i < n) {
        data = (uint8_t) rExtMem(fromAddress, from_cs, false);
        wExtMem(toAddress, data, to_cs, false);
        fromAddress++;
        toAddress++;
        i++;
    }
}

/* --- write array of bytes to FRAM sequentially --- */
bool wExtMem_DataSet(uint32_t address, uint8_t* dat, uint8_t len, uint8_t cs, bool delay)
{
    uint8_t i;
    uExtMem(cs);

    for (i = 0; i < len; i++) {
        /* keep previous policy: write if not zero or index==4, otherwise still write if you prefer */
        if (dat[i] != 0 || i == 4) {
            wExtMem(address, dat[i], cs, delay);
            address++;
        } else {
            /* if skipping zero, still increment address to preserve sequence */
            address++;
        }
    }
    return true;
}


/*
	uint16_t photos_taken;
	uint32_t total_frames_sent;
	uint32_t time_on;
	uint8_t camera_regs_A[20];		// Camera registers to write
	uint8_t *camera_reg_B[20];
	uint8_t camera_params_A[20];	// Parameters to write them with
	uint8_t *camera_params_B[20];
*/
void Init_status(status_t *status)
{
	uint16_t val16 = 0;
	uint16_t val32 = 0;

	val16 = rExtMem(START_ADDR_FRAM, 0, 1);		// Delay on


	static_assert(sizeof(status_t) == 92);

}

void UpdateStatus(status_t *status)
{

}
