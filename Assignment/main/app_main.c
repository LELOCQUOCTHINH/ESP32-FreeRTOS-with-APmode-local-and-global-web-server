#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"

/* Include User Components */
#include "apmode.h"
#include "dht11.h"
#include "soil_sensor.h"
#include "relay.h"
#include "STAmode.h"

/* --- CONFIGURATION --- */
#define TELEMETRY_INTERVAL_SEC  10      /* Gửi dữ liệu lên Server mỗi 10 giây */
#define CONTROL_INTERVAL_MS     1000    /* Điều khiển Relay mỗi 1 giây */
#define NETWORK_QUEUE_SIZE      20      /* Bộ đệm chứa 20 gói tin khi mất mạng */
#define DEVICE_QUEUE_SIZE       5       /* Hàng đợi lệnh điều khiển từ Server */

/* Pins Config */
#define DHT11_PIN               GPIO_NUM_18
#define SOIL_PIN                GPIO_NUM_1
#define RELAY_PIN               GPIO_NUM_10
#define LED_PIN                 GPIO_NUM_48

/* MQTT Config */
#define MQTT_BROKER_IP          "app.coreiot.io"
#define MQTT_BROKER_PORT        1883
#define ACCESS_TOKEN            "wszmzebjxp41b0c5ynvv"
#define MQTT_TOPIC_TELEMETRY    "v1/devices/me/telemetry"

static const char *TAG = "MAIN_APP";

/* --- DATA STRUCTURES --- */
typedef struct {
    float temp;
    float hum;
    int soil;
    int relay_state; // Thêm trạng thái relay để báo cáo
} sensor_data_t;

typedef struct {
    int command_id; // 1 = ON, 0 = OFF, 2 = AUTO
} device_control_t;

/* --- QUEUE HANDLES --- */
static QueueHandle_t network_queue = NULL;
static QueueHandle_t device_queue = NULL;

/* --- GLOBAL STATE --- */
static bool is_wifi_connected = false;
static bool is_manual_mode = false; // Cờ để biết đang chạy Auto hay Manual

/* -------------------------------------------------------------------------- */
/* EVENT HANDLERS                                                             */
/* -------------------------------------------------------------------------- */
static void system_event_handler(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        is_wifi_connected = true;
        ESP_LOGI(TAG, "WiFi Connected. Starting MQTT...");
        mqtt_app_start(MQTT_BROKER_IP, MQTT_BROKER_PORT, ACCESS_TOKEN);
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        is_wifi_connected = false;
        ESP_LOGW(TAG, "WiFi Lost. Buffering data...");
    }
}

/* -------------------------------------------------------------------------- */
/* TASK 1: RELAY CONTROL (Fast Loop - 1s)                                     */
/* -------------------------------------------------------------------------- */
void relay_control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Relay Control Task Started");
    
    /* Init Actuator */
    relay_init(RELAY_PIN, RELAY_ACTIVE_HIGH);
    relay_init(LED_PIN, RELAY_ACTIVE_HIGH);
    // Init Soil Sensor here or in main, handled in main for safety
    
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval_ticks = pdMS_TO_TICKS(CONTROL_INTERVAL_MS);

    while (1) {
        /* 1. Check commands from Server (Non-blocking) */
        device_control_t cmd;
        if (xQueueReceive(device_queue, &cmd, 0) == pdTRUE) {
            ESP_LOGI(TAG, "CMD Received: %d", cmd.command_id);
            if (cmd.command_id == 2) {
                is_manual_mode = false; // Switch to Auto
                ESP_LOGI(TAG, "Switched to AUTO Mode");
            } else {
                is_manual_mode = true; // Switch to Manual
                if (cmd.command_id == 1) relay_on(RELAY_PIN);
                else relay_off(RELAY_PIN);
            }
        }

        /* 2. Automatic Logic (Only if NOT in Manual Mode) */
        if (!is_manual_mode) {
            /* Read Soil Sensor (Fast ADC operation) */
            int soil = soil_sensor_read_percentage();
            
            if (soil != -1) {
                if (soil < 30) {
                    // ESP_LOGI(TAG, "Soil Dry (%d%%) -> Pump ON", soil);
                    relay_on(RELAY_PIN);
                } else {
                    // ESP_LOGI(TAG, "Soil Wet (%d%%) -> Pump OFF", soil);
                    relay_off(RELAY_PIN);
                }
            }
        }

        relay_toggle(LED_PIN);

        /* Ensure strictly 1s interval */
        vTaskDelayUntil(&last_wake_time, interval_ticks);
    }
}

/* -------------------------------------------------------------------------- */
/* TASK 2: SENSOR TELEMETRY (Slow Loop - 10s)                                 */
/* -------------------------------------------------------------------------- */
void sensor_telemetry_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Telemetry Task Started");
    
    /* Init Sensors */
    dht11_init(DHT11_PIN);
    // Soil sensor initiated in main/relay task

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval_ticks = pdMS_TO_TICKS(TELEMETRY_INTERVAL_SEC * 1000);

    while (1) {
        /* 1. Read All Sensors */
        dht11_reading_t dht = dht11_read();
        int soil = soil_sensor_read_percentage();
        int relay_st = (relay_get_state(RELAY_PIN) == RELAY_ON) ? 1 : 0;

        /* 2. Prepare Data Packet */
        if (dht.temperature != -1) {
            sensor_data_t packet;
            packet.temp = dht.temperature;
            packet.hum = dht.humidity;
            packet.soil = soil;
            packet.relay_state = relay_st;
            
            /* 3. Send to Queue with "Drop Oldest" Logic */
            /* Try to send immediately */
            if (xQueueSend(network_queue, &packet, 0) != pdTRUE) {
                /* Queue Full! Read one item (drop oldest) to make space */
                sensor_data_t dummy;
                xQueueReceive(network_queue, &dummy, 0);
                
                /* Now send the new one */
                if (xQueueSend(network_queue, &packet, 0) == pdTRUE) {
                    ESP_LOGW(TAG, "Queue Full. Dropped Oldest. Buffered Newest.");
                    ESP_LOGI(TAG, "Data Queued: T=%.1f H=%.1f S=%d R=%d", 
                         packet.temp, packet.hum, packet.soil, packet.relay_state);
                }
            } else {
                ESP_LOGI(TAG, "Data Queued: T=%.1f H=%.1f S=%d R=%d", 
                         packet.temp, packet.hum, packet.soil, packet.relay_state);
            }
        } else {
            ESP_LOGW(TAG, "DHT11 Read Error");
        }

        vTaskDelayUntil(&last_wake_time, interval_ticks);
    }
}

/* -------------------------------------------------------------------------- */
/* TASK 3: NETWORK MANAGER                                                    */
/* -------------------------------------------------------------------------- */
void network_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Network Task Started");
    char json_payload[128];
    sensor_data_t packet;

    /* Start WiFi Manager */
    apmode_init();

    while (1) {
        /* Only process queue if we have WiFi connection */
        if (is_wifi_connected) {
            /* Wait for data (Blocking wait 100ms to check connection flag periodically) */
            if (xQueueReceive(network_queue, &packet, pdMS_TO_TICKS(100)) == pdTRUE) {
                
                /* Format JSON */
                sprintf(json_payload, "{\"temperature\":%.1f,\"humidity\":%.1f,\"soil_moisture\":%d,\"relay\":%d}", 
                        packet.temp, packet.hum, packet.soil, packet.relay_state);
                
                /* Publish MQTT */
                int msg_id = mqtt_app_publish(MQTT_TOPIC_TELEMETRY, json_payload);
                
                if (msg_id != -1) {
                    ESP_LOGI(TAG, "MQTT Sent: %s", json_payload);
                } else {
                    ESP_LOGE(TAG, "MQTT Publish Failed");
                    /* Optional: Put back to queue? Or just drop to keep realtime */
                }
            }
        } else {
            /* Lost Connection: Just sleep to let other tasks run. 
               The Queue will accumulate data in background. */
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

/* -------------------------------------------------------------------------- */
/* MAIN ENTRY                                                                 */
/* -------------------------------------------------------------------------- */
void app_main(void)
{
    /* 1. Init System Resources */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    /* Global Interfaces */
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    /* Register Handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &system_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &system_event_handler, NULL, NULL));

    /* 2. Create Queues */
    network_queue = xQueueCreate(NETWORK_QUEUE_SIZE, sizeof(sensor_data_t));
    device_queue = xQueueCreate(DEVICE_QUEUE_SIZE, sizeof(device_control_t));

    /* 3. Init Shared Hardware */
    if (soil_sensor_init(SOIL_PIN) != ESP_OK) {
        ESP_LOGE(TAG, "Soil Sensor Init Failed");
    }

    /* 4. Create Tasks (Multi-core usage) */
    /* Sensor Task: Core 1, Priority 5 */
    xTaskCreatePinnedToCore(sensor_telemetry_task, "telemetry", 4096, NULL, 5, NULL, 1);
    
    /* Relay Task: Core 1, Priority 6 (Higher priority for realtime control) */
    xTaskCreatePinnedToCore(relay_control_task, "relay_ctrl", 4096, NULL, 6, NULL, 1);
    
    /* Network Task: Core 0, Priority 10 (Highest for Comms stability) */
    xTaskCreatePinnedToCore(network_task, "network", 4096, NULL, 10, NULL, 0);
    
    ESP_LOGI(TAG, "System Initialized.");
}