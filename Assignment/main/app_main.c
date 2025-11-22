#include <stdio.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

/* Include header của component chúng ta vừa viết */
#include "apmode.h"

void app_main(void)
{
    /* 1. System Init (NVS, Netif, Event Loop) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    

    /* 2. Start WiFi Manager Component */
    ESP_LOGI("MAIN", "Starting WiFi Manager Component...");
    apmode_init();
}