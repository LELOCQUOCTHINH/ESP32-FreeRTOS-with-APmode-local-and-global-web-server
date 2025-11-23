#include "relay.h"
#include "esp_log.h"

static const char *TAG = "Relay";

/* Simple internal storage to keep track of active levels for up to 10 relays.
   Index corresponds to internal slot, not GPIO number directly to save space.
   For a more robust solution, a linked list or hash map could be used, 
   but arrays are faster and simpler for embedded. 
*/
#define MAX_RELAYS 2

typedef struct {
    gpio_num_t gpio_num;
    relay_active_level_t active_level;
    bool is_initialized;
} relay_config_t;

static relay_config_t s_relays[MAX_RELAYS] = {0};

/* Helper: Find existing relay config or next empty slot */
static int find_relay_slot(gpio_num_t gpio_num) {
    for (int i = 0; i < MAX_RELAYS; i++) {
        if (s_relays[i].is_initialized && s_relays[i].gpio_num == gpio_num) {
            return i; // Found existing
        }
    }
    return -1;
}

static int find_empty_slot() {
    for (int i = 0; i < MAX_RELAYS; i++) {
        if (!s_relays[i].is_initialized) {
            return i; // Found empty slot
        }
    }
    return -1;
}

esp_err_t relay_init(gpio_num_t gpio_num, relay_active_level_t active_level) {
    /* 1. Configure GPIO */
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT; // Input to read state for toggle
    io_conf.pin_bit_mask = (1ULL << gpio_num);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) return err;

    /* 2. Save config internally */
    int slot = find_relay_slot(gpio_num);
    if (slot == -1) {
        slot = find_empty_slot();
        if (slot == -1) {
            ESP_LOGE(TAG, "Max relays reached (%d)", MAX_RELAYS);
            return ESP_ERR_NO_MEM;
        }
    }
    
    s_relays[slot].gpio_num = gpio_num;
    s_relays[slot].active_level = active_level;
    s_relays[slot].is_initialized = true;

    /* 3. Set initial state to OFF */
    // If Active High -> OFF is 0
    // If Active Low  -> OFF is 1
    int off_level = (active_level == RELAY_ACTIVE_HIGH) ? 0 : 1;
    gpio_set_level(gpio_num, off_level);

    ESP_LOGI(TAG, "Relay initialized on GPIO %d (Active Level: %s)", 
             gpio_num, active_level == RELAY_ACTIVE_HIGH ? "HIGH" : "LOW");
    return ESP_OK;
}

esp_err_t relay_on(gpio_num_t gpio_num) {
    int slot = find_relay_slot(gpio_num);
    if (slot == -1) {
        ESP_LOGE(TAG, "Relay on GPIO %d not initialized", gpio_num);
        return ESP_ERR_INVALID_STATE;
    }

    int level = (s_relays[slot].active_level == RELAY_ACTIVE_HIGH) ? 1 : 0;
    return gpio_set_level(gpio_num, level);
}

esp_err_t relay_off(gpio_num_t gpio_num) {
    int slot = find_relay_slot(gpio_num);
    if (slot == -1) {
        ESP_LOGE(TAG, "Relay on GPIO %d not initialized", gpio_num);
        return ESP_ERR_INVALID_STATE;
    }

    int level = (s_relays[slot].active_level == RELAY_ACTIVE_HIGH) ? 0 : 1;
    return gpio_set_level(gpio_num, level);
}

esp_err_t relay_toggle(gpio_num_t gpio_num) {
    relay_state_t current_state = relay_get_state(gpio_num);
    if (current_state == RELAY_ON) {
        return relay_off(gpio_num);
    } else {
        return relay_on(gpio_num);
    }
}

relay_state_t relay_get_state(gpio_num_t gpio_num) {
    int slot = find_relay_slot(gpio_num);
    if (slot == -1) return RELAY_OFF;

    int current_level = gpio_get_level(gpio_num);
    
    if (s_relays[slot].active_level == RELAY_ACTIVE_HIGH) {
        return (current_level == 1) ? RELAY_ON : RELAY_OFF;
    } else {
        return (current_level == 0) ? RELAY_ON : RELAY_OFF;
    }
}