#ifndef INC_TASK_FUNCTIONS_H_
#define INC_TASK_FUNCTIONS_H_

#include <string.h> // for fatfs
#include <stdio.h>
#include "stm32g0xx.h"
#include "bme280.h"
#include "sd_driver.h"
#include "ff.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define BME_SAMPLE_PERIOD	1000
#define QUEUE_SEND_TIMEOUT	100
#define SD_WRITE_CHECK		500
#define BATCH_SIZE			10
#define SD_FILENAME			"envlog.csv"
#define LED_GPIO_BME		4
#define LED_GPIO_SD			5

#define LED_BME_ON()		(GPIOC->BSRR |= (1 << LED_GPIO_BME))
#define LED_BME_OFF()		(GPIOC->BSRR |= (1 << (LED_GPIO_BME + 16)))
#define LED_SD_ON()			(GPIOC->BSRR |= (1 << LED_GPIO_SD))
#define LED_SD_OFF()		(GPIOC->BSRR |= (1 << (LED_GPIO_SD + 16)))

// cubeMX PV

extern I2C_HandleTypeDef hi2c1;

// user PV

extern bme280_t bme;
extern sd_card_t sd;
extern FATFS fs;
extern FIL file;
extern FRESULT res;
extern UINT bw;
extern QueueHandle_t rtos_queue;

void task_read_sensor(void *parameters);
void task_write_to_sd(void *parameters);




#endif /* INC_TASK_FUNCTIONS_H_ */
