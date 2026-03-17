#include "ConfigLoader.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

namespace {

void setError(char* errorBuffer, size_t errorBufferLen, const char* message) {
    if (errorBuffer != nullptr && errorBufferLen > 0) {
        snprintf(errorBuffer, errorBufferLen, "%s", message);
    }
}

void copyBounded(char* dst, size_t dstSize, const char* src) {
    if (dst == nullptr || dstSize == 0) {
        return;
    }

    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

bool parseWifiConfig(RuntimeConfig& outConfig, char* errorBuffer, size_t errorBufferLen) {
    File wifiFile = LittleFS.open("/wifi_credentials.json", "r");
    if (!wifiFile) {
        setError(errorBuffer, errorBufferLen, "Missing /wifi_credentials.json");
        return false;
    }

    StaticJsonDocument<512> wifiDoc;
    DeserializationError wifiErr = deserializeJson(wifiDoc, wifiFile);
    wifiFile.close();

    if (wifiErr) {
        setError(errorBuffer, errorBufferLen, "Invalid wifi_credentials.json");
        return false;
    }

    JsonVariantConst versionValue = wifiDoc["version"];
    if (versionValue.isNull()) {
        setError(errorBuffer, errorBufferLen, "wifi_credentials.json missing version");
        return false;
    }

    int version = versionValue.as<int>();
    if (version != CONFIG_SCHEMA_VERSION) {
        setError(errorBuffer, errorBufferLen, "wifi_credentials.json schema version mismatch");
        return false;
    }

    const char* ssid = wifiDoc["ssid"];
    const char* password = wifiDoc["password"];

    if (ssid == nullptr || strlen(ssid) == 0) {
        setError(errorBuffer, errorBufferLen, "wifi_credentials.json missing ssid");
        return false;
    }

    if (password == nullptr || strlen(password) == 0) {
        setError(errorBuffer, errorBufferLen, "wifi_credentials.json missing password");
        return false;
    }

    copyBounded(outConfig.wifi.ssid, sizeof(outConfig.wifi.ssid), ssid);
    copyBounded(outConfig.wifi.password, sizeof(outConfig.wifi.password), password);

    return true;
}

bool parseBusTargetsConfig(RuntimeConfig& outConfig, char* errorBuffer, size_t errorBufferLen) {
    File targetsFile = LittleFS.open("/bus_targets.json", "r");
    if (!targetsFile) {
        setError(errorBuffer, errorBufferLen, "Missing /bus_targets.json");
        return false;
    }

    StaticJsonDocument<4096> targetsDoc;
    DeserializationError targetsErr = deserializeJson(targetsDoc, targetsFile);
    targetsFile.close();

    if (targetsErr) {
        setError(errorBuffer, errorBufferLen, "Invalid bus_targets.json");
        return false;
    }

    JsonVariantConst versionValue = targetsDoc["version"];
    if (versionValue.isNull()) {
        setError(errorBuffer, errorBufferLen, "bus_targets.json missing version");
        return false;
    }

    int version = versionValue.as<int>();
    if (version != CONFIG_SCHEMA_VERSION) {
        setError(errorBuffer, errorBufferLen, "bus_targets.json schema version mismatch");
        return false;
    }

    JsonArrayConst targets = targetsDoc["targets"].as<JsonArrayConst>();
    if (targets.isNull()) {
        setError(errorBuffer, errorBufferLen, "bus_targets.json missing targets array");
        return false;
    }

    if (targets.size() == 0) {
        setError(errorBuffer, errorBufferLen, "targets array cannot be empty");
        return false;
    }

    if (targets.size() > MAX_RUNTIME_TARGETS) {
        setError(errorBuffer, errorBufferLen, "Too many targets in bus_targets.json");
        return false;
    }

    int idx = 0;
    for (JsonObjectConst targetObj : targets) {
        const char* stationId = targetObj["stationId"];
        const char* line = targetObj["line"];
        const char* destination = targetObj["destination"];

        if (stationId == nullptr || strlen(stationId) == 0) {
            setError(errorBuffer, errorBufferLen, "Target missing stationId");
            return false;
        }
        if (line == nullptr || strlen(line) == 0) {
            setError(errorBuffer, errorBufferLen, "Target missing line");
            return false;
        }
        if (destination == nullptr || strlen(destination) == 0) {
            setError(errorBuffer, errorBufferLen, "Target missing destination");
            return false;
        }

        copyBounded(outConfig.bus.targets[idx].stationId, sizeof(outConfig.bus.targets[idx].stationId), stationId);
        copyBounded(outConfig.bus.targets[idx].line, sizeof(outConfig.bus.targets[idx].line), line);
        copyBounded(outConfig.bus.targets[idx].destination, sizeof(outConfig.bus.targets[idx].destination), destination);

        idx++;
    }

    outConfig.bus.targetCount = idx;
    return true;
}

}  // namespace

bool loadRuntimeConfig(RuntimeConfig& outConfig, char* errorBuffer, size_t errorBufferLen) {
    if (!LittleFS.begin(false)) {
        setError(errorBuffer, errorBufferLen, "LittleFS mount failed");
        return false;
    }

    if (!parseWifiConfig(outConfig, errorBuffer, errorBufferLen)) {
        return false;
    }

    if (!parseBusTargetsConfig(outConfig, errorBuffer, errorBufferLen)) {
        return false;
    }

    return true;
}

const char* configErrorToDisplayMessage(const char* configError) {
    if (configError == nullptr || configError[0] == '\0') {
        return "Config Error";
    }

    if (strstr(configError, "LittleFS mount failed") != nullptr) {
        return "FS Mount Failed";
    }
    if (strstr(configError, "wifi_credentials.json") != nullptr) {
        return "WiFi JSON Error";
    }
    if (strstr(configError, "bus_targets.json") != nullptr) {
        return "Bus JSON Error";
    }
    if (strstr(configError, "targets") != nullptr) {
        return "Targets Error";
    }

    return "Config Error";
}
