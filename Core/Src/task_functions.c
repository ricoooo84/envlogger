#include "task_functions.h"

static bme280_comp_data_t batches[BATCH_SIZE];
static uint8_t batch_count;

static void helper_bme_init(void) {
	if (bme280_config(&bme, hi2c1) != I2C_OK) {
		printf("bme config error\r\n");
	} else {
		printf("bme config passed\r\n");
	}

	if (bme280_read_calib(&bme) != I2C_OK) {
		printf("bme calib error\r\n");
	} else {
		printf("bme calib passed\r\n");
	}

}

static void helper_sd_init(void) {
	sd.spi = SPI1;
	sd.type = SD_V2_HC;
	sd.block_count = 125829120; // hardcoded for 64 GB

	res = f_mount(&fs, "", 1);
	if (res != FR_OK) {
		printf("sd failed mount");
	}

	res = f_open(&file, SD_FILENAME, FA_CREATE_ALWAYS | FA_WRITE);

	const char *header = "timestamp,temp_c,humidity_pct,pressure_hpa\r\n";
	UINT bw;
	f_write(&file, header, strlen(header), &bw);
	f_sync(&file);
}

static void helper_sd_write(void) {
	char line[40];

	for (uint16_t i = 0; i < batch_count; i++) {

		uint16_t write_len = snprintf(line, sizeof(line), "%lu,%.2f,%.2f,%.2f\r\n",
				(unsigned long)batches[i].timestamp, batches[i].temp, batches[i].humid, batches[i].pres);

		if (write_len >= sizeof(line)) {
			write_len = sizeof(line) - 1;
		}

		UINT bytes_written;
		res = f_write(&file, line, write_len, &bytes_written);

		if (res != FR_OK) {
			printf("sd write error: %d\r\n", res);
			LED_SD_OFF();
		}
	}

	f_sync(&file);
}

// status led: should always be on
// OFF: i2c transaction failure
void task_read_sensor(void *parameters) {
	(void)parameters;

	TickType_t last_tick = xTaskGetTickCount();

	// bme has hardcoded configurations, go to bme280.c to change them
	helper_bme_init();

	for (;;) {

		if (bme280_read(&bme) != I2C_OK) {
			LED_BME_OFF();
		} else {
			LED_BME_ON();
			bme.data.timestamp = xTaskGetTickCount() / configTICK_RATE_HZ;

			bme280_comp_data_t copy = bme.data;

			if (xQueueSend(rtos_queue, &copy, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT)) != pdPASS) {
				LED_BME_OFF();
			}
		}
		vTaskDelayUntil(&last_tick, pdMS_TO_TICKS(BME_SAMPLE_PERIOD));
	}
}

// status led: should be mostly off and toggles on occasionally
// ON: writing, dont unplug
// OFF: safe to unplug
void task_write_to_sd(void *parameters) {
	TickType_t last_tick = xTaskGetTickCount();
	bme280_comp_data_t copy;

	helper_sd_init();
	batch_count = 0;

	for (;;) {
		BaseType_t q_status = xQueueReceive(rtos_queue, &copy, 10);
		if (q_status == pdPASS) {

			batches[batch_count++] = copy;

			if (batch_count >= BATCH_SIZE) {
				LED_SD_ON();
				helper_sd_write();
				batch_count = 0;
			} else {
				LED_SD_OFF();
			}
		}

		vTaskDelayUntil(&last_tick, pdMS_TO_TICKS(SD_WRITE_CHECK));
	}
}

