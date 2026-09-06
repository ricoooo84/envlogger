#include "i2c_driver.h"



// returns 0 if ok, -1 if nack, -2 if timeout
i2c_status_t i2c_write_reg(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg, uint8_t buf) {
	uint32_t timeout;

	// r/w bit implied set to 0
	i2c->CR2 = (addr << 1) | (2 << I2C_CR2_NBYTES_Pos) | (I2C_CR2_AUTOEND);

	// send address
	i2c->CR2 |= I2C_CR2_START;

	// check send success
	timeout = I2C_TIMEOUT;
	while (!(i2c->ISR & I2C_ISR_TXIS)) {
		if (i2c->ISR & I2C_ISR_NACKF) {
			i2c->ICR |= I2C_ISR_NACKF;
			return I2C_ERR_NACK;
		}
		if (--timeout == 0) {
			return I2C_ERR_TIMEOUT;
		}
	}

	// send register
	i2c->TXDR = reg;

	// check send success
	timeout = I2C_TIMEOUT;
	while (!(i2c->ISR & I2C_ISR_TXIS)) {
		if (i2c->ISR & I2C_ISR_NACKF) {
			i2c->ICR |= I2C_ICR_NACKCF;
			return I2C_ERR_NACK;
		}
		if (--timeout == 0) {
			return I2C_ERR_TIMEOUT;
		}
	}

	// send data
	i2c->TXDR = buf;

	// check send success + account for stop transmission
	timeout = I2C_TIMEOUT;
	while (!(i2c->ISR & I2C_ISR_STOPF)) {
		if (i2c->ISR & I2C_ISR_NACKF) {
			i2c->ICR |= I2C_ICR_NACKCF;
			return I2C_ERR_NACK;
		}
		if (--timeout == 0) {
			return I2C_ERR_TIMEOUT;
		}
	}

	i2c->ICR |= I2C_ICR_STOPCF;

	return I2C_OK;
}

i2c_status_t i2c_read_regs(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len) {

	uint32_t timeout;

	// CR2: write, no autoend
	i2c->CR2 = (addr << 1) | (1 << I2C_CR2_NBYTES_Pos);

	// send address probe
	i2c->CR2 |= (I2C_CR2_START);

	timeout = I2C_TIMEOUT;
	while (!(i2c->ISR & I2C_ISR_TXIS)) {
		if (i2c->ISR & I2C_ISR_NACKF) {
			i2c->ICR |= I2C_ICR_NACKCF;
			return I2C_ERR_NACK;
		}
		if (--timeout == 0) {
			return I2C_ERR_TIMEOUT;
		}
	}

	// send register address
	i2c->TXDR = reg;

	timeout = I2C_TIMEOUT;
	while (!(i2c->ISR & I2C_ISR_TC)) {
		if (i2c->ISR & I2C_ISR_NACKF) {
			i2c->ICR |= I2C_ICR_NACKCF;
			return I2C_ERR_NACK;
		}
		if (--timeout == 0) {
			return I2C_ERR_TIMEOUT;
		}
	}

	// CR2: read, autoend
	if (len > 255) return -3; // NBYTES is 8 bits
	i2c->CR2 = (addr << 1) | (len << I2C_CR2_NBYTES_Pos) | (I2C_CR2_RD_WRN) | (I2C_CR2_AUTOEND);
	i2c->CR2 |= I2C_CR2_START;

	for (uint8_t i = 0; i < len; i++) {
		timeout = I2C_TIMEOUT;
		// while theres nothing sent yet
		while (!(i2c->ISR & I2C_ISR_RXNE)) {
			if (i2c->ISR & I2C_ISR_NACKF) {
				i2c->ICR |= I2C_ICR_NACKCF;
				return I2C_ERR_NACK;
			}
			if (--timeout == 0) {
				return I2C_ERR_TIMEOUT;
			}
		}
		buf[i] = i2c->RXDR;
	}

	timeout = I2C_TIMEOUT;
	while (!(i2c->ISR & I2C_ISR_STOPF)) {
		if (i2c->ISR & I2C_ISR_NACKF) {
			i2c->ICR |= I2C_ICR_NACKCF;
			return I2C_ERR_NACK;
		}
		if (--timeout == 0) {
			return I2C_ERR_TIMEOUT;
		}
	}

	i2c->ICR |= I2C_ICR_STOPCF;
	return I2C_OK;
}
