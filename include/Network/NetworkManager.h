#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>
#include <Arduino.h>

/**
 * @brief Struct to hold WiFi credentials.
 * Using const char* instead of String for memory efficiency in embedded systems.
 */
struct WifiCredentials {
    const char* ssid;
    const char* password;

    // Constructor using initializer list
    WifiCredentials(const char* s, const char* p) : ssid(s), password(p) {}
};

class NetworkManager {
private:
    WifiCredentials _credentials; // Private member (naming convention: underscore prefix)

public:
    /**
     * @brief Construct a new Network Manager object
     * @param creds Pass by reference (&) to avoid unnecessary copying of the struct
     */
    NetworkManager(const WifiCredentials& creds);

    /**
     * @brief Connects to the WiFi network stored in the credentials
     * @return true if connected, false if timeout or error
     */
    bool connect_to_wifi();

    /**
     * @brief Scans for available networks and prints their details to Serial
     */
    void print_networks();

    /**
     * @brief Decodes the current WiFi status and prints a human-readable message
     */
    void print_wifi_status();
};

#endif