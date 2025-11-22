# DHT11 Sensor Component

A simple, lightweight ESP-IDF component to read **temperature** and **humidity** from the DHT11 sensor using a single GPIO pin (1-wire protocol).

## Hardware Connection

- **VCC**: 3.3V or 5V  
- **GND**: GND  
- **DATA**: Any GPIO pin (e.g., GPIO 4)  
- **Recommendation**: Use a **4.7kΩ to 10kΩ pull-up resistor** between VCC and DATA for stable readings.

## Usage Guide

### 1. Include Header
```c
#include "dht11.h"
```

### 2. Initialize & Read
```c
void app_main(void) {
    // Initialize on GPIO 4
    dht11_init(GPIO_NUM_4);

    while (1) {
        dht11_reading_t data = dht11_read();

        if (data.temperature != -1) {
            printf("Temp: %.1f °C, Hum: %.1f %%\n", data.temperature, data.humidity);
        } else {
            printf("Failed to read from DHT11\n");
        }

        // DHT11 requires at least 2 seconds between readings
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```