#include "spi_driver.h"
#include "sd_driver.h"

static uint8_t sd_cmd(sd_card_t *card, uint8_t cmd, uint32_t arg, uint8_t crc) {
	cmd = 0x40 | cmd;

	spi_write_byte(card->spi, 0xFF);

	spi_write_byte(card->spi, cmd);
	spi_write_byte(card->spi, (arg >> 24)); spi_write_byte(card->spi, (arg >> 16)); spi_write_byte(card->spi, (arg >> 8)); spi_write_byte(card->spi, arg);
	spi_write_byte(card->spi, crc);

	uint8_t response;
	uint8_t count = 9;
	do {
		spi_read_byte(card->spi, &response);
	} while ((response & 0x80) && (count-- > 0));

	return response;
}

static void sd_powerup(SPI_TypeDef *spi) {
	SPI_CS_HIGH();

	for (uint8_t i = 0; i < 10; i++) {
		spi_write_byte(spi, 0xFF);
	}
}
static sd_status_t sd_cmd0_go_idle(sd_card_t *card) {
	uint8_t r1;

	SPI_CS_LOW();
	r1 = sd_cmd(card, 0, 0, 0x95);
	SPI_CS_HIGH();
	spi_write_byte(card->spi, 0xFF);

	if (r1 != 0x01) {
		return SD_ERR_NO_CARD;
	}

	return SD_OK;
}
static sd_status_t sd_cmd8_check_voltage(sd_card_t *card) {
	uint8_t r1;

	SPI_CS_LOW();
	r1 = sd_cmd(card, 8, 0x1AA, 0x87);

	// if SDv2 supported, r1 = 0x01
	if (r1 != 0x01) {

		SPI_CS_HIGH();
		spi_write_byte(card->spi, 0xFF);
		return SD_ERR_LEGACY;

	}

	// read echo
	uint8_t r7[4];
	for (uint8_t i = 0; i < 4; i++) {
		spi_read_byte(card->spi, &r7[i]);
	}
	SPI_CS_HIGH();
	spi_write_byte(card->spi, 0xFF);

	// verify echo
	if ((r7[2] == 0x01) && (r7[3] == 0xAA)) {
		return SD_OK;
	}

	return SD_ERR_VOLTAGE;

}
static sd_status_t sd_acmd41_init_loop(sd_card_t *card) {
	// this was in LOOPS, not time when i first tested
	uint32_t timeout = SD_ACMD41_TIMEOUT;
	uint8_t r1 = 0x01;

	do {
		if (timeout-- == 0) {
			return SD_ERR_TIMEOUT_ACMD41;
		}

		// cmd55 = prepare for acmd
		SPI_CS_LOW();
		r1 = sd_cmd(card, 55, 0, 0x01);
		SPI_CS_HIGH();
		spi_write_byte(card->spi, 0xFF);

		// acmd41
		SPI_CS_LOW();
		r1 = sd_cmd(card, 41, 0x40000000, 0x01);
		SPI_CS_HIGH();
		spi_write_byte(card->spi, 0xFF);

	} while (r1 != 0x00);

	return SD_OK;
}
static sd_status_t sd_cmd58_read_ocr(sd_card_t *card) {
	uint8_t r1;
	uint8_t ocr[4];

	SPI_CS_LOW();
	r1 = sd_cmd(card, 58, 0, 0xFD);
	for (uint8_t i = 0; i < 4; i++) {
		spi_read_byte(card->spi, &ocr[i]);
	}
	SPI_CS_HIGH();
	spi_write_byte(card->spi, 0xFF);

	if (r1 != 0x00) {
		return SD_ERR_OCR;
	}

	if (ocr[0] & 0x40) {
		card->type = SD_V2_HC;
	} else {
		card->type = SD_V2_SC;
	}

	return SD_OK;
}

static sd_status_t sd_cmd9_send_csd(sd_card_t *card) {
	uint8_t r1;

	SPI_CS_LOW();
	r1 = sd_cmd(card, 9, 0, 0xAF);

	if (r1 != 0x00) {
		SPI_CS_HIGH();
		spi_write_byte(card->spi, 0xFF);
		return SD_ERR_CSD;
	}

	uint8_t token = 0xFF;
	uint32_t timeout = 2000;
	do {
		if (timeout-- == 0) {
			SPI_CS_HIGH();
			spi_write_byte(card->spi, 0xFF);
			return SD_ERR_TIMEOUT;
		}
		spi_read_byte(card->spi, &token);
	} while (token != 0xFE);

	uint8_t csd[16];
	uint8_t crc[2];
	for (uint8_t i = 0; i < 16; i++) {
		spi_read_byte(card->spi, &csd[i]);
	}

	// discard crc
	spi_read_byte(card->spi, &crc[0]);
	spi_read_byte(card->spi, &crc[1]);

	SPI_CS_HIGH();
	spi_write_byte(card->spi, 0xFF);

	if (card->type == SD_V2_SC) {
		// ****HELP i dont understand this
		uint8_t read_bl_len = csd[5] & 0x0F;
		uint8_t c_size_mult = ((csd[9] & 0x03) << 1) | ((csd[10] >> 7) & 0x01);
		uint32_t c_size = ((uint32_t)(csd[6] & 0x03) << 10) | ((uint32_t)csd[7] << 2) | ((csd[8] >> 6) & 0x03);

		uint32_t block_len = 1UL << read_bl_len;
		uint32_t mult = 1UL << (c_size_mult + 2);
		uint64_t capacity_bytes = (uint64_t)(c_size + 1) * mult * block_len;

		card->block_count = capacity_bytes / 512;

	} else if (card->type == SD_V2_HC) {
		uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
		card->block_count = ((uint64_t)c_size + 1) * 1024;
	}

	return SD_OK;
}

static sd_status_t sd_cmd16_set_blocklen(sd_card_t *card) {
	// function hardcoded to set blocklen to 512

	// SDHC already has 512 as default
	if (card->type == SD_V2_SC) {
		uint8_t r1;

		SPI_CS_LOW();
		r1 = sd_cmd(card, 16, 512, 0xFF);
		SPI_CS_HIGH();
		spi_write_byte(card->spi, 0xFF);

		if (r1 != 0x00) {
			return SD_ERR_CMD16;
		}
	}

	return SD_OK;
}


sd_status_t sd_init(sd_card_t *card) {
	sd_status_t status;

	spi_config(card->spi, SPI_BR_DIV256);

	sd_powerup(card->spi);

	status = sd_cmd0_go_idle(card);
	SD_RETURN_STATUS_IF_ERROR(status);

	status  = sd_cmd8_check_voltage(card);
	SD_RETURN_STATUS_IF_ERROR(status);

	status = sd_acmd41_init_loop(card);
	SD_RETURN_STATUS_IF_ERROR(status);

	status = sd_cmd58_read_ocr(card);
	SD_RETURN_STATUS_IF_ERROR(status);

	status = sd_cmd9_send_csd(card);
	SD_RETURN_STATUS_IF_ERROR(status);

	status = sd_cmd16_set_blocklen(card);
	SD_RETURN_STATUS_IF_ERROR(status);

	spi_config(card->spi, SPI_BR_DIV128);

	card->initialized = 1;

	return SD_OK;
}

static sd_status_t sd_cmd17_read_block(sd_card_t *card, uint32_t addr) {
	uint8_t r1;

	SPI_CS_LOW();
	r1 = sd_cmd(card, 17, addr, 0xFF);
	if (r1 != 0x00) {
		SPI_CS_HIGH();
		spi_write_byte(card->spi, 0xFF);
		return SD_ERR_READ;
	}

	uint8_t token = 0xFF;
	uint32_t timeout = 2000;
	do {
		if (timeout-- == 0) {
			SPI_CS_HIGH();
			spi_write_byte(card->spi, 0xFF);
			return SD_ERR_TIMEOUT;
		}
		spi_read_byte(card->spi, &token);
	} while (token != 0xFE);

	return SD_OK;
}
static void sd_read_payload(sd_card_t *card, uint8_t *buf, uint16_t len) {
	for (uint16_t i = 0; i < len; i++) {
		spi_read_byte(card->spi, &buf[i]);
	}

	uint8_t crc_discard[2];
	spi_read_byte(card->spi, &crc_discard[0]);
	spi_read_byte(card->spi, &crc_discard[1]);

	SPI_CS_HIGH();
	spi_write_byte(card->spi, 0xFF);
}

sd_status_t sd_read_block(sd_card_t *card, uint32_t addr, uint8_t *buf) {
	if (card->type != SD_V2_HC) {
		addr *= 512;
	}

	sd_status_t status;

	status = sd_cmd17_read_block(card, addr);
	SD_RETURN_STATUS_IF_ERROR(status);

	sd_read_payload(card, buf, 512);

	return SD_OK;
}

static sd_status_t sd_cmd24_write_block(sd_card_t *card, uint32_t addr) {
	uint8_t r1;

	SPI_CS_LOW();
	r1 = sd_cmd(card, 24, addr, 0xFF);

	if (r1 != 0x00) {
		SPI_CS_HIGH();
		spi_write_byte(card->spi, 0xFF);
		return SD_ERR_WRITE;
	}

	return SD_OK;
}
static sd_status_t sd_write_payload(sd_card_t *card, const uint8_t *buf) {
	spi_write_byte(card->spi, 0xFE);

	for (uint16_t i = 0; i < 512; i++) {
		spi_write_byte(card->spi, buf[i]);
	}

	// discarded crc bytes
	spi_write_byte(card->spi, 0xFF);
	spi_write_byte(card->spi, 0xFF);

	uint8_t response;
	spi_read_byte(card->spi, &response);

	if ((response & 0x1F) != (0x05)) {
		SPI_CS_HIGH();
		spi_write_byte(card->spi, 0xFF);
		return SD_ERR_WRITE;
	}

	uint8_t busy = 0x00;
	uint32_t timeout = 300;
	do {
		if (timeout-- == 0) {
			SPI_CS_HIGH();
			spi_write_byte(card->spi, 0xFF);
			return SD_ERR_TIMEOUT;
		}
		spi_read_byte(card->spi, &busy);
	} while (busy == 0x00);

	SPI_CS_HIGH();
	spi_write_byte(card->spi, 0xFF);

	return SD_OK;
}
static sd_status_t sd_cmd13_send_status(sd_card_t *card) {
	uint8_t r1;
	uint8_t r2[2];

	SPI_CS_LOW();
	r1 = sd_cmd(card, 13, 0, 0x01);
	spi_read_byte(card->spi, &r2[0]);
	spi_read_byte(card->spi, &r2[1]);
	SPI_CS_HIGH();
	spi_write_byte(card->spi, 0xFF);

	if (r1 != 0x00) {
		return SD_ERR_WRITE;
	}

	return SD_OK;
}

sd_status_t sd_write_block(sd_card_t *card, uint32_t addr, const uint8_t *buf) {
	if (card->type != SD_V2_HC) {
		addr *= 512;
	}

	sd_status_t status;

	status = sd_cmd24_write_block(card, addr);
	SD_RETURN_STATUS_IF_ERROR(status);

	status = sd_write_payload(card, buf);
	SD_RETURN_STATUS_IF_ERROR(status);

	status = sd_cmd13_send_status(card);
	SD_RETURN_STATUS_IF_ERROR(status);

	return SD_OK;
}
