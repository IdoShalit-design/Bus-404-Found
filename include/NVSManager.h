#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include <Preferences.h>
#include <Arduino.h>
#include "Structs.h"

#define MAX_NVS_TARGETS 6    // Max bus targets loadable from NVS
#define NVS_STOP_ID_LEN 16
#define NVS_LINE_LEN    8

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
    String getRoutes();
    void setRoutes(const String& routes);

    // Legacy getters (for backward compatibility)
    String getStopID();
    String getLineNumbers();

    /**
     * @brief Parses NVS stop ID and line numbers into a BusTarget array.
     * @param targets Output array to populate (must have room for MAX_NVS_TARGETS).
     * @param maxTargets Maximum number of targets to fill.
     * @return Number of targets populated, or 0 if NVS has no bus settings.
     */
    int loadTargets(BusTarget* targets, int maxTargets);

    /** @brief Returns the persistent stop ID buffer for a given index (valid after loadTargets). */
    const char* getLoadedStopId(int index = 0) const { return _stopIdBufs[index]; }

private:
    Preferences _prefs;

    // Persistent string buffers - pointers in BusTarget point into these
    char _stopIdBufs[MAX_NVS_TARGETS][NVS_STOP_ID_LEN];
    char _lineBufs[MAX_NVS_TARGETS][NVS_LINE_LEN];
};

#endif // NVS_MANAGER_H
