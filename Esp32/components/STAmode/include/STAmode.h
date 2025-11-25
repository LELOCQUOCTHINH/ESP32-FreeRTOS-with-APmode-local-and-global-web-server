#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- DATA STRUCTURES --- */
/* Message Types for Update Queue */
typedef enum {
    MSG_TYPE_SENSOR_DATA,
    MSG_TYPE_CONFIG_JSON
} update_msg_type_t;

/* Struct Sensor Data */
typedef struct {
    float temp;
    float hum;
    int soil;
    int relay_state;
    int mode; 
} sensor_data_t;

/* Struct Update Message (Union để tiết kiệm) */
typedef struct {
    update_msg_type_t type;
    union {
        sensor_data_t sensor;
        char *config_json; // Pointer to JSON string (malloced)
    } data;
} update_message_t;

/* Struct Device Control (Cho Relay Task) */
typedef struct {
    int command_id; 
} device_control_t;

void stamode_mqtt_publish_config_now();

void stamode_get_config(int *t_en, float *t_th, int *t_op, 
                        int *h_en, float *h_th, int *h_op, 
                        int *s_en, int *s_th, int *s_op);

void stamode_apply_config(const char *json_str);

void stamode_get_sensor_values(float *temp, float *hum, int *soil);

void stamode_start(const char *broker_url, int mqtt_port, const char *user, const char *pass);

void stamode_stop(void);

/* Hàm này CHỈ cập nhật dữ liệu cho Local Web Server (HTTP Packet) */
void stamode_update_http_data(float temp, float hum, int soil, int relay, int mode);

/* Hàm này CHỈ thực hiện gửi tin nhắn MQTT (MQTT Packet) */
void stamode_publish_mqtt(float temp, float hum, int soil, int relay, int mode);

/* Hàm cập nhật nhanh trạng thái Relay cho Web (dùng khi bấm nút) */
void stamode_update_relay_status_http(int relay_state, int mode);

#ifdef __cplusplus
}
#endif