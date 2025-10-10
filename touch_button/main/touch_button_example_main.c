#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "Touch Button Example";

void app_main(void) {
    esp_log_level_set("gpio", ESP_LOG_DEBUG);

    // Configure GPIO directions and pull-ups
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_6) | (1ULL << GPIO_NUM_7) | (1ULL << GPIO_NUM_0), // Inputs
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_LOGI(TAG, "Configured inputs GPIO 6, 7, 0 with pull-ups");

    // Configure GPIO 48 as output
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_48); // Output
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLDOWN_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_LOGI(TAG, "Configured output GPIO 48");

    // Simple polling loop
    while (1) {
        if (!gpio_get_level(GPIO_NUM_6) || !gpio_get_level(GPIO_NUM_7) || !gpio_get_level(GPIO_NUM_0)) {
            // Turn on GPIO 48 if any input is low (pressed)
            gpio_set_level(GPIO_NUM_48, 1);
            ESP_LOGI(TAG, "Button pressed! GPIO 48 ON");
        } else {
            // Turn off GPIO 48 if all inputs are high
            gpio_set_level(GPIO_NUM_48, 0);
            // ESP_LOGI(TAG, "No button pressed, GPIO 48 OFF"); // Optional debug
        }
        vTaskDelay(100 / portTICK_PERIOD_MS); // Small delay
    }
}