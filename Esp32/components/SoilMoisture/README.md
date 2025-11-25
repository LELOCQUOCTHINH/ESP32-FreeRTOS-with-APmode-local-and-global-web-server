# Soil Moisture Sensor Component (ADC)

This component provides a driver to read analog soil moisture values using the ADC interface on ESP32 family microcontrollers. It supports raw data reading and percentage conversion with calibration.

## Hardware Connection

Connect the **Analog Output (AO)** pin of your soil moisture sensor to any ADC-supported GPIO pin on the ESP32.

- **VCC**: 3.3V or 5V (depending on sensor specs)  
- **GND**: GND  
- **AO**: ADC Pin (e.g., GPIO 34, 35, 36, 39 – input-only pins recommended)

## Usage Guide

### 1. Include Header
```c
#include "soil_sensor.h"
```

### 2. Initialization
```c
void app_main(void) {
    if (soil_sensor_init(GPIO_NUM_01) == ESP_OK) {
        printf("Soil Sensor initialized successfully!\n");
    } else {
        printf("Failed to initialize Soil Sensor. Check GPIO validity.\n");
    }
}
```

### 3. Reading Data

```c
// Read Raw Value (Inverse logic: High = Dry, Low = Wet for resistive sensors)
int raw_value = soil_sensor_read_raw();

// Read Percentage (0% = Dry, 100% = Wet)
int moisture_percent = soil_sensor_read_percentage();

printf("Moisture: %d (Raw) | %d%%\n", raw_value, moisture_percent);
```

## Calibration (Highly Recommended)

Soil moisture sensors vary significantly. Manual calibration is required for accurate percentage readings.

1. **Find Dry Value (`SOIL_DRY_VALUE`)**  
   → Expose sensor to air → Read raw value → Update macro

2. **Find Wet Value (`SOIL_WET_VALUE`)**  
   → Submerge sensor in water → Read raw value → Update macro

3. **Update macros in `soil_sensor.c`:**
```c
#define SOIL_DRY_VALUE  3200  // Example: raw value when completely dry
#define SOIL_WET_VALUE  1200  // Example: raw value when fully submerged in water
```

## API Reference

| Function                                    | Description                                                                 |
|---------------------------------------------|-----------------------------------------------------------------------------|
| `esp_err_t soil_sensor_init(gpio_num_t gpio_num)` | Initializes the ADC unit for the specified GPIO                            |
| `int soil_sensor_read_raw(void)`            | Returns the raw 12-bit ADC reading (average of multiple samples)           |
| `int soil_sensor_read_percentage(void)`     | Returns moisture percentage (0–100%) based on calibrated `SOIL_DRY/WET_VALUE` |

## Notes

- Most resistive soil moisture sensors have **inverse logic**: higher raw value = drier soil.
- Capacitive sensors (e.g., v2.0) usually have direct logic (higher = wetter). Adjust calibration accordingly.

Enjoy accurate and reliable soil moisture readings!