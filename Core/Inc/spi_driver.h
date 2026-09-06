#ifndef INC_SPI_DRIVER_H_
#define INC_SPI_DRIVER_H_


#include "stdint.h"
#include "stm32g0xx.h"

#define SPI_TIMEOUT 100000
#define SPI_CS_LOW()	(GPIOA->BSRR = (1U << 16));
#define SPI_CS_HIGH()	(GPIOA->BSRR = (1U << 0));

#define DMA_TX	DMA1_Channel1
#define DMA_RX	DMA1_Channel2

typedef enum {
	SPI_BR_DIV2,
	SPI_BR_DIV4,
	SPI_BR_DIV8,
	SPI_BR_DIV16,
	SPI_BR_DIV32,
	SPI_BR_DIV64,
	SPI_BR_DIV128,
	SPI_BR_DIV256
} spi_baud_rate_t;

typedef enum {
	SPI_OK,
	SPI_ERR_TIMEOUT
} spi_status_t;

void spi_config(SPI_TypeDef *spi, spi_baud_rate_t br);
spi_status_t spi_read_byte(SPI_TypeDef *spi, uint8_t *readBuf);
spi_status_t spi_write_byte(SPI_TypeDef *spi, uint8_t writeBuf);


void spi_dma_transfer(SPI_TypeDef *spi, DMA_Channel_TypeDef *dma_tx, DMA_Channel_TypeDef *dma_rx, uint8_t *rxBuf, uint8_t *txBuf, uint16_t len);

void spi_dma_rxcomplete_callback(void);

#endif /* INC_SPI_DRIVER_H_ */
