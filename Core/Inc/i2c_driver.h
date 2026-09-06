#ifndef INC_I2C_DRIVER_H_
#define INC_I2C_DRIVER_H_

#include "stdint.h"
#include "stm32g0xx.h"

#define I2C_TIMEOUT	100000
#define I2C_RETURN_STATUS_IF_ERROR(_status) do {if (_status != I2C_OK) return _status;} while(0)

typedef enum {
	I2C_OK,
	I2C_ERR_NACK,
	I2C_ERR_TIMEOUT,
	I2C_ERR_OTHER
} i2c_status_t;

i2c_status_t i2c_write_reg(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg, uint8_t buf);
i2c_status_t i2c_read_regs(I2C_TypeDef *i2c, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);


#endif /* INC_I2C_DRIVER_H_ */
