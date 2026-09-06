#include <spi_driver.h>

extern volatile uint8_t dma_done;

static SPI_TypeDef *active_spi;
static DMA_Channel_TypeDef *active_dma_rx, *active_dma_tx;

void spi_config(SPI_TypeDef *spi, spi_baud_rate_t br) {
	spi->CR1 &= ~SPI_CR1_SPE;

	spi->CR1 &= ~(SPI_CR1_CPHA | SPI_CR1_CPOL | SPI_CR1_BR_Msk);
	spi->CR1 |= SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (br << SPI_CR1_BR_Pos);

	spi->CR2 &= ~(0xF << 8);
	spi->CR2 |= SPI_CR2_FRXTH | (0x7 << 8); // using 8-bit data size

	spi->CR1 |= SPI_CR1_SPE;
}

static spi_status_t spi_transfer_byte(SPI_TypeDef *spi, uint8_t *readBuf, uint8_t writeBuf) {
	uint32_t timeout;

	timeout = SPI_TIMEOUT;
	while (!(spi->SR & SPI_SR_TXE)) {
		if (--timeout == 0) return SPI_ERR_TIMEOUT;
	}
	*(volatile uint8_t*)&spi->DR = writeBuf;

	timeout = SPI_TIMEOUT;
	while (!(spi->SR & SPI_SR_RXNE)) {
		if (--timeout == 0) return SPI_ERR_TIMEOUT;
	}
	*readBuf = *(volatile uint8_t*)&spi->DR;

	return SPI_OK;
}

spi_status_t spi_write_byte(SPI_TypeDef *spi, uint8_t writeBuf) {
	uint8_t discard_byte;
	return spi_transfer_byte(spi, &discard_byte, writeBuf);
}

spi_status_t spi_read_byte(SPI_TypeDef *spi, uint8_t *readBuf) {
	const uint8_t dummy_byte = 0xFF;
	return spi_transfer_byte(spi, readBuf, dummy_byte);
}








static void spi_dma_cfg_regs(SPI_TypeDef *spi, DMA_Channel_TypeDef *dma_tx, DMA_Channel_TypeDef *dma_rx, uint8_t *rxBuf, uint8_t *txBuf, uint16_t len) {
	dma_tx->CCR &= ~DMA_CCR_EN;
	dma_rx->CCR &= ~DMA_CCR_EN;
	spi->CR2 &= ~(SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN);

	// configure dma rx
	dma_rx->CPAR = (uint32_t)&spi->DR;
	dma_rx->CMAR = (uint32_t)rxBuf;
	dma_rx->CNDTR = len;
	dma_rx->CCR |= DMA_CCR_MINC;

	// configure dma tx
	dma_tx->CPAR = (uint32_t)&spi->DR;
	dma_tx->CMAR = (uint32_t)txBuf;
	dma_tx->CNDTR = len;
	dma_tx->CCR |= DMA_CCR_MINC | DMA_CCR_DIR;

	spi->CR2 |= SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN;
}

void spi_dma_transfer(SPI_TypeDef *spi, DMA_Channel_TypeDef *dma_tx, DMA_Channel_TypeDef *dma_rx, uint8_t *rxBuf, uint8_t *txBuf, uint16_t len) {
	while (spi->SR & SPI_SR_BSY);

	active_spi = spi;
	active_dma_rx = dma_rx;
	active_dma_tx = dma_tx;

	spi_dma_cfg_regs(spi, dma_tx, dma_rx, rxBuf, txBuf, len);

	dma_rx->CCR |= DMA_CCR_TCIE;
	dma_rx->CCR |= DMA_CCR_EN;
	dma_tx->CCR |= DMA_CCR_EN;
}

// theres already a DMA1_Channel2_3_IRQHandler function, put this code in there and delete this
void spi_dma_rxcomplete_callback(void) {
	// rx, needs flag
	if (DMA1->ISR & DMA_ISR_TCIF2) {
		DMA1->IFCR |= DMA_IFCR_CTCIF2;
		dma_done = 1;

		active_dma_rx->CCR &= ~(DMA_CCR_EN);
		active_dma_tx->CCR &= ~(DMA_CCR_EN);
		active_spi->CR2 &= ~(SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN);
	}
}
