#include "soil_sensor.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SoilSensor";

/* Internal State */
static adc1_channel_t s_adc_channel = -1; 
static bool s_is_initialized = false;

/* --- CALIBRATION VALUES --- */
/* Based on observation: 
   - Dry (Air): Raw ~ 100
   - Wet (Water): Raw ~ 3000-4095
   -> Higher Raw = Wetter Soil
*/
#define SOIL_DRY_VALUE  100   // Threshold for 0% (Dry)
#define SOIL_WET_VALUE  3000  // Threshold for 100% (Wet)

/* Helper: Map GPIO to ADC1 Channel for ESP32-S3 (Yolo Uno) */
static adc1_channel_t get_adc_channel_from_gpio(gpio_num_t gpio_num) {
    switch (gpio_num) {
        case GPIO_NUM_1: return ADC1_CHANNEL_0;
        case GPIO_NUM_2: return ADC1_CHANNEL_1;
        case GPIO_NUM_3: return ADC1_CHANNEL_2;
        case GPIO_NUM_4: return ADC1_CHANNEL_3;
        case GPIO_NUM_5: return ADC1_CHANNEL_4;
        case GPIO_NUM_6: return ADC1_CHANNEL_5;
        case GPIO_NUM_7: return ADC1_CHANNEL_6;
        case GPIO_NUM_8: return ADC1_CHANNEL_7;
        case GPIO_NUM_9: return ADC1_CHANNEL_8;
        case GPIO_NUM_10: return ADC1_CHANNEL_9;
        default: return -1;
    }
}

esp_err_t soil_sensor_init(gpio_num_t gpio_num) {
    adc1_channel_t channel = get_adc_channel_from_gpio(gpio_num);
    
    if (channel == -1) {
        ESP_LOGE(TAG, "Invalid GPIO for ADC1 on ESP32-S3! Please use GPIO 1-10.");
        return ESP_ERR_INVALID_ARG;
    }
    
    s_adc_channel = channel;
    
    /* Configure ADC: 12-bit resolution, 11dB attenuation (up to ~3.1V) */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(s_adc_channel, ADC_ATTEN_DB_11);
    
    s_is_initialized = true;
    ESP_LOGI(TAG, "Soil Sensor initialized on GPIO %d", gpio_num);
    return ESP_OK;
}

int soil_sensor_read_raw(void) {
    if (!s_is_initialized) {
        ESP_LOGE(TAG, "Sensor not initialized!");
        return -1;
    }

    /* Average 64 samples for noise reduction */
    long sum = 0;
    for (int i = 0; i < 64; i++) {
        sum += adc1_get_raw(s_adc_channel);
        esp_rom_delay_us(50); 
    }
    return (int)(sum / 64);
}

int soil_sensor_read_percentage(void) {
    int raw = soil_sensor_read_raw();
    if (raw == -1) return -1;

    /* Clamp values within calibration range */
    int clamped_raw = raw;
    if (clamped_raw < SOIL_DRY_VALUE) clamped_raw = SOIL_DRY_VALUE;
    if (clamped_raw > SOIL_WET_VALUE) clamped_raw = SOIL_WET_VALUE;

    /* Map to Percentage: (Current - Dry) / (Wet - Dry) * 100 */
    int percent = (int)(100.0 * (clamped_raw - SOIL_DRY_VALUE) / (SOIL_WET_VALUE - SOIL_DRY_VALUE));
    
    return percent;
}