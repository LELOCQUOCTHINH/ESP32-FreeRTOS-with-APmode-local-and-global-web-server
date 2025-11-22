#include <stdio.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

/* Include header của component chúng ta vừa viết */
#include "apmode.h"
#include "dht11.h"
#include "soil_sensor.h"
#include "relay.h"

#define DHT11_pin GPIO_NUM_18
#define SOILMOISTURE_pin GPIO_NUM_1

void app_main(void)
{
    /* 1. System Init (NVS, Netif, Event Loop) */
    // esp_err_t ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     ret = nvs_flash_init();
    // }
    // ESP_ERROR_CHECK(ret);

    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    

    // /* 2. Start WiFi Manager Component */
    // ESP_LOGI("MAIN", "Starting WiFi Manager Component...");
    // apmode_init();

    dht11_init(DHT11_pin);
    soil_sensor_init(SOILMOISTURE_pin);

    // Example 1: Active High Relay (1 = ON, 0 = OFF)
    relay_init(GPIO_NUM_10, RELAY_ACTIVE_HIGH);
    // relay_init(GPIO_NUM_9, RELAY_ACTIVE_HIGH);

    while (1) {
        // dht11_reading_t data = dht11_read();

        // if (data.temperature != -1) {
        //     printf("Temp: %.1f C, Hum: %.1f %%\n", data.temperature, data.humidity);
        // } else {
        //     printf("Failed to read from DHT11\n");
        // }

        // int raw = soil_sensor_read_raw();
        // int percent = soil_sensor_read_percentage();

        // printf("Soil Moisture: %d (Raw) - %d%%\n", raw, percent);

        if (relay_get_state(GPIO_NUM_10) == RELAY_ON) {
            printf("Relay is ON\n");
        } else {
            printf("Relay is OFF\n");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
        relay_toggle(GPIO_NUM_10);
    }
}