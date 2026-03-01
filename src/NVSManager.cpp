#include "NVSManager.h"

static const char* NVS_NAMESPACE = "bus404";

void NVSManager::begin() {
    _prefs.begin(NVS_NAMESPACE, false); // false = read/write mode
    Serial.println("[NVSManager] NVS initialized");
}

// --- WiFi Credentials ---

String NVSManager::getSSID() {
    return _prefs.getString("ssid", "");
}

String NVSManager::getPassword() {
    return _prefs.getString("password", "");
}

void NVSManager::setSSID(const String& ssid) {
    _prefs.putString("ssid", ssid);
}

void NVSManager::setPassword(const String& password) {
    _prefs.putString("password", password);
}

bool NVSManager::hasWiFiCredentials() {
    return getSSID().length() > 0;
}

// --- Bus Display Settings ---

String NVSManager::getStopID() {
    return _prefs.getString("stopId", "");
}

String NVSManager::getLineNumbers() {
    return _prefs.getString("lines", "");
}

void NVSManager::setStopID(const String& stopId) {
    _prefs.putString("stopId", stopId);
}

void NVSManager::setLineNumbers(const String& lines) {
    _prefs.putString("lines", lines);
}
