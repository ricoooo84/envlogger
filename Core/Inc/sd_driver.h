#ifndef INC_SD_DRIVER_H_
#define INC_SD_DRIVER_H_

#include "stdint.h"
#include "stm32g0xx.h"

#define SD_RETURN_STATUS_IF_ERROR(_status)	do {if (_status != SD_OK) return _status; } while(0)
#define SD_ACMD41_TIMEOUT 300

typedef enum {
	SD_OK,
	// init
	SD_ERR_NO_CARD,
	SD_ERR_VOLTAGE,
	SD_ERR_TIMEOUT_ACMD41,
	SD_ERR_LEGACY,
	SD_ERR_OCR,
	SD_ERR_CSD,
	SD_ERR_CMD16,

	// read/write
	SD_ERR_TIMEOUT,
	SD_ERR_TOKEN,
	SD_ERR_READ,
	SD_ERR_WRITE,
} sd_status_t;

typedef enum {
	SD_V2_SC,
	SD_V2_HC
} sd_type_t;

typedef struct {
	SPI_TypeDef *spi;
	sd_type_t type;
	uint32_t block_count;
	uint8_t initialized;
} sd_card_t;

sd_status_t sd_init(sd_card_t *card);

sd_status_t sd_read_block(sd_card_t *card, uint32_t addr, uint8_t *buf);

sd_status_t sd_write_block(sd_card_t *card, uint32_t addr, const uint8_t *buf);

#endif /* INC_SD_DRIVER_H_ */
