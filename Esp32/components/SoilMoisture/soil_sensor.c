#include "soil_sensor.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SoilSensor";

/* ADC Handle */
static adc_oneshot_unit_handle_t adc1_handle;
static adc_channel_t adc_channel;
static bool s_is_initialized = false;

/* Calibration Values */
#define SOIL_DRY_VALUE  100   
#define SOIL_WET_VALUE  3000  

/* Helper: Map GPIO to ADC1 Channel (ESP32-S3 Specific) */
static bool get_adc_channel(gpio_num_t gpio_num, adc_channel_t *channel) {
    switch (gpio_num) {
        case GPIO_NUM_1: *channel = ADC_CHANNEL_0; return true;
        case GPIO_NUM_2: *channel = ADC_CHANNEL_1; return true;
        case GPIO_NUM_3: *channel = ADC_CHANNEL_2; return true;
        case GPIO_NUM_4: *channel = ADC_CHANNEL_3; return true;
        case GPIO_NUM_5: *channel = ADC_CHANNEL_4; return true;
        case GPIO_NUM_6: *channel = ADC_CHANNEL_5; return true;
        case GPIO_NUM_7: *channel = ADC_CHANNEL_6; return true;
        case GPIO_NUM_8: *channel = ADC_CHANNEL_7; return true;
        case GPIO_NUM_9: *channel = ADC_CHANNEL_8; return true;
        case GPIO_NUM_10: *channel = ADC_CHANNEL_9; return true;
        default: return false;
    }
}

esp_err_t soil_sensor_init(gpio_num_t gpio_num) {
    if (!get_adc_channel(gpio_num, &adc_channel)) {
        ESP_LOGE(TAG, "Invalid GPIO for ADC1 on ESP32-S3");
        return ESP_ERR_INVALID_ARG;
    }

    /* 1. Config ADC Unit */
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    /* 2. Config Channel */
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT, // Usually 12-bit
        .atten = ADC_ATTEN_DB_12,         // New enum for 11dB (legacy DB_11)
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, adc_channel, &config));

    s_is_initialized = true;
    ESP_LOGI(TAG, "Soil Sensor initialized on GPIO %d (Channel %d)", gpio_num, adc_channel);
    return ESP_OK;
}

int soil_sensor_read_raw(void) {
    if (!s_is_initialized) return -1;

    int adc_raw = 0;
    long sum = 0;
    
    /* Read 64 samples */
    for (int i = 0; i < 64; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, adc_channel, &adc_raw));
        sum += adc_raw;
        esp_rom_delay_us(50);
    }
    
    return (int)(sum / 64);
}

int soil_sensor_read_percentage(void) {
    int raw = soil_sensor_read_raw();
    if (raw == -1) return -1;

    int clamped_raw = raw;
    if (clamped_raw < SOIL_DRY_VALUE) clamped_raw = SOIL_DRY_VALUE;
    if (clamped_raw > SOIL_WET_VALUE) clamped_raw = SOIL_WET_VALUE;

    int percent = (int)(100.0 * (clamped_raw - SOIL_DRY_VALUE) / (SOIL_WET_VALUE - SOIL_DRY_VALUE));
    return percent;
}