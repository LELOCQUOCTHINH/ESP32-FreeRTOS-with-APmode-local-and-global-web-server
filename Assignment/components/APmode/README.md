# APMode Component (WiFi Provisioning Manager)

This component provides a **professional, production-ready WiFi provisioning solution** for ESP32 devices. It implements a robust self-healing state machine with automatic recovery – no user intervention required even when WiFi is temporarily lost.

<div align="center">
  <img src="https://github.com/user-attachments/assets/d81fff97-5068-4b04-859e-05bb281beead" width="500" alt="APMode Web Interface"/>
  <br/><br/>
  <em>Responsive mobile-friendly Web Interface for WiFi Configuration</em>
</div>

## Key Features

- **Smart Auto-Reconnection** – Connects to last known network stored in NVS on every boot  
- **Robust Fallback** – Retries `MAX_RETRY_COUNT` (default 10) times, then switches to SoftAP  
- **Background Auto-Recovery (New!)** – While in AP mode, every `RECONNECT_INTERVAL_MS` (default 60s) it silently tries to reconnect to the saved network and switches back automatically when WiFi returns  
- **Responsive Web Interface** – Embedded mobile-friendly HTML/JS UI  
- **Secure Verify-before-Save** – Credentials are tested before being saved (prevents boot loops)  
- **All Assets Embedded** – HTML, CSS, JS, favicon, logo compiled into firmware (no SPIFFS needed)

## Folder Structure

```
components/APmode/
├── CMakeLists.txt         # Build config & asset embedding
├── apmode.c               # Core logic (state machine, WiFi events, NVS)
├── index.html             # Web UI
├── images/
│   └── favicon-32x32.png   # Favicon (embedded)
|   └── logoBK.png         # Logo (embedded)
├── README.md              # This file
└── include/
    └── apmode.h           # Public API header
```

## Integration Guide

Just copy the whole `APmode` folder into your project's `components/` directory.

```c
#include "apmode.h"

void app_main(void)
{
    // 1. Initialize system resources
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 2. Start the WiFi manager – that's all!
    apmode_init();
}
```

## Configuration (apmode.c)

| Macro                    | Default Value       | Description                                           |
|--------------------------|---------------------|-------------------------------------------------------|
| `AP_SSID`                | `"ESP32_Config_Wifi"` | SoftAP SSID                                          |
| `AP_PASS`                | `"12345678"`        | SoftAP password (WPA2)                                |
| `MAX_RETRY_COUNT`        | `10`                | Retries before entering AP mode                       |
| `RECONNECT_INTERVAL_MS`  | `60000` (1 minute)  | Background reconnect check interval while in AP mode  |

## Operational Workflow (State Machine)

1. **Boot** → Check NVS for saved credentials  
   - Found → **Normal Mode**  
   - Not found → **Provisioning Mode**

2. **Normal Mode**  
   - Try to connect → success → stay connected  
   - Disconnect → retry up to `MAX_RETRY_COUNT`  
   - All retries fail → switch to **Provisioning Mode**

3. **Provisioning Mode** (SoftAP + background recovery)  
   - Starts AP (`192.168.4.1`) + Web Server  
   - User configures new WiFi → tested → saved → reboot to Normal Mode  
   - **Background**: every minute silently tries saved network → if available → auto switch back

## Web Server API Endpoints

| Method | Endpoint   | Description                                      |
|--------|------------|--------------------------------------------------|
| GET    | `/`        | Main `index.html` page                           |
| GET    | `/scan`    | WiFi scan → returns JSON list                    |
| POST   | `/connect` | Test connection with provided `{ssid, password}` |
| GET    | `/status`  | Current status & IP (used for polling)           |
| POST   | `/save`    | Save tested credentials to NVS + switch mode    |
| POST   | `/cleanup` | Signal successful UI load (optional)            |

## Troubleshooting

- **Stuck in AP mode?** → Saved WiFi is offline or wrong password. Connect to the AP and configure again.
- **Build error `esp_http_server.h` not found** → Add `REQUIRES esp_http_server esp_wifi` in `CMakeLists.txt`
- **Web UI not loading** → Connect to ESP32 AP, disable mobile data, open http://192.168.4.1

## License

Open-source from Le Loc Quoc Thinh - HCMUT VNU (Bach Khoa Univerisy HCMC) – free for personal and commercial projects.

Enjoy rock-solid WiFi provisioning!