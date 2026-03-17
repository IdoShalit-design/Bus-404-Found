#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <Arduino.h>
#include "Config.h"

constexpr size_t MAX_WIFI_SSID_LEN = 33;
constexpr size_t MAX_WIFI_PASSWORD_LEN = 65;
constexpr size_t MAX_STATION_ID_LEN = 24;
constexpr size_t MAX_LINE_LEN = 16;
constexpr size_t MAX_DESTINATION_LEN = 48;
constexpr uint8_t CONFIG_SCHEMA_VERSION = 1;

struct WifiConfig {
    char ssid[MAX_WIFI_SSID_LEN];
    char password[MAX_WIFI_PASSWORD_LEN];
};

struct BusTargetConfig {
    char stationId[MAX_STATION_ID_LEN];
    char line[MAX_LINE_LEN];
    char destination[MAX_DESTINATION_LEN];
};

struct BusConfig {
    BusTargetConfig targets[MAX_RUNTIME_TARGETS];
    int targetCount;
};

struct RuntimeConfig {
    WifiConfig wifi;
    BusConfig bus;
};

bool loadRuntimeConfig(RuntimeConfig& outConfig, char* errorBuffer, size_t errorBufferLen);
const char* configErrorToDisplayMessage(const char* configError);

#endif
