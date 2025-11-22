#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Relay activation logic levels.
 * Some relays trigger on HIGH signal, others on LOW signal.
 */
typedef enum {
    RELAY_ACTIVE_LOW = 0,   /*!< Relay turns ON when GPIO is LOW (0) */
    RELAY_ACTIVE_HIGH = 1   /*!< Relay turns ON when GPIO is HIGH (1) */
} relay_active_level_t;

/**
 * @brief Relay state definitions.
 */
typedef enum {
    RELAY_OFF = 0,          /*!< Relay is currently OFF (Open circuit) */
    RELAY_ON = 1            /*!< Relay is currently ON (Closed circuit) */
} relay_state_t;

/**
 * @brief Initialize a relay on a specific GPIO pin.
 * * @param gpio_num The GPIO pin connected to the relay control pin.
 * @param active_level The logic level that activates the relay (HIGH or LOW).
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t relay_init(gpio_num_t gpio_num, relay_active_level_t active_level);

/**
 * @brief Turn the relay ON.
 * * @param gpio_num The GPIO pin of the relay.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t relay_on(gpio_num_t gpio_num);

/**
 * @brief Turn the relay OFF.
 * * @param gpio_num The GPIO pin of the relay.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t relay_off(gpio_num_t gpio_num);

/**
 * @brief Toggle the relay state (ON -> OFF or OFF -> ON).
 * * @param gpio_num The GPIO pin of the relay.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t relay_toggle(gpio_num_t gpio_num);

/**
 * @brief Get the current state of the relay.
 * * @param gpio_num The GPIO pin of the relay.
 * @return relay_state_t RELAY_ON or RELAY_OFF.
 */
relay_state_t relay_get_state(gpio_num_t gpio_num);

#ifdef __cplusplus
}
#endif