#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Struct for containing the result read from DHT11
 */
typedef struct {
    float temperature;  // Temperature (Celsius)
    float humidity;     // Humidity (%)
} dht11_reading_t;

/**
 * @brief Init GPIO for DHT11
 * * @param gpio_num GPIO pin for receiving data from DHT11
 */
void dht11_init(gpio_num_t gpio_num);

/**
 * @brief read data from DHT11
 * * @return dht11_reading_t struct for containing temperature and humidity.
 * return -1 if error.
 */
dht11_reading_t dht11_read(void);

#ifdef __cplusplus
}
#endif