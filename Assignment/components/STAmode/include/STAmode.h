#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

void stamode_start(const char *mqtt_broker_uri, int mqtt_port, const char *mqtt_token);

/* Hàm này CHỈ cập nhật dữ liệu cho Local Web Server (HTTP Packet) */
void stamode_update_http_data(float temp, float hum, int soil, int relay, int mode);

/* Hàm này CHỈ thực hiện gửi tin nhắn MQTT (MQTT Packet) */
void stamode_publish_mqtt(float temp, float hum, int soil, int relay, int mode);

/* Hàm cập nhật nhanh trạng thái Relay cho Web (dùng khi bấm nút) */
void stamode_update_relay_status_http(int relay_state, int mode);

#ifdef __cplusplus
}
#endif