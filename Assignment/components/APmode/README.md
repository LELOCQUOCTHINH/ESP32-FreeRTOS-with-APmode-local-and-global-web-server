# APMode Component (WiFi Provisioning Manager)

This component provides a professional, production-ready **WiFi Provisioning** solution for ESP32 devices. It implements a captive portal mechanism using SoftAP and an embedded Web Server to allow users to configure WiFi credentials dynamically.

## Features

- **Smart Auto-Reconnection**: Automatically attempts to connect to the last known WiFi network stored in NVS (Non-Volatile Storage) upon boot.
- **Robust Fallback Mechanism**: If the saved network is unavailable or connection fails after `MAX_RETRY_COUNT` (default: 5), the device automatically switches to Provisioning Mode (SoftAP).
- **Responsive Web Interface**: A mobile-friendly, embedded HTML/JS web app for scanning networks and entering credentials.
- **Secure Verification Logic**: Implements a "Verify-before-Save" workflow. Credentials are only saved to NVS after a successful connection test, preventing boot loops caused by incorrect passwords.
- **Embedded Assets**: All static assets (HTML, CSS, JS, Favicon, Logo) are binary-embedded directly into the firmware image, eliminating the need for an external filesystem (SPIFFS/LittleFS).
- **User-Friendly Feedback**: The Web UI provides real-time status updates via polling (scanning, connecting, success/failure) and visual cues (spinners, alerts).

## Folder Structure

```
components/APmode/
├── CMakeLists.txt          # Build configuration & Asset embedding rules
├── apmode.c                # Core C logic (WiFi Event Handling, NVS, State Machine)
├── index.html              # Frontend UI (Single-page application)
├── images/
│   ├── favicon-32x32.png   # Browser Tab Icon
│   └── logoBK.png          # Branding Logo
├── README.md               # This documentation
└── include/
    └── apmode.h            # Public API header
```

## Integration Guide

### 1. Copy Component
Copy the entire `APmode` directory into your project's `components/` folder.

### 2. Include Header
In your main application file (e.g., `main.c`), include the component header:

```c
#include "apmode.h"
```

### 3. Initialization
Call `apmode_init()` inside your `app_main()` function.  
**Note:** Ensure that system services (NVS, Netif, Event Loop) are initialized before calling this function.

```c
void app_main(void) {
    // 1. Initialize NVS (Required for WiFi driver & Credential storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Initialize Network Interface & Event Loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Start the WiFi Manager
    apmode_init();
}
```

## Configuration

You can customize the behavior by modifying macros in `apmode.c`:

| Macro             | Default Value       | Description                                      |
|-------------------|---------------------|--------------------------------------------------|
| `AP_SSID`         | `"ESP32_Config_Wifi"` | The SSID name of the SoftAP                     |
| `AP_PASS`         | `"12345678"`        | The password for the SoftAP (WPA2)              |
| `MAX_RETRY_COUNT` | `5`                 | Number of connection attempts before falling back to AP Mode |

## Operational Workflow (State Machine)

1. **Boot Up**: The device initializes and checks NVS for ssid and password.
2. **Credential Check**:
   - Found → Enters Normal Mode (Station)
   - Not Found → Enters Provisioning Mode (SoftAP)
3. **Normal Mode**:
   - Attempts to connect to the saved WiFi
   - If disconnected, retries up to `MAX_RETRY_COUNT`
   - If retry limit is reached → automatically switches to Provisioning Mode
4. **Provisioning Mode**:
   - Starts SoftAP with IP `192.168.4.1`
   - Starts Web Server
   - User connects to SoftAP and opens the Web UI
   - User enters new credentials → Device attempts to connect (without saving yet)
   - **If Success**: Saves credentials to NVS → Reboots → Enters Normal Mode
   - **If Failure**: Remains in AP Mode and reports error to Web UI

## API Endpoints (Internal Web Server)

| Method | Endpoint     | Description                                           |
|--------|--------------|-------------------------------------------------------|
| GET    | `/`          | Returns the main `index.html` page                    |
| GET    | `/scan`      | Performs a WiFi scan and returns a JSON list of networks |
| POST   | `/connect`   | Receives `{ssid, password}` JSON and attempts a temporary connection |
| GET    | `/status`    | Returns the current connection status & IP (used for UI polling) |
| POST   | `/save`      | Commits the temporary credentials to NVS and triggers a system reboot |
| GET    | `/logo.png`  | Returns the embedded logo image                       |
| GET    | `/favicon.ico`| Returns the embedded favicon                         |

## Troubleshooting

- **Build Error**: `fatal error: esp_http_server.h: No such file...`  
  → Ensure `REQUIRES esp_http_server` is present in `components/APmode/CMakeLists.txt`.

- **Build Error**: `linker input file unused because linking not done`  
  → Check `EMBED_TXTFILES` and `EMBED_FILES` syntax in `CMakeLists.txt`.

- **Device keeps rebooting (Boot Loop)**  
  → This component prevents boot loops by validating passwords before saving. If you manually modified NVS, erase the flash:  
  ```bash
  idf.py -p PORT erase-flash
  ```

- **Web UI not loading**  
  - Ensure you are connected to the ESP32's SoftAP  
  - Disable mobile data on your phone (some phones prefer mobile data over WiFi with no internet)  
  - Navigate manually to `http://192.168.4.1`

## License

This component is open-source from Le Loc Quoc Thinh - HCMUT (Bach Khoa University HCMC) and available for use in personal and commercial projects.

---

Enjoy a smooth and reliable WiFi provisioning experience!