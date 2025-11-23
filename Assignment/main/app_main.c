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
#include "mdns.h"

/* Include User Components */
#include "apmode.h"
#include "dht11.h"
#include "soil_sensor.h"
#include "relay.h"
#include "stamode.h"

/* --- CONFIGURATION --- */
#define BASE_TICK_RATE_MS       1000    
#define MQTT_PUBLISH_CYCLE      10      

#define NETWORK_QUEUE_SIZE      20      
#define DEVICE_QUEUE_SIZE       5       

#define DHT11_PIN               GPIO_NUM_18
#define SOIL_PIN                GPIO_NUM_1
#define RELAY_PIN               GPIO_NUM_10
#define LED_PIN                 GPIO_NUM_48

#define MQTT_BROKER_HOST        "app.coreiot.io"
#define MQTT_BROKER_PORT        1883
#define ACCESS_TOKEN            "wszmzebjxp41b0c5ynvv"
#define MDNS_HOSTNAME           "smartgarden" 

static const char *TAG = "MAIN_APP";

/* --- DATA STRUCTURES --- */
typedef struct {
    float temp;
    float hum;
    int soil;
    int relay_state;
    int mode; 
} sensor_data_t;

typedef struct {
    int command_id; 
} device_control_t;

/* UPDATED IMPORT function */
extern void stamode_get_config(int *t_en, float *t_th, int *t_op, 
                               int *h_en, float *h_th, int *h_op, 
                               int *s_en, int *s_th, int *s_op);
extern void stamode_get_sensor_values(float *temp, float *hum, int *soil);

QueueHandle_t network_queue = NULL; 
QueueHandle_t device_queue = NULL;
QueueHandle_t ui_queue = NULL;      

static bool is_wifi_connected = false;
static bool is_manual_mode = false;
static bool is_stamode_services_started = false;
static SemaphoreHandle_t wifi_connected_sem = NULL; 

void start_mdns_service()
{
    esp_err_t err = mdns_init();
    if (err) { ESP_LOGE(TAG, "MDNS Init failed: %d", err); return; }
    mdns_hostname_set(MDNS_HOSTNAME);
    mdns_instance_name_set("Smart Garden ESP32 Device");
    mdns_service_add("SmartGarden-Web", "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS Service started. Access via: http://%s.local", MDNS_HOSTNAME);
}

/* --- HANDLERS --- */
static void system_event_handler(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "___ WiFi Connected! IP: " IPSTR " ___", IP2STR(&event->ip_info.ip));
        is_wifi_connected = true;
        xSemaphoreGive(wifi_connected_sem);
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        is_wifi_connected = false;
        ESP_LOGW(TAG, "WiFi Lost.");

        /* NEW: Reset flag để khi có mạng lại, nó biết phải bật lại services */
        is_stamode_services_started = false;
    }
}

/* --- TASK 1: RELAY CONTROL (Core 1) --- */
void relay_control_task(void *pvParameters) {
    relay_init(RELAY_PIN, RELAY_ACTIVE_HIGH);
    relay_init(LED_PIN, RELAY_ACTIVE_HIGH);
    
    device_control_t cmd;
    
    float t_th, h_th; 
    int t_op, h_op, s_th, s_op;
    int t_en, h_en, s_en; // NEW: Biến bật/tắt

    float cur_temp, cur_hum;
    int cur_soil;

    while (1) {
        relay_toggle(LED_PIN); 

        bool cmd_received = xQueueReceive(device_queue, &cmd, pdMS_TO_TICKS(BASE_TICK_RATE_MS));
        bool state_changed = false;

        if (cmd_received == pdTRUE) {
            ESP_LOGI(TAG, "CMD Received Immediate: %d", cmd.command_id);
            if (cmd.command_id == 2) { is_manual_mode = false; state_changed = true; }      
            else if (cmd.command_id == 3) { is_manual_mode = true; state_changed = true; }  
            else if (cmd.command_id == 1) { is_manual_mode = true; relay_on(RELAY_PIN); state_changed = true; }
            else if (cmd.command_id == 0) { is_manual_mode = true; relay_off(RELAY_PIN); state_changed = true; }
        }
        
        /* UPDATED: LOGIC AUTO THÔNG MINH HƠN */
        if (!is_manual_mode) {
            stamode_get_sensor_values(&cur_temp, &cur_hum, &cur_soil);
            stamode_get_config(&t_en, &t_th, &t_op, 
                               &h_en, &h_th, &h_op, 
                               &s_en, &s_th, &s_op);

            if (cur_temp != 0.0 && cur_soil != -1) {
                // Logic cho từng cảm biến:
                // Nếu DISABLED (!en) -> Luôn coi là ĐÚNG (true) để không ảnh hưởng điều kiện khác
                // Nếu ENABLED (en) -> Kiểm tra điều kiện thực tế
                
                bool cond_temp = (!t_en) || ((t_op == 1) ? (cur_temp > t_th) : (cur_temp < t_th));
                bool cond_hum  = (!h_en) || ((h_op == 1) ? (cur_hum > h_th)  : (cur_hum < h_th));
                bool cond_soil = (!s_en) || ((s_op == 1) ? (cur_soil > s_th) : (cur_soil < s_th));

                // Chỉ bật nếu ít nhất 1 cảm biến được kích hoạt VÀ tất cả điều kiện enabled đều thỏa mãn
                bool any_enabled = (t_en || h_en || s_en);
                bool should_be_on = any_enabled && cond_temp && cond_hum && cond_soil;

                int current_relay = (relay_get_state(RELAY_PIN) == RELAY_ON) ? 1 : 0;
                
                if (should_be_on != current_relay) {
                    if (should_be_on) relay_on(RELAY_PIN); else relay_off(RELAY_PIN);
                    state_changed = true;
                    ESP_LOGI(TAG, "Auto Trig: T:%.1f H:%.1f S:%d [En:%d%d%d] -> %s", 
                             cur_temp, cur_hum, cur_soil, t_en, h_en, s_en, should_be_on ? "ON":"OFF");
                }
            }
        }
        
        if (state_changed) {
            int current_relay = (relay_get_state(RELAY_PIN) == RELAY_ON) ? 1 : 0;
            int current_mode = is_manual_mode ? 1 : 0;
            stamode_update_relay_status_http(current_relay, current_mode);
        }
    }
}

/* --- TASK 2: SENSOR READING (Core 1) --- */
void sensor_telemetry_task(void *pvParameters) {
    dht11_init(DHT11_PIN);
    TickType_t last_wake = xTaskGetTickCount();
    static int mqtt_counter = 0; 
    
    while (1) {
        dht11_reading_t dht = dht11_read();
        int soil = soil_sensor_read_percentage();
        int relay_st = (relay_get_state(RELAY_PIN) == RELAY_ON) ? 1 : 0;
        int mode_st = is_manual_mode ? 1 : 0;

        if (dht.temperature != -1) {
            sensor_data_t packet = { .temp=dht.temperature, .hum=dht.humidity, .soil=soil, .relay_state=relay_st, .mode=mode_st };
            
            xQueueOverwrite(ui_queue, &packet);

            mqtt_counter++;
            if (mqtt_counter >= MQTT_PUBLISH_CYCLE) {
                if (xQueueSend(network_queue, &packet, 0) != pdTRUE) {
                    sensor_data_t dum; xQueueReceive(network_queue, &dum, 0);
                    xQueueSend(network_queue, &packet, 0);
                }
                mqtt_counter = 0; 
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(BASE_TICK_RATE_MS));
    }
}

/* --- TASK 3: UI UPDATE (Core 0) --- */
void ui_update_task(void *pvParameters) {
    sensor_data_t packet;
    while (1) {
        if (xQueueReceive(ui_queue, &packet, portMAX_DELAY) == pdTRUE) {
            stamode_update_http_data(packet.temp, packet.hum, packet.soil, packet.relay_state, packet.mode);
        }
    }
}

/* --- TASK 4: NETWORK MANAGER (Core 0) --- */
void network_task(void *pvParameters) {
    sensor_data_t packet;
    apmode_init();

    while (1) {
        if (xSemaphoreTake(wifi_connected_sem, 0) == pdTRUE) {
            if (!is_stamode_services_started) {
                ESP_LOGI(TAG, "Starting STA Mode Services...");
                stamode_start(MQTT_BROKER_HOST, MQTT_BROKER_PORT, ACCESS_TOKEN);
                is_stamode_services_started = true;
            }
        }

        if (is_wifi_connected) {
            if (xQueueReceive(network_queue, &packet, pdMS_TO_TICKS(100)) == pdTRUE) {
                stamode_publish_mqtt(packet.temp, packet.hum, packet.soil, packet.relay_state, packet.mode);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase(); nvs_flash_init();
    }
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta(); 

    start_mdns_service();

    wifi_connected_sem = xSemaphoreCreateBinary();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &system_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &system_event_handler, NULL, NULL));

    network_queue = xQueueCreate(NETWORK_QUEUE_SIZE, sizeof(sensor_data_t));
    device_queue = xQueueCreate(DEVICE_QUEUE_SIZE, sizeof(device_control_t));
    ui_queue = xQueueCreate(1, sizeof(sensor_data_t));

    if (soil_sensor_init(SOIL_PIN) != ESP_OK) ESP_LOGE(TAG, "Soil Sensor Init Failed");

    xTaskCreatePinnedToCore(sensor_telemetry_task, "telemetry", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(relay_control_task, "relay_ctrl", 4096, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(ui_update_task, "ui_update", 3072, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(network_task, "network", 4096, NULL, 10, NULL, 0);
    
    ESP_LOGI(TAG, "System Initialized.");
}