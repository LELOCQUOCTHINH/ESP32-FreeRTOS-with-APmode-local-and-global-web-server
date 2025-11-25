# STAmode Component

This module manages **Station Mode (STA)** for the ESP32. It handles Wi-Fi connection, runs a **local Web Server** for monitoring & control, and maintains an **MQTT** connection for telemetry and remote commands.

## Directory Structure

```
components/STAmode/
├── include/
│   └── STAmode.h           # Public API declarations and structs
├── STAmode.c               # Core source (Web server, MQTT client, data handling)
├── sta_index.html          # Embedded Web Dashboard (compiled into firmware)
├── images/
│   └── favicon-32x32.png   # Favicon (embedded)
|   └── logoBK.png         # Logo (embedded)
└── README.md               # This file
```

## Key Features

### 1. Local Web Server
- Runs on **port 80**
- Serves a responsive Dashboard (`/`)
- RESTful JSON API for sensors, settings and control

### 2. MQTT Client
- Secure TLS connection (default port 8883)
- Periodic telemetry publishing
- Subscribes to RPC/command topics
- Automatic reconnect

### 3. Configuration Management
- Stores Auto-Mode thresholds (temp, hum, soil) in **NVS**
- Full sync between Web UI and remote cloud

## Usage

### 1. Initialization (call after Wi-Fi obtains an IP)

```c
#include "STAmode.h"

stamode_start(
    "your-broker.s1.eu.hivemq.cloud",   // Broker URL
    8883,                               // Port (TLS)
    "your-username",                    // MQTT username
    "your-password"                     // MQTT password
);
```

### 2. Stopping the module (e.g., when switching back to AP mode)

```c
stamode_stop();   // Stops HTTP server & MQTT client, frees resources
```

### 3. Updating data (call from your sensor / control tasks)

```c
// Update local Web UI
stamode_update_http_data(temp, hum, soil, relay_state, mode);

// Publish to MQTT (only when connected)
stamode_publish_mqtt(temp, hum, soil, relay_state, mode);
```

## Local Web API Endpoints

| Method | URI                  | Description                                   |
|--------|----------------------|-----------------------------------------------|
| GET    | `/`                  | Main Dashboard (sta_index.html)               |
| GET    | `/api/sensors`       | Current sensor values (JSON)                  |
| GET    | `/api/settings`      | Current Auto-Mode configuration               |
| POST   | `/api/settings`      | Update Auto-Mode thresholds                   |
| POST   | `/api/control`       | Switch Auto / Manual mode                     |
| POST   | `/api/relay/toggle`  | Toggle relay (only works in Manual mode)      |
| GET    | `/favicon.ico`       | Embedded favicon                              |
| GET    | `/logo.png`          | Embedded logo                                 |

## Dependencies (add to component’s `CMakeLists.txt`)

```cmake
REQUIRES esp_http_server esp_wifi mqtt nvs_flash
```

## Important Notes

- **Asset embedding** – `sta_index.html`, `favicon-32x32.png` and `logoBK.png` must be binary-embedded (see `CMakeLists.txt` with `EMBED_FILES`).
- All public functions are **FreeRTOS-safe** – call them from tasks with sufficient stack size.
- The component automatically starts the HTTP server and MQTT client **only once** when Wi-Fi gets an IP.

Enjoy a clean, responsive local dashboard + reliable cloud connectivity!