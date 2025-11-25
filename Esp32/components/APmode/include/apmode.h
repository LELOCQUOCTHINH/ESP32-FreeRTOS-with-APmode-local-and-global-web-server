#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the WiFi Provisioning Manager (APmode Component).
 * * This function will automatically:
 * 1. Check NVS for existing WiFi credentials.
 * 2. If found: Attempt to connect (Normal Mode).
 * 3. If not found or connection fails: Start SoftAP + Web Server (Provisioning Mode).
 */
void apmode_init(void);

#ifdef __cplusplus
}
#endif