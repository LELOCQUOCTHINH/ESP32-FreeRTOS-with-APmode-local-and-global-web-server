#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "esp_http_server.h"

/* --- Configuration --- */
#define AP_SSID "ESP32_Config_Wifi"
#define AP_PASS "12345678"
#define MAX_RETRY_COUNT 10
#define RECONNECT_INTERVAL_MS (60 * 1000) // 1 phút check lại mạng 1 lần

static const char *TAG = "WiFiManager";

/* --- EMBEDDED FILES --- */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t favicon_png_start[] asm("_binary_favicon_32x32_png_start");
extern const uint8_t favicon_png_end[]   asm("_binary_favicon_32x32_png_end");
extern const uint8_t logo_png_start[]    asm("_binary_logoBK_png_start");
extern const uint8_t logo_png_end[]      asm("_binary_logoBK_png_end");

/* --- Global State Management --- */
typedef enum {
    WIFI_STATUS_IDLE = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_FAIL_AUTH,
    WIFI_STATUS_FAIL_NOT_FOUND,
    WIFI_STATUS_FAIL_OTHER
} wifi_connect_status_t;

static int s_connect_status = WIFI_STATUS_IDLE;
static char s_connected_ip[16] = "0.0.0.0";
static char s_temp_ssid[33] = {0};
static char s_temp_pass[65] = {0};

/* NEW: Flags for Logic Control */
static bool s_is_provisioning = false; // True = AP Mode, False = Normal STA Mode
static int s_retry_num = 0;
static httpd_handle_t s_server = NULL; // Keep track of server handle
static TimerHandle_t s_reconnect_timer = NULL;
static TaskHandle_t s_reconnect_task_handle = NULL;

/* --- Function Prototypes --- */
void start_provisioning_mode(void);
void start_normal_mode(char *ssid, char *pass);
static httpd_handle_t start_webserver(void);
static void stop_webserver(httpd_handle_t server);
static void reconnect_timer_callback(TimerHandle_t xTimer);
esp_err_t load_wifi_credentials(char *ssid, char *pass, size_t max_len);

/* --- NVS Helper Functions --- */
esp_err_t save_wifi_credentials(const char *ssid, const char *pass) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(my_handle, "ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(my_handle, "password", pass);
    
    nvs_commit(my_handle);
    nvs_close(my_handle);
    return err;
}

esp_err_t load_wifi_credentials(char *ssid, char *pass, size_t max_len) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK) return err;

    size_t ssid_len = max_len;
    size_t pass_len = max_len;
    err = nvs_get_str(my_handle, "ssid", ssid, &ssid_len);
    if (err == ESP_OK) err = nvs_get_str(my_handle, "password", pass, &pass_len);

    nvs_close(my_handle);
    return err;
}

/* --- Task: Background Reconnect --- */
static void reconnect_task(void *pvParameter) {
    char ssid[33] = {0};
    char pass[65] = {0};

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (s_is_provisioning) {
            ESP_LOGI(TAG, "Auto-reconnect Task: Checking saved WiFi...");
            if (load_wifi_credentials(ssid, pass, sizeof(ssid)) == ESP_OK) {
                ESP_LOGI(TAG, "Found saved WiFi '%s'. Attempting background connection...", ssid);
                
                wifi_config_t sta_config = {0};
                strcpy((char *)sta_config.sta.ssid, ssid);
                strcpy((char *)sta_config.sta.password, pass);
                
                /* Important: APSTA mode allows concurrent STA connection attempts */
                esp_wifi_set_config(WIFI_IF_STA, &sta_config);
                esp_wifi_connect();
            }
        }
    }
}

static void reconnect_timer_callback(TimerHandle_t xTimer) {
    if (s_is_provisioning && s_reconnect_task_handle != NULL) {
        xTaskNotifyGive(s_reconnect_task_handle);
    }
}

/* --- Task: Switch to STA Mode --- */
static void switch_to_sta_task(void *pvParameter) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "Credentials saved. Switching to Station Mode (No Reboot)...");
    
    /* Load credentials we just saved (or use s_temp_*) */
    char ssid[33] = {0};
    char pass[65] = {0};
    load_wifi_credentials(ssid, pass, sizeof(ssid));
    
    /* Transition to Normal Mode */
    start_normal_mode(ssid, pass);
    
    vTaskDelete(NULL);
}

/* --- Event Handler --- */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        
        /* LOGIC A: If we are in Provisioning Mode (User entered pass on Web) */
        if (s_is_provisioning) {
            if (s_connect_status == WIFI_STATUS_CONNECTING) {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                ESP_LOGE(TAG, "Provisioning Connect Failed. Reason: %d", event->reason);
                
                if (event->reason == WIFI_REASON_AUTH_EXPIRE || 
                    event->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                    event->reason == WIFI_REASON_BEACON_TIMEOUT || 
                    event->reason == WIFI_REASON_AUTH_FAIL || 
                    event->reason == WIFI_REASON_ASSOC_FAIL || 
                    event->reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
                    s_connect_status = WIFI_STATUS_FAIL_AUTH;
                } else if (event->reason == WIFI_REASON_NO_AP_FOUND) {
                    s_connect_status = WIFI_STATUS_FAIL_NOT_FOUND;
                } else {
                    s_connect_status = WIFI_STATUS_FAIL_OTHER;
                }
                esp_wifi_disconnect();
            }
            /* If background reconnect failed -> Do nothing, stay in AP mode */
            else {
                ESP_LOGW(TAG, "Background reconnect failed. Still in AP Mode.");
            }
        } 
        /* LOGIC B: If we are in Normal Mode (Startup attempt) */
        else {
            if (s_retry_num < MAX_RETRY_COUNT) {
                s_retry_num++;
                ESP_LOGW(TAG, "Connect to saved WiFi failed. Retrying %d/%d...", s_retry_num, MAX_RETRY_COUNT);
                esp_wifi_connect();
            } else {
                ESP_LOGE(TAG, "Failed to connect to saved WiFi. Falling back to AP Mode.");
                /* Reset retry count and start provisioning */
                s_retry_num = 0;
                start_provisioning_mode();
            }
        }
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        sprintf(s_connected_ip, IPSTR, IP2STR(&event->ip_info.ip));
        s_connect_status = WIFI_STATUS_CONNECTED;
        s_retry_num = 0; // Reset retry counter on success
        ESP_LOGI(TAG, "Got IP: %s", s_connected_ip);

        /* LOGIC: If we got IP while in Provisioning Mode -> It means success! */
        if (s_is_provisioning) {
             ESP_LOGI(TAG, "Connection Successful! Switching to Normal Mode...");
             
             /* FIX: Increased Stack Size to 4096 to prevent Stack Overflow */
             xTaskCreate(switch_to_sta_task, "switch_sta", 4096, NULL, 5, NULL);
        }
    }
}

/* --- HTTP Handlers (Unchanged) --- */
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
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

static esp_err_t scan_handler(httpd_req_t *req) {
    wifi_scan_config_t scan_config = { .show_hidden = true, .scan_type = WIFI_SCAN_TYPE_ACTIVE };
    esp_wifi_scan_start(&scan_config, true);

    uint16_t ap_count = 10;
    wifi_ap_record_t ap_info[10];
    esp_wifi_scan_get_ap_records(&ap_count, ap_info);

    char *json_resp = malloc(2048);
    if(!json_resp) return ESP_FAIL;
    strcpy(json_resp, "[");
    for (int i = 0; i < ap_count; i++) {
        char entry[150];
        if (i > 0) strcat(json_resp, ",");
        sprintf(entry, "{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%d}", (char*)ap_info[i].ssid, ap_info[i].rssi, ap_info[i].authmode);
        strcat(json_resp, entry);
    }
    strcat(json_resp, "]");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_resp, strlen(json_resp));
    free(json_resp);
    return ESP_OK;
}

static esp_err_t connect_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    char ssid[32] = {0};
    char pass[64] = {0};
    
    char *ptr = strstr(buf, "\"ssid\":\"");
    if (ptr) { ptr += 8; char *end = strchr(ptr, '"'); if (end) strncpy(ssid, ptr, (end - ptr)); }
    ptr = strstr(buf, "\"password\":\"");
    if (ptr) { ptr += 12; char *end = strchr(ptr, '"'); if (end) strncpy(pass, ptr, (end - ptr)); }

    if (strlen(ssid) > 0) {
        ESP_LOGI(TAG, "Web Request: Testing Credentials for %s", ssid);
        strcpy(s_temp_ssid, ssid);
        strcpy(s_temp_pass, pass);
        
        wifi_config_t wifi_config;
        memset(&wifi_config, 0, sizeof(wifi_config_t));
        strcpy((char*)wifi_config.sta.ssid, ssid);
        strcpy((char*)wifi_config.sta.password, pass);
        
        s_connect_status = WIFI_STATUS_CONNECTING;
        esp_wifi_disconnect();
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        esp_wifi_connect();
        
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send(req, "Error: Missing SSID", HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req) {
    char json_resp[100];
    sprintf(json_resp, "{\"status\":%d,\"ip\":\"%s\"}", s_connect_status, s_connected_ip);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_resp, strlen(json_resp));
    return ESP_OK;
}

static esp_err_t save_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Client confirmed success. Saving to NVS...");
    if (save_wifi_credentials(s_temp_ssid, s_temp_pass) == ESP_OK) {
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send(req, "Saved", HTTPD_RESP_USE_STRLEN);
        xTaskCreate(switch_to_sta_task, "switch_sta", 4096, NULL, 5, NULL);
    } else {
        httpd_resp_send_500(req);
    }
    return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
    if (s_server != NULL) return s_server; // Already started
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    // httpd_handle_t server = NULL;

    if (httpd_start(&s_server, &config) == ESP_OK) {
        httpd_uri_t uris[] = {
            { "/", HTTP_GET, root_handler, NULL },
            { "/favicon.ico", HTTP_GET, favicon_handler, NULL },
            { "/logo.png", HTTP_GET, logo_handler, NULL },
            { "/scan", HTTP_GET, scan_handler, NULL },
            { "/connect", HTTP_POST, connect_handler, NULL },
            { "/status", HTTP_GET, status_handler, NULL },
            { "/save", HTTP_POST, save_handler, NULL }
        };
        for (int i = 0; i < sizeof(uris)/sizeof(httpd_uri_t); i++) {
            httpd_register_uri_handler(s_server, &uris[i]);
        }
        ESP_LOGI(TAG, "Web Server Started");
        return s_server;
    }
    return NULL;
}

static void stop_webserver(httpd_handle_t server) {
    if (server) {
        httpd_stop(server);
        s_server = NULL;
        ESP_LOGI(TAG, "Web Server Stopped");
    }
}

/* --- MODE 1: Provisioning Mode (APSTA + WebServer) --- */
void start_provisioning_mode(void) {
    ESP_LOGI(TAG, "Starting PROVISIONING MODE (AP)...");
    s_is_provisioning = true; // FLAG: We are in AP Mode

    /* Reset WiFi config to ensure clean slate */
    esp_wifi_stop();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    
    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = AP_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .channel = 1
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    start_webserver();

    /* START RECONNECT MECHANISM (Timer + Task) */
    if (s_reconnect_task_handle == NULL) {
        xTaskCreate(reconnect_task, "reconnect_task", 4096, NULL, 5, &s_reconnect_task_handle);
    }
    if (s_reconnect_timer == NULL) {
        s_reconnect_timer = xTimerCreate("reconnect_tmr", pdMS_TO_TICKS(RECONNECT_INTERVAL_MS), pdTRUE, NULL, reconnect_timer_callback);
    }
    if (s_reconnect_timer) xTimerStart(s_reconnect_timer, 0);

    ESP_LOGI(TAG, "AP Started. Connect to: '%s', password: '%s", AP_SSID, AP_PASS);
}

/* --- MODE 2: Normal Mode (STA only) --- */
void start_normal_mode(char *ssid, char *pass) {
    ESP_LOGI(TAG, "Starting NORMAL MODE (STA)...");
    s_is_provisioning = false; // FLAG: We are in Normal Mode

    /* 1. Stop Web Server (Free up RAM) */
    stop_webserver(s_server);

    /* 2. Stop WiFi to clear AP config */
    esp_wifi_stop();
    
    /* 3. Re-init for STA only */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t sta_config = {0};
    strcpy((char *)sta_config.sta.ssid, ssid);
    strcpy((char *)sta_config.sta.password, pass);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Attempting to connect to saved WiFi: %s", ssid);
    esp_wifi_connect();
}

void apmode_init(void) {
   /* 1. Initialize NVS (Safe Check) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    
    /* 2. Initialize Netif & Loop (Safe Check) */
    /* esp_netif_init() internally checks if already init, safe to call multiple times */
    esp_netif_init();
    /* esp_event_loop_create_default() returns ERR_INVALID_STATE if already created, we can ignore that error */
    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Event Loop init warning: %s", esp_err_to_name(loop_err));
    }
    
    /* 3. Create Interfaces ONLY if they don't exist */
    if (esp_netif_get_handle_from_ifkey("WIFI_AP_DEF") == NULL) {
        esp_netif_create_default_wifi_ap();
    }
    if (esp_netif_get_handle_from_ifkey("WIFI_STA_DEF") == NULL) {
        esp_netif_create_default_wifi_sta();
    }
    
    /* Register Event Handler ONCE here */
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    char ssid[33] = {0};
    char pass[65] = {0};
    
    /* Check NVS */
    if (load_wifi_credentials(ssid, pass, sizeof(ssid)) == ESP_OK) {
        start_normal_mode(ssid, pass);
    } else {
        start_provisioning_mode();
    }
}