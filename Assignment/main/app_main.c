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
#include "APmode.h"
#include "dht11.h"
#include "soil_sensor.h"
#include "relay.h"
#include "STAmode.h"

/* --- CONFIGURATION --- */
#define SENSOR_INTERVAL_SEC     10      /* Gửi dữ liệu mỗi 10 giây */
#define NETWORK_QUEUE_SIZE      20      /* Chứa được 20 gói tin (đủ cho > 3 phút mất mạng) */
#define DEVICE_QUEUE_SIZE       5       /* Hàng đợi lệnh điều khiển */

#define DHT11_PIN               GPIO_NUM_18
#define SOIL_PIN                GPIO_NUM_1
#define RELAY_PIN               GPIO_NUM_10

#define MQTT_BROKER_IP      "app.coreiot.io"
#define MQTT_BROKER_PORT    1883
#define ACCESS_TOKEN        "wszmzebjxp41b0c5ynvv"
#define MQTT_TOPIC_TELEMETRY    "v1/devices/me/telemetry"
#define MQTT_TOPIC_RPC          "v1/devices/me/rpc/request/+"

static const char *TAG = "MAIN_APP";

/* --- DATA STRUCTURES --- */
typedef struct {
    float temp;
    float hum;
    int soil;
    int64_t timestamp; // Optional: Time stamp if needed
} sensor_data_t;

typedef struct {
    int command_id;
    int value; // 1=ON, 0=OFF
} device_control_t;

/* --- QUEUE HANDLES --- */
static QueueHandle_t network_queue = NULL;
static QueueHandle_t device_queue = NULL;

/* --- GLOBAL STATE FLAGS --- */
static bool is_wifi_connected = false;
static bool is_mqtt_connected = false;

/* -------------------------------------------------------------------------- */
/* EVENT HANDLERS                              */
/* -------------------------------------------------------------------------- */

/* Handle WiFi & IP Events to update state flags */
static void system_event_handler(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        is_wifi_connected = true;
        ESP_LOGI(TAG, "System: WiFi Connected. Ready for Network Task.");
        
        /* Start MQTT when WiFi is ready */
        mqtt_app_start(MQTT_BROKER_IP, MQTT_BROKER_PORT, ACCESS_TOKEN);
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        is_wifi_connected = false;
        is_mqtt_connected = false;
        ESP_LOGW(TAG, "System: WiFi Lost.");
    }
}

/* Note: You need to update MQTTClient component to expose connection status 
   or register a callback here. For simplicity, we assume MQTT works if started. */

/* -------------------------------------------------------------------------- */
/* TASKS IMPLEMENTATION                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief Task: DEVICES & SENSORS
 * - Read sensors periodically.
 * - Send data to Network Queue.
 * - Listen for control commands from Network Task.
 */
void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Sensor Task Started");
    
    /* Init Sensors */
    dht11_init(DHT11_PIN);
    if (soil_sensor_init(SOIL_PIN) != ESP_OK) {
        ESP_LOGE(TAG, "Soil Sensor Init Failed");
    }
    relay_init(RELAY_PIN, RELAY_ACTIVE_HIGH);

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval_ticks = pdMS_TO_TICKS(SENSOR_INTERVAL_SEC * 1000);

    while (1) {
        /* 1. Check for incoming control commands (Non-blocking) */
        device_control_t cmd;
        if (xQueueReceive(device_queue, &cmd, 0) == pdTRUE) {
            ESP_LOGI(TAG, "Received Command: Set Relay %d", cmd.value);
            if (cmd.value == 1) relay_on(RELAY_PIN);
            else relay_off(RELAY_PIN);
        }

        /* 2. Read Sensors */
        dht11_reading_t dht = dht11_read();
        int soil = soil_sensor_read_percentage();

        /* 3. Prepare Data Packet */
        if (dht.temperature != -1) {
            sensor_data_t packet;
            packet.temp = dht.temperature;
            packet.hum = dht.humidity;
            packet.soil = soil;
            
            /* 4. Send to Network Queue */
            /* If queue is full, we overwrite the oldest data (optional) or just fail */
            if (xQueueSend(network_queue, &packet, 0) != pdTRUE) {
                ESP_LOGW(TAG, "Network Queue Full! Dropping data.");
            } else {
                ESP_LOGI(TAG, "Data Queued: T=%.1f H=%.1f S=%d", packet.temp, packet.hum, packet.soil);
            }
        } else {
            ESP_LOGW(TAG, "Sensor Read Failed");
        }

        /* 5. Auto Irrigation Logic (Local Control) */
        /* Only run if no manual command overrides it recently? (For simple logic, we run it always) */
        if (soil < 30) relay_on(RELAY_PIN);
        else relay_off(RELAY_PIN);

        /* Wait until next interval */
        vTaskDelayUntil(&last_wake_time, interval_ticks);
    }
}

/**
 * @brief Task: NETWORK
 * - Manage WiFi/MQTT.
 * - Consume data from Network Queue.
 * - Send data to Server when connected.
 */
void network_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Network Task Started");
    char json_payload[128];
    sensor_data_t packet;

    /* Start WiFi Manager (APMode Component) */
    /* This will handle NVS check, STA connection or AP provisioning */
    apmode_init();

    while (1) {
        /* Wait for data from Sensor Task (Blocking wait) */
        if (xQueueReceive(network_queue, &packet, portMAX_DELAY) == pdTRUE) {
            
            /* Check Connectivity */
            if (is_wifi_connected) {
                /* Format JSON */
                sprintf(json_payload, "{\"temperature\":%.1f,\"humidity\":%.1f,\"soil_moisture\":%d}", 
                        packet.temp, packet.hum, packet.soil);
                
                /* Publish MQTT */
                int msg_id = mqtt_app_publish(MQTT_TOPIC_TELEMETRY, json_payload);
                
                if (msg_id != -1) {
                    ESP_LOGI(TAG, "MQTT Sent: %s", json_payload);
                } else {
                    ESP_LOGE(TAG, "MQTT Publish Failed");
                    /* Optional: Push back to front of queue if critical? */
                }
            } else {
                /* No Network: 
                   Data stays in queue? No, we already popped it.
                   If you want to buffer offline, you need a secondary buffer or peek.
                   For this simple logic, we just drop and log warning.
                   Or: We could re-push it to queue if it's not full.
                */
                ESP_LOGW(TAG, "No Network. Data buffered/dropped.");
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* MAIN ENTRY                                  */
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
    
    /* Register Global Event Handler */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &system_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &system_event_handler, NULL, NULL));

    /* 2. Create Queues */
    network_queue = xQueueCreate(NETWORK_QUEUE_SIZE, sizeof(sensor_data_t));
    device_queue = xQueueCreate(DEVICE_QUEUE_SIZE, sizeof(device_control_t));

    if (network_queue == NULL || device_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queues");
        return;
    }

    /* 3. Create Tasks */
    /* Sensor Task: Stack 4096, Priority 5 */
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    
    /* Network Task: Stack 4096, Priority 10 (Higher priority to handle coms fast) */
    xTaskCreate(network_task, "network_task", 4096, NULL, 10, NULL);
    
    ESP_LOGI(TAG, "System Initialized. Multi-tasking Active.");
}