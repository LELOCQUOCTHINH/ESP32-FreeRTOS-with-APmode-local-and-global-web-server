#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start MQTT Client with specified connection details.
 * * @param host MQTT Broker Host (e.g., "app.coreiot.io")
 * @param port MQTT Broker Port (e.g., 1883)
 * @param access_token Access Token (used as Username)
 */
void mqtt_app_start(const char *host, int port, const char *access_token);

/**
 * @brief Publish data to a specific topic.
 * * @param topic The topic string (e.g., "v1/devices/me/telemetry")
 * @param data The payload string (e.g., "{\"temp\": 25}")
 * @return int Message ID on success, -1 on failure.
 */
int mqtt_app_publish(const char *topic, const char *data);

#ifdef __cplusplus
}
#endif