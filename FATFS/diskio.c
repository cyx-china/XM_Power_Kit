/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2025        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Basic definitions of FatFs */
#include "diskio.h"		/* Declarations FatFs MAI */

#include "cmsis_os2.h"
#include "Flash.h"
#include "os_handles.h"

/* Example: Declarations of the platform and disk functions in the project */

/* Example: Mapping of physical drive number for each drive */

#define SPI_FLASH		0	/* Map SPI_FLASH to physical drive 0 */



/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat;

	if (FLASH_ReadID() != 0) {
		stat = 0;
	} else {
		stat = STA_NOINIT;
	}
	return stat;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	return disk_status(pdrv);
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	DRESULT res;

	uint32_t addr = sector * 4096;
	uint32_t size = count * 4096;

	osMutexAcquire(Flash_MutexHandle, osWaitForever);

	if (FLASH_Read(addr, buff, size) == HAL_OK) {
		res = RES_OK;
	} else {
		res = RES_ERROR;
	}

	osMutexRelease(Flash_MutexHandle);

	return res;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	DRESULT res;

	uint32_t addr = sector * 4096;
	uint32_t size = count * 4096;

	osMutexAcquire(Flash_MutexHandle,osWaitForever);

	if (FLASH_Write(addr, (uint8_t *)buff, size) == HAL_OK) {
		res = RES_OK;
	} else {
		res = RES_ERROR;
	}

	osMutexRelease(Flash_MutexHandle);

	return res;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res;
	switch (cmd) {
		case CTRL_SYNC:  // Flush disk cache (for write functions)
			res = RES_OK;
			break;

		case GET_SECTOR_COUNT:  // Get media size (for only f_mkfs())
			*(DWORD*)buff = FLASH_SECTOR_COUNT;
			res = RES_OK;
			break;

		case GET_SECTOR_SIZE:  // Get sector size (for multiple sector size (_MAX_SS >= 1024))
			*(WORD*)buff = FLASH_SECTOR_SIZE;  // 4096 bytes
			res = RES_OK;
			break;

		case GET_BLOCK_SIZE:  // Get erase block size (for only f_mkfs())	// 表示一次擦除几个扇区，填1即可
			*(DWORD*)buff = 1;  // Erase block size in unit of sector (since sector is the erase unit)
			res = RES_OK;
			break;

		default:
			res = RES_PARERR;
			break;
	}

	return res;
}

