#include "dht11.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "DHT11";
static gpio_num_t dht_gpio = GPIO_NUM_4; // Default, can be changed by init

/* Helper: Delay in microseconds */
static void delay_us(uint32_t us) {
    esp_rom_delay_us(us);
}

/* Helper: Wait for pin state change with timeout */
static int wait_for_level(int expected_level, int timeout_us) {
    int count = 0;
    while (gpio_get_level(dht_gpio) != expected_level) {
        if (count++ > timeout_us) return -1; // Timeout
        delay_us(1);
    }
    return count;
}

void dht11_init(gpio_num_t gpio_num) {
    dht_gpio = gpio_num;
    /* Set pin as input initially with pull-up (DHT11 idle state is High) */
    gpio_reset_pin(dht_gpio);
    /* Note: External pull-up resistor (4.7k-10k) is recommended, 
       but internal pull-up helps stability if wire is short */
    gpio_set_pull_mode(dht_gpio, GPIO_PULLUP_ONLY);
    ESP_LOGI(TAG, "Initialized on GPIO %d", dht_gpio);
}

dht11_reading_t dht11_read(void) {
    dht11_reading_t result = { .temperature = -1, .humidity = -1 };
    uint8_t data[5] = {0};

    /* --- 1. Send Start Signal --- */
    gpio_set_direction(dht_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht_gpio, 0); // Pull Low
    vTaskDelay(pdMS_TO_TICKS(20)); // Keep Low for at least 18ms
    gpio_set_level(dht_gpio, 1); // Pull High
    delay_us(30);
    gpio_set_direction(dht_gpio, GPIO_MODE_INPUT); // Switch to Input to listen

    /* --- 2. Check Sensor Response --- */
    // Expect Low for ~80us then High for ~80us
    if (wait_for_level(0, 85) == -1) {
        ESP_LOGE(TAG, "Timeout waiting for Low response");
        return result;
    }
    if (wait_for_level(1, 85) == -1) {
        ESP_LOGE(TAG, "Timeout waiting for High response");
        return result;
    }
    if (wait_for_level(0, 85) == -1) {
        ESP_LOGE(TAG, "Timeout waiting for Start Data");
        return result;
    }

    /* --- 3. Read 40 bits (5 bytes) --- */
    for (int i = 0; i < 40; i++) {
        // Wait for start of bit (Low signal ends)
        if (wait_for_level(1, 55) == -1) {
            ESP_LOGE(TAG, "Timeout reading bit %d (Low)", i);
            return result;
        }

        // Measure duration of High signal
        // ~26-28us = '0', ~70us = '1'
        int duration = 0;
        while (gpio_get_level(dht_gpio) == 1) {
            if (duration++ > 80) break; // Safety break
            delay_us(1);
        }

        // Store bit
        if (duration > 40) { // Threshold usually around 40-50us
            data[i / 8] |= (1 << (7 - (i % 8)));
        }
    }

    /* --- 4. Validate Checksum --- */
    // DHT11 Format:
    // Byte 0: Humidity Int
    // Byte 1: Humidity Dec (Always 0 for DHT11)
    // Byte 2: Temperature Int
    // Byte 3: Temperature Dec (Always 0 for DHT11)
    // Byte 4: Checksum (Sum of Byte 0-3)
    
    if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        result.humidity = (float)data[0] + (float)data[1] * 0.1; // DHT11 usually int only
        result.temperature = (float)data[2] + (float)data[3] * 0.1; // DHT22 uses bytes differently
        
        /* Fix for standard DHT11 which only returns integer part */
        /* Some DHT11 modules might return decimal, but standard ones put 0 in byte 1 & 3 */
        
        ESP_LOGD(TAG, "Read Success: T=%.1f, H=%.1f", result.temperature, result.humidity);
    } else {
        ESP_LOGE(TAG, "Checksum Error! Recv: %02x %02x %02x %02x | Calc: %02x", 
                 data[0], data[1], data[2], data[3], ((data[0] + data[1] + data[2] + data[3]) & 0xFF));
    }

    return result;
}