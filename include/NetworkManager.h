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

// Renamed off "NetworkManager" for ESP-IDF 5.x / arduino-esp32 3.x: the core now
// defines its own global `class NetworkManager` in libraries/Network, which
// arrives here transitively through <WiFi.h> and collides with this one. Nothing
// instantiates this class today -- App.cpp includes this header for the free
// function ensureWiFiConnected() below -- so this is a rename, not a redesign.
class WifiConnectionManager {
private:
    WifiCredentials _credentials; // Private member (naming convention: underscore prefix)

public:
    /**
     * @brief Construct a new Wifi Connection Manager object
     * @param creds Pass by reference (&) to avoid unnecessary copying of the struct
     */
    WifiConnectionManager(const WifiCredentials& creds);

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

/**
 * @brief Ensures WiFi is connected using provided credentials.
 * @param ssid WiFi SSID.
 * @param password WiFi password.
 * @param timeoutMs Reconnect timeout in milliseconds.
 * @return true if connected, false otherwise.
 */
bool ensureWiFiConnected(const char* ssid, const char* password, unsigned long timeoutMs = 10000);

#endif