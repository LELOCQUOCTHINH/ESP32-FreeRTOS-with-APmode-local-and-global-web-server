#pragma once

#include "driver/adc.h"
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Soil Moisture Sensor on a specific GPIO pin.
 * * Automatically maps the GPIO to the corresponding ADC1 Channel.
 * * @param gpio_num The GPIO pin connected to the Analog Output (AO) of the sensor.
 * (Supports: GPIO 1-10 on ESP32-S3)
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if the pin is invalid.
 */
esp_err_t soil_sensor_init(gpio_num_t gpio_num);

/**
 * @brief Read raw ADC value from the sensor.
 * * @return int 12-bit ADC value (0 - 4095). Returns -1 if not initialized.
 */
int soil_sensor_read_raw(void);

/**
 * @brief Read soil moisture as a percentage (%).
 * * @return int Value from 0% (Dry) to 100% (Wet).
 */
int soil_sensor_read_percentage(void);

#ifdef __cplusplus
}
#endif