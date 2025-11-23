#include "stamode.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h" 
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "STAMode";
#define NVS_NAMESPACE "storage"
#define NVS_KEY_CONFIG "auto_cfg_v2" // Đổi key để tránh xung đột struct cũ

/* --- Global Data Storage --- */
typedef struct {
    float temp;
    float hum;
    int soil;
    int relay;
    int mode; 
} http_packet_t;

/* UPDATED: Struct lưu cấu hình Auto Mode (Thêm biến Enable) */
typedef struct {
    int temp_en;    // 0: Ignore, 1: Enable
    float temp_thresh;
    int temp_op;    
    
    int hum_en;     // 0: Ignore, 1: Enable
    float hum_thresh;
    int hum_op;     
    
    int soil_en;    // 0: Ignore, 1: Enable
    int soil_thresh;
    int soil_op;    
} auto_config_t;

static http_packet_t s_http_packet = {0};

/* Default Settings: Chỉ kích hoạt Soil Sensor, bỏ qua Temp/Hum */
static auto_config_t s_auto_config = {
    .temp_en = 0, .temp_thresh = 0.0, .temp_op = 1,
    .hum_en = 0,  .hum_thresh = 0.0,  .hum_op = 1,
    .soil_en = 1, .soil_thresh = 30,  .soil_op = 0
};

extern QueueHandle_t device_queue; 

typedef struct {
    int command_id;
    int value; 
} device_control_t; 

/* --- EMBEDDED FILES --- */
extern const uint8_t sta_index_html_start[] asm("_binary_sta_index_html_start");
extern const uint8_t sta_index_html_end[]   asm("_binary_sta_index_html_end");
extern const uint8_t favicon_png_start[]    asm("_binary_sta_favicon_32x32_png_start");
extern const uint8_t favicon_png_end[]      asm("_binary_sta_favicon_32x32_png_end");
extern const uint8_t logo_png_start[]       asm("_binary_sta_logoBK_png_start");
extern const uint8_t logo_png_end[]         asm("_binary_sta_logoBK_png_end");

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static httpd_handle_t s_server = NULL; 

/* --- NVS HELPER FUNCTIONS --- */

static void save_config_nvs() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        err = nvs_set_blob(my_handle, NVS_KEY_CONFIG, &s_auto_config, sizeof(auto_config_t));
        if (err == ESP_OK) nvs_commit(my_handle);
        nvs_close(my_handle);
        ESP_LOGI(TAG, "Config Saved to NVS");
    }
}

static void load_config_nvs() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);
    if (err == ESP_OK) {
        size_t required_size = sizeof(auto_config_t);
        auto_config_t saved_cfg;
        err = nvs_get_blob(my_handle, NVS_KEY_CONFIG, &saved_cfg, &required_size);
        
        if (err == ESP_OK && required_size == sizeof(auto_config_t)) {
            s_auto_config = saved_cfg;
            ESP_LOGI(TAG, "Config Loaded: Soil En=%d, Thresh=%d", s_auto_config.soil_en, s_auto_config.soil_thresh);
        }
        nvs_close(my_handle);
    } else {
        ESP_LOGW(TAG, "No config found, using defaults");
    }
}

/* --- MQTT Event Handler --- */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        esp_mqtt_client_subscribe(s_mqtt_client, "v1/devices/me/rpc/request/+", 0);
        break;
    case MQTT_EVENT_DATA:
        if (device_queue) {
            device_control_t cmd;
            bool valid = false;
            if (strstr(event->data, "setRelay")) {
                cmd.command_id = (strstr(event->data, "true") || strstr(event->data, "1")) ? 1 : 0;
                valid = true;
            } else if (strstr(event->data, "setMode")) {
                if (strstr(event->data, "manual")) cmd.command_id = 3;
                else cmd.command_id = 2;
                valid = true;
            }
            if (valid) xQueueSend(device_queue, &cmd, 0);
        }
        break;
    default: break;
    }
}

/* --- HTTP Handlers --- */
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)sta_index_html_start, sta_index_html_end - sta_index_html_start);
    return ESP_OK;
}
static esp_err_t favicon_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/png");
    httpd_resp_send(req, (const char *)favicon_png_start, favicon_png_end - favicon_png_start);
    return ESP_OK;
}
static esp_err_t logo_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/png");
    httpd_resp_send(req, (const char *)logo_png_start, logo_png_end - logo_png_start);
    return ESP_OK;
}

static esp_err_t sensors_api_handler(httpd_req_t *req) {
    char json[128];
    sprintf(json, "{\"temp\":%.1f,\"hum\":%.1f,\"soil\":%d,\"relay\":%d,\"mode\":%d}", 
            s_http_packet.temp, s_http_packet.hum, s_http_packet.soil, s_http_packet.relay, s_http_packet.mode);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

/* UPDATED: GET Settings Handler (Thêm các biến Enabled: te, he, se) */
static esp_err_t settings_get_handler(httpd_req_t *req) {
    char json[300];
    sprintf(json, "{\"te\":%d,\"tt\":%.1f,\"to\":%d,\"he\":%d,\"ht\":%.1f,\"ho\":%d,\"se\":%d,\"st\":%d,\"so\":%d}", 
            s_auto_config.temp_en, s_auto_config.temp_thresh, s_auto_config.temp_op,
            s_auto_config.hum_en, s_auto_config.hum_thresh, s_auto_config.hum_op,
            s_auto_config.soil_en, s_auto_config.soil_thresh, s_auto_config.soil_op);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

/* UPDATED: POST Settings Handler (Parse thêm te, he, se) */
static esp_err_t settings_post_handler(httpd_req_t *req) {
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';
    
    int te = s_auto_config.temp_en; float tt = s_auto_config.temp_thresh; int to = s_auto_config.temp_op;
    int he = s_auto_config.hum_en; float ht = s_auto_config.hum_thresh; int ho = s_auto_config.hum_op;
    int se = s_auto_config.soil_en; int st = s_auto_config.soil_thresh; int so = s_auto_config.soil_op;

    char *ptr;
    if ((ptr = strstr(buf, "\"te\":"))) te = atoi(ptr + 5);
    if ((ptr = strstr(buf, "\"tt\":"))) tt = atof(ptr + 5);
    if ((ptr = strstr(buf, "\"to\":"))) to = atoi(ptr + 5);
    
    if ((ptr = strstr(buf, "\"he\":"))) he = atoi(ptr + 5);
    if ((ptr = strstr(buf, "\"ht\":"))) ht = atof(ptr + 5);
    if ((ptr = strstr(buf, "\"ho\":"))) ho = atoi(ptr + 5);
    
    if ((ptr = strstr(buf, "\"se\":"))) se = atoi(ptr + 5);
    if ((ptr = strstr(buf, "\"st\":"))) st = atoi(ptr + 5);
    if ((ptr = strstr(buf, "\"so\":"))) so = atoi(ptr + 5);

    s_auto_config.temp_en = te; s_auto_config.temp_thresh = tt; s_auto_config.temp_op = to;
    s_auto_config.hum_en = he;  s_auto_config.hum_thresh = ht;  s_auto_config.hum_op = ho;
    s_auto_config.soil_en = se; s_auto_config.soil_thresh = st; s_auto_config.soil_op = so;

    save_config_nvs();
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

static esp_err_t relay_toggle_handler(httpd_req_t *req) {
    device_control_t cmd;
    cmd.command_id = (s_http_packet.relay == 1) ? 0 : 1; 
    if (device_queue) {
        xQueueSend(device_queue, &cmd, 0);
        httpd_resp_send(req, "OK", 2);
    } else httpd_resp_send_500(req);
    return ESP_OK;
}

static esp_err_t control_mode_handler(httpd_req_t *req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';
    int cmd_id = -1;
    if (strstr(buf, "2")) cmd_id = 2; 
    if (strstr(buf, "3")) cmd_id = 3; 
    if (cmd_id != -1 && device_queue) {
        device_control_t cmd;
        cmd.command_id = cmd_id;
        xQueueSend(device_queue, &cmd, 0);
        httpd_resp_send(req, "OK", 2);
    } else httpd_resp_send_500(req);
    return ESP_OK;
}

/* --- PUBLIC FUNCTIONS --- */

/* UPDATED: Export thêm các biến enabled */
void stamode_get_config(int *t_en, float *t_th, int *t_op, 
                        int *h_en, float *h_th, int *h_op, 
                        int *s_en, int *s_th, int *s_op) {
    *t_en = s_auto_config.temp_en;
    *t_th = s_auto_config.temp_thresh;
    *t_op = s_auto_config.temp_op;
    
    *h_en = s_auto_config.hum_en;
    *h_th = s_auto_config.hum_thresh;
    *h_op = s_auto_config.hum_op;
    
    *s_en = s_auto_config.soil_en;
    *s_th = s_auto_config.soil_thresh;
    *s_op = s_auto_config.soil_op;
}

void stamode_get_sensor_values(float *temp, float *hum, int *soil) {
    *temp = s_http_packet.temp;
    *hum = s_http_packet.hum;
    *soil = s_http_packet.soil;
}

void stamode_update_http_data(float temp, float hum, int soil, int relay, int mode) {
    s_http_packet.temp = temp;
    s_http_packet.hum = hum;
    s_http_packet.soil = soil;
    s_http_packet.relay = relay;
    s_http_packet.mode = mode;
}

void stamode_update_relay_status_http(int relay_state, int mode) {
    s_http_packet.relay = relay_state;
    s_http_packet.mode = mode;
}

void stamode_publish_mqtt(float temp, float hum, int soil, int relay, int mode) {
    if (s_mqtt_client) {
        char payload[128];
        sprintf(payload, "{\"temperature\":%.1f,\"humidity\":%.1f,\"soil_moisture\":%d,\"relay\":%d,\"mode\":%d}", 
                temp, hum, soil, relay, mode);
        esp_mqtt_client_publish(s_mqtt_client, "v1/devices/me/telemetry", payload, 0, 1, 0);
    }
}

void stamode_stop(void) {
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "STA Web Server Stopped");
    }
    if (s_mqtt_client) {
        esp_mqtt_client_stop(s_mqtt_client);
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        ESP_LOGI(TAG, "MQTT Client Stopped");
    }
}

void stamode_start(const char *broker_uri, int mqtt_port, const char *mqtt_token) {
    load_config_nvs();

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12; 

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_handler };
        httpd_uri_t favicon = { .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler };
        httpd_uri_t logo = { .uri = "/logo.png", .method = HTTP_GET, .handler = logo_handler };
        httpd_uri_t api_sen = { .uri = "/api/sensors", .method = HTTP_GET, .handler = sensors_api_handler };
        httpd_uri_t api_set_get = { .uri = "/api/settings", .method = HTTP_GET, .handler = settings_get_handler };
        httpd_uri_t api_set_post = { .uri = "/api/settings", .method = HTTP_POST, .handler = settings_post_handler };
        httpd_uri_t api_rel = { .uri = "/api/relay/toggle", .method = HTTP_POST, .handler = relay_toggle_handler };
        httpd_uri_t api_mode = { .uri = "/api/control", .method = HTTP_POST, .handler = control_mode_handler };
        
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &favicon);
        httpd_register_uri_handler(server, &logo);
        httpd_register_uri_handler(server, &api_sen);
        httpd_register_uri_handler(server, &api_set_get);
        httpd_register_uri_handler(server, &api_set_post);
        httpd_register_uri_handler(server, &api_rel);
        httpd_register_uri_handler(server, &api_mode);
        
        ESP_LOGI(TAG, "Local Dashboard started on Port 80");
    }

    char full_uri[128];
    sprintf(full_uri, "mqtt://%s:%d", broker_uri, mqtt_port);
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = full_uri,
        .credentials.username = mqtt_token,
    };
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);
}