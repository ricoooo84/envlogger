#include "diskio.h"
#include "sd_driver.h"
#include <stdio.h>

extern sd_card_t sd;

DSTATUS disk_initialize (BYTE pdrv) {
	if (pdrv != 0) {
		return STA_NOINIT;
	}

	if (sd_init(&sd) != SD_OK) {
		return STA_NOINIT;
	}

	return 0;
}


DSTATUS disk_status (BYTE pdrv) {
	if (pdrv != 0) {
		return STA_NOINIT;
	}
	if (sd.initialized == 0) {
		return STA_NOINIT;
	}
	return 0;
}

DRESULT disk_read (BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
	if (pdrv != 0) {
		return RES_PARERR;
	}

	for (UINT i = 0; i < count; i++) {
		sd_status_t status = sd_read_block(&sd, sector + i, buff + 512*i);
		if (status != SD_OK) {
			return RES_ERROR;
		}
	}

	return RES_OK;
}

DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
	if (pdrv != 0) {
		return RES_PARERR;
	}

	for (UINT i = 0; i < count; i++) {
		sd_status_t status = sd_write_block(&sd, sector + i, buff + (512*i));
		if (status != SD_OK) {
			printf("disk_write FAIL sector=%lu status=%d\r\n", (unsigned long)(sector + i), status);
			return RES_ERROR;
		}
	}

	return RES_OK;
}
DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff) {
	if (pdrv != 0) {
		return RES_PARERR;
	}

	switch (cmd) {
		case CTRL_SYNC:
			// flushes buffer, already handled by sd_driver
			return RES_OK;
		case GET_SECTOR_COUNT:
			*(LBA_t *)buff = sd.block_count;
			return RES_OK;
		case GET_SECTOR_SIZE:
			// already hardcoded: FF_MAX_SS == FF_MIN_SS == 512
			return RES_OK;
		case GET_BLOCK_SIZE:
			*(DWORD *)buff = 1;
			return RES_OK;
		default:
			return RES_PARERR;
	}
}
