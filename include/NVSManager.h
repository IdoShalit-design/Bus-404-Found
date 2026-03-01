#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include <Preferences.h>
#include <Arduino.h>

/**
 * @brief Manages persistent storage of configuration data using ESP32 NVS.
 *
 * Stores WiFi credentials (SSID, Password) and bus display settings
 * (StopID, LineNumbers) using the Preferences library.
 */
class NVSManager {
public:
    /**
     * @brief Initializes the NVS namespace. Must be called before any get/set.
     */
    void begin();

    // --- WiFi Credentials ---
    String getSSID();
    String getPassword();
    void setSSID(const String& ssid);
    void setPassword(const String& password);
    bool hasWiFiCredentials();

    // --- Bus Display Settings ---
    String getStopID();
    String getLineNumbers();
    void setStopID(const String& stopId);
    void setLineNumbers(const String& lines);

private:
    Preferences _prefs;
};

#endif // NVS_MANAGER_H
