#ifndef INC_BME280_H_
#define INC_BME280_H_

#include "main.h"
#include "i2c_driver.h"

#define BME280_ADDR		0x77

#define REG_HUM_MSB		0xFD
#define REG_TEMP_MSB	0xFA
#define REG_PRESS_MSB	0xF7

#define REG_CONFIG		0xF5
#define REG_CTRL_HUM	0xF2
#define REG_CTRL_MEAS	0xF4

#define REG_CALIB_00	0x88
#define REG_CALIB_26	0xE1

#define REG_CHIP_ID		0xD0

typedef enum {
	BME280_OSRS_H_SKIP,
	BME280_OSRS_H_x1,
	BME280_OSRS_H_x2,
	BME280_OSRS_H_x4,
	BME280_OSRS_H_x8,
	BME280_OSRS_H_x16
} bme280_osrs_h_t;
typedef enum {
	BME280_OSRS_T_SKIP,
	BME280_OSRS_T_x1 = (1 << 5),
	BME280_OSRS_T_x2 = (2 << 5),
	BME280_OSRS_T_x4 = (3 << 5),
	BME280_OSRS_T_x8 = (4 << 5),
	BME280_OSRS_T_x16 = (5 << 5),
} bme280_osrs_t_t;
typedef enum {
	BME280_OSRS_P_SKIP,
	BME280_OSRS_P_x1 = (1 << 2),
	BME280_OSRS_P_x2 = (2 << 2),
	BME280_OSRS_P_x4 = (3 << 2),
	BME280_OSRS_P_x8 = (4 << 2),
	BME280_OSRS_P_x16 = (5 << 2),
} bme280_osrs_p_t;
typedef enum {
	BME280_MODE_SLEEP,
	BME280_MODE_FORCED,
	BME280_MODE_NORMAL = 3
} bme280_mode_t;
typedef enum {
	BME280_FILTER_OFF,
	BME280_FILTER_2,
	BME280_FILTER_4,
	BME280_FILTER_8,
	BME280_FILTER_16,
} bme280_filter_t;

typedef struct {
	bme280_osrs_h_t osrs_h;
	bme280_osrs_t_t osrs_t;
	bme280_osrs_p_t osrs_p;

	bme280_mode_t mode;

	bme280_filter_t filter;
} bme280_config_t;

typedef struct {
	uint16_t dig_T1;
	int16_t dig_T2, dig_T3;
	uint16_t dig_P1;
	int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
	uint8_t dig_H1;
	int16_t dig_H2;
	uint8_t dig_H3;
	int16_t dig_H4, dig_H5;
	int8_t dig_H6;
} bme280_calib_t;

typedef struct {
	int32_t raw_temp;
	int32_t raw_pres;
	int16_t raw_humid;
	int32_t t_fine;
} bme280_raw_data_t;

typedef struct {
	uint32_t timestamp;
	double temp;
	double pres;
	double humid;
} bme280_comp_data_t;

typedef struct {
	I2C_TypeDef *i2c;

	bme280_calib_t calib;
	bme280_config_t config;
	bme280_raw_data_t raw_data;
	bme280_comp_data_t data;
} bme280_t;

i2c_status_t bme280_config(bme280_t *dev, I2C_HandleTypeDef hi2c);
i2c_status_t bme280_read_chip_id(bme280_t *dev);
i2c_status_t bme280_read_calib(bme280_t *dev);
i2c_status_t bme280_read(bme280_t *dev);


#endif /* INC_BME280_H_ */
