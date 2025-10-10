#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "Touch Button Example";

// Define enumeration for buttons with GPIO numbers as values
typedef enum {
    BUTTON_6 = GPIO_NUM_6,  // GPIO 6
    BUTTON_7 = GPIO_NUM_7,  // GPIO 7
    BUTTON_0 = GPIO_NUM_0,  // GPIO 0
    BUTTON_COUNT = 3        // Total number of buttons (manually set to enum count)
} button_t;

// Array of button enum values for dynamic mask generation
static const button_t buttons[] = {BUTTON_6, BUTTON_7, BUTTON_0};

void app_main(void) {
    esp_log_level_set("gpio", ESP_LOG_DEBUG);

    // Dynamically build pin_bit_mask for all buttons
    uint64_t pin_bit_mask = 0;
    for (int i = 0; i < BUTTON_COUNT; i++) {
        pin_bit_mask |= (1ULL << buttons[i]); // Use actual GPIO numbers from enum
    }

    // Configure GPIO directions and pull-ups for all buttons
    gpio_config_t io_conf = {
        .pin_bit_mask = pin_bit_mask, // Dynamically generated mask
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_LOGI(TAG, "Configured inputs with pull-ups");

    // Configure GPIO 48 as output
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_48); // Output
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLDOWN_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_LOGI(TAG, "Configured output GPIO 48");

    // Simple polling loop (print only on press)
    while (1) {
        bool any_button_pressed = false;
        for (int i = 0; i < BUTTON_COUNT; i++) {
            button_t button = buttons[i]; // Use actual button GPIO
            if (!gpio_get_level(button)) { // Check if button is pressed (low)
                any_button_pressed = true;
                ESP_LOGI(TAG, "Button %d pressed! (GPIO %d)", i, button);
            }
        }
        gpio_set_level(GPIO_NUM_48, any_button_pressed ? 1 : 0); // Turn on/off GPIO 48
        if (any_button_pressed) {
            ESP_LOGI(TAG, "GPIO 48 ON");
        }
        vTaskDelay(100 / portTICK_PERIOD_MS); // Small delay
    }
}