#include "STAmode.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include <stdio.h> // For sprintf

static const char *TAG = "MQTT_APP";
static esp_mqtt_client_handle_t s_client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT Disconnected");
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "Published ID=%d", event->msg_id);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT Error");
        break;
    default:
        break;
    }
}

void mqtt_app_start(const char *host, int port, const char *access_token) {
    if (s_client != NULL) {
        ESP_LOGW(TAG, "MQTT Client already started");
        return;
    }

    /* Construct the full URI string, e.g., "mqtt://app.coreiot.io:1883" */
    char uri[128];
    sprintf(uri, "mqtt://%s:%d", host, port);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .credentials.username = access_token, /* Thingsboard uses token as username */
        /* .credentials.authentication.password = "", // Password is usually empty for token auth */
    };

    ESP_LOGI(TAG, "Connecting to %s with Token: %s", uri, access_token);

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
}

int mqtt_app_publish(const char *topic, const char *data) {
    if (s_client == NULL) return -1;
    /* QoS 1 ensures delivery */
    return esp_mqtt_client_publish(s_client, topic, data, 0, 1, 0);
}