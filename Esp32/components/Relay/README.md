# Relay Control Component

A flexible and robust ESP-IDF component for controlling relays on ESP32. Supports both **Active High** and **Active Low** logic levels (phổ biến nhất trên các module relay giá rẻ).

## Features

- **Active Level Configuration**: Works with relays that trigger on HIGH or LOW signals  
- **State Management**: Turn ON, OFF, and TOGGLE the relay  
- **State Feedback**: Query current relay state (ON/OFF) anytime  

## Hardware Connection

Connect the relay module control pin (IN / JD-VCC pin) to any GPIO on ESP32.

- **VCC**: 3.3V or 5V (check your relay module specs)  
- **GND**: GND  
- **IN**: Any GPIO (e.g., GPIO 5, 18, etc.)

> Most cheap relay modules are **Active Low** (0 = ON, 1 = OFF)

## Usage Guide

### 1. Include Header
```c
#include "relay.h"
```

### 2. Initialize

You **must** specify the active logic level during initialization.

```c
// Example 1: Active High relay (1 = ON, 0 = OFF)
relay_init(GPIO_NUM_5, RELAY_ACTIVE_HIGH);

// Example 2: Active Low relay (0 = ON, 1 = OFF) – Most common
relay_init(GPIO_NUM_18, RELAY_ACTIVE_LOW);
```

### 3. Control

```c
// Turn ON
relay_on(GPIO_NUM_18);

// Turn OFF
relay_off(GPIO_NUM_18);

// Toggle (invert current state)
relay_toggle(GPIO_NUM_18);
```

### 4. Check Status

```c
if (relay_get_state(GPIO_NUM_18) == RELAY_ON) {
    printf("Relay is ON\n");
} else {
    printf("Relay is OFF\n");
}
```

## Full Example (app_main)

```c
void app_main(void)
{
    // Initialize as Active Low (common for cheap modules)
    relay_init(GPIO_NUM_18, RELAY_ACTIVE_LOW);

    while (1) {
        relay_on(GPIO_NUM_18);
        printf("Relay ON\n");
        vTaskDelay(pdMS_TO_TICKS(2000));

        relay_off(GPIO_NUM_18);
        printf("Relay OFF\n");
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Or use toggle
        relay_toggle(GPIO_NUM_18);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## API Summary

| Function                          | Description                              |
|-----------------------------------|------------------------------------------|
| `relay_init(gpio_num_t gpio, relay_active_t active_level)` | Initialize GPIO and set active logic |
| `relay_on(gpio_num_t gpio)`       | Turn relay ON                           |
| `relay_off(gpio_num_t gpio)`      | Turn relay OFF                          |
| `relay_toggle(gpio_num_t gpio)`   | Toggle relay state                      |
| `relay_get_state(gpio_num_t gpio)`| Return `RELAY_ON` or `RELAY_OFF`        |

## Number of Relay

You can define maximum number of your relays in relay.c following this

```c
#define MAX_RELAYS 1
```

Simple, safe, and works perfectly with all common relay modules!
