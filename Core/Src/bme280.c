#include <stdint.h>
#include <stdio.h>
#include "main.h"
#include "i2c_driver.h"
#include "bme280.h"

i2c_status_t bme280_config(bme280_t *dev, I2C_HandleTypeDef hi2c) {
	i2c_status_t status;

	dev->i2c = hi2c.Instance;
	dev->config.mode = BME280_MODE_NORMAL;
	dev->config.osrs_h = BME280_OSRS_H_x2;
	dev->config.osrs_p = BME280_OSRS_P_x2;
	dev->config.osrs_t = BME280_OSRS_T_x2;

	// REG_CONFIG
	uint8_t reg_data = 0;
	reg_data |= dev->config.filter;
	status = i2c_write_reg(dev->i2c, BME280_ADDR, REG_CONFIG, reg_data);
	I2C_RETURN_STATUS_IF_ERROR(status);

	// REG_CTRL_HUM
	reg_data = 0;
	reg_data |= dev->config.osrs_h;
	status = i2c_write_reg(dev->i2c, BME280_ADDR, REG_CTRL_HUM, reg_data);
	I2C_RETURN_STATUS_IF_ERROR(status);

	// REG_CTRL_MEAS
	reg_data = 0;
	reg_data |= (dev->config.osrs_p) | (dev->config.osrs_t) | (dev->config.mode);
	status = i2c_write_reg(dev->i2c, BME280_ADDR, REG_CTRL_MEAS, reg_data);
	I2C_RETURN_STATUS_IF_ERROR(status);

	return status;
}

i2c_status_t bme280_read_chip_id(bme280_t *dev) {
	i2c_status_t status;

	uint8_t chip_id;
	status = i2c_read_regs(dev->i2c, BME280_ADDR, REG_CHIP_ID, &chip_id, 1);
	I2C_RETURN_STATUS_IF_ERROR(status);

	if (chip_id != 0x60) {
		return I2C_ERR_OTHER;
	}

	return status;
}

i2c_status_t bme280_read_calib(bme280_t *dev) {
	i2c_status_t status;
	uint8_t buf1[25], buf2[7];

	status = i2c_read_regs(dev->i2c, BME280_ADDR, REG_CALIB_00, buf1, 25);
	I2C_RETURN_STATUS_IF_ERROR(status);

	dev->calib.dig_T1 = (buf1[1] << 8) | buf1[0];
	dev->calib.dig_T2 = (buf1[3] << 8) | buf1[2];
	dev->calib.dig_T3 = (buf1[5] << 8) | buf1[4];

	dev->calib.dig_P1 = (buf1[7] << 8) | buf1[6];
	dev->calib.dig_P2 = (buf1[9] << 8) | buf1[8];
	dev->calib.dig_P3 = (buf1[11] << 8) | buf1[10];
	dev->calib.dig_P4 = (buf1[13] << 8) | buf1[12];
	dev->calib.dig_P5 = (buf1[15] << 8) | buf1[14];
	dev->calib.dig_P6 = (buf1[17] << 8) | buf1[16];
	dev->calib.dig_P7 = (buf1[19] << 8) | buf1[18];
	dev->calib.dig_P8 = (buf1[21] << 8) | buf1[20];
	dev->calib.dig_P9 = (buf1[23] << 8) | buf1[22];

	dev->calib.dig_H1 = (buf1[25]);

	status = i2c_read_regs(dev->i2c, BME280_ADDR, REG_CALIB_26, buf2, 7);
	I2C_RETURN_STATUS_IF_ERROR(status);

	dev->calib.dig_H2 = (buf2[1] << 8) | buf2[0];
	dev->calib.dig_H3 = buf2[2];
	dev->calib.dig_H4 = (buf2[3] << 4) | (buf2[4] & 0xF);
	dev->calib.dig_H5 = (buf2[4] >> 4) | (buf2[5] << 4);
	dev->calib.dig_H6 = (buf2[6]);

	return status;
}

static i2c_status_t bme280_read_raw_data(bme280_t *dev) {
	i2c_status_t status;
	uint8_t buf[8];

	status = i2c_read_regs(dev->i2c, BME280_ADDR, REG_PRESS_MSB, buf, 8);
	I2C_RETURN_STATUS_IF_ERROR(status);

	dev->raw_data.raw_humid = (buf[6] << 8) | buf[7];
	dev->raw_data.raw_temp = (buf[3] << 12) | (buf[4] << 4) | (buf[5] >> 4);
	dev->raw_data.raw_pres = (buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4);

	return status;
}

static int32_t bme280_compensate_temp(bme280_t *dev) {
	int32_t var1, var2, adc_T, t_fine, T;
	adc_T = dev->raw_data.raw_temp;

	var1 = ((((adc_T >> 3) - ((int32_t)dev->calib.dig_T1 << 1))) * ((int32_t)dev->calib.dig_T2)) >> 11;
	var2 = (((((adc_T >> 4) - ((int32_t)dev->calib.dig_T1)) * ((adc_T>>4) - ((int32_t)dev->calib.dig_T1))) >> 12) *
			((int32_t)dev->calib.dig_T3)) >> 14;
	t_fine = var1 + var2;
	dev->raw_data.t_fine = t_fine;
	T = ((t_fine) * 5 + 128) >> 8;
	return T;
}
static uint32_t bme280_compensate_pres(bme280_t *dev) {
	int64_t var1, var2, adc_P, p;
	adc_P = dev->raw_data.raw_pres;

	var1 = ((int64_t)dev->raw_data.t_fine) - 128000;
	var2 = var1 * var1 * (int64_t)dev->calib.dig_P6;
	var2 = var2 + ((var1*(int64_t)dev->calib.dig_P5) << 17);
	var2 = var2 + (((int64_t)dev->calib.dig_P4) << 35);
	var1 = ((var1 * var1 * (int64_t)dev->calib.dig_P3) >> 8) + ((var1 * (int64_t)dev->calib.dig_P2) << 12);
	var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dev->calib.dig_P1) >> 33;
	if (var1 == 0) {
		return 0;
	}
	p = 1048576 - adc_P;
	p = (((p << 31) - var2)*3125)/var1;
	var1 = (((int64_t)dev->calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
	var2 = (((int64_t)dev->calib.dig_P8) * p) >> 19;
	p = ((p + var1 + var2) >> 8) + (((int64_t)dev->calib.dig_P7)<<4);

	return (uint32_t)p;
}
static uint32_t bme280_compensate_hum(bme280_t *dev) {
	int32_t v_x1, adc_H;
	adc_H = dev->raw_data.raw_humid;

	v_x1 = (dev->raw_data.t_fine - ((int32_t)76800));

	v_x1 = (((((adc_H << 14) - (((int32_t)dev->calib.dig_H4) << 20)
			- (((int32_t)dev->calib.dig_H5) * v_x1)) + ((int32_t)16384)) >> 15)
			* (((((((v_x1 * ((int32_t)dev->calib.dig_H6)) >> 10)
			* (((v_x1 * ((int32_t)dev->calib.dig_H3)) >> 11) + ((int32_t)32768))) >> 10)
			+ ((int32_t)2097152)) * ((int32_t)dev->calib.dig_H2) + 8192) >> 14));

	v_x1 = (v_x1 - (((((v_x1 >> 15) * (v_x1 >> 15)) >> 7)
			* ((int32_t)dev->calib.dig_H1)) >> 4));

	v_x1 = (v_x1 < 0) ? 0 : v_x1;
	v_x1 = (v_x1 > 419430400) ? 419430400 : v_x1;

	return (uint32_t)(v_x1 >> 12);
}

// temp in Celsius, pres in hPa, humid in %RH
i2c_status_t bme280_read(bme280_t *dev) {
	i2c_status_t status;

	status = bme280_read_raw_data(dev);
	I2C_RETURN_STATUS_IF_ERROR(status);

	dev->data.temp = bme280_compensate_temp(dev) * 0.01;
	dev->data.pres = bme280_compensate_pres(dev) / 256.0 / 100.0;
	dev->data.humid = bme280_compensate_hum(dev) / 1024.0;

	return status;
}
