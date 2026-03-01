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

String NVSManager::getRoutes() {
    return _prefs.getString("routes", "");
}

void NVSManager::setRoutes(const String& routes) {
    _prefs.putString("routes", routes);
}

// Legacy getters - build from routes string for backward compat
String NVSManager::getStopID() {
    // Return the first stop ID from routes (e.g. "1570" from "1570:7, 3541:19")
    String routes = getRoutes();
    int colon = routes.indexOf(':');
    if (colon > 0) return routes.substring(0, colon);
    return _prefs.getString("stopId", "");  // fallback to old key
}

String NVSManager::getLineNumbers() {
    // Build comma-separated lines from routes
    String routes = getRoutes();
    if (routes.length() == 0) return _prefs.getString("lines", "");  // fallback to old key
    String result = "";
    int start = 0;
    for (int i = 0; i <= (int)routes.length(); i++) {
        if (i == (int)routes.length() || routes[i] == ',') {
            String pair = routes.substring(start, i);
            pair.trim();
            int colon = pair.indexOf(':');
            if (colon > 0) {
                if (result.length() > 0) result += ",";
                result += pair.substring(colon + 1);
            }
            start = i + 1;
        }
    }
    return result;
}

// --- Target Loading ---

int NVSManager::loadTargets(BusTarget* targets, int maxTargets) {
    String routes = getRoutes();
    Serial.printf("[NVSManager] loadTargets: routes key = '%s'\n", routes.c_str());

    // If no routes in new format, try legacy format
    if (routes.length() == 0) {
        String stopId = _prefs.getString("stopId", "");
        String lines  = _prefs.getString("lines", "");
        Serial.printf("[NVSManager] loadTargets: legacy stopId='%s', lines='%s'\n",
                      stopId.c_str(), lines.c_str());
        if (stopId.length() > 0 && lines.length() > 0) {
            // Rebuild as "stop:line,stop:line,..."
            routes = "";
            int start = 0;
            for (int i = 0; i <= (int)lines.length(); i++) {
                if (i == (int)lines.length() || lines[i] == ',') {
                    String token = lines.substring(start, i);
                    token.trim();
                    if (token.length() > 0) {
                        if (routes.length() > 0) routes += ",";
                        routes += stopId + ":" + token;
                    }
                    start = i + 1;
                }
            }
        }
    }

    if (routes.length() == 0) return 0;

    int count = 0;
    int start = 0;

    // Parse "stop:line" pairs (e.g. "1570:7, 3541:19, 6134:72")
    for (int i = 0; i <= (int)routes.length() && count < maxTargets; i++) {
        if (i == (int)routes.length() || routes[i] == ',') {
            String pair = routes.substring(start, i);
            pair.trim();
            int colon = pair.indexOf(':');
            if (colon > 0) {
                String stopPart = pair.substring(0, colon);
                String linePart = pair.substring(colon + 1);
                stopPart.trim();
                linePart.trim();

                if (stopPart.length() > 0 && linePart.length() > 0) {
                    strncpy(_stopIdBufs[count], stopPart.c_str(), sizeof(_stopIdBufs[count]) - 1);
                    _stopIdBufs[count][sizeof(_stopIdBufs[count]) - 1] = '\0';

                    strncpy(_lineBufs[count], linePart.c_str(), sizeof(_lineBufs[count]) - 1);
                    _lineBufs[count][sizeof(_lineBufs[count]) - 1] = '\0';

                    targets[count] = {};
                    targets[count].stationId = _stopIdBufs[count];
                    targets[count].line = _lineBufs[count];
                    strncpy(targets[count].destination, "END LINE", sizeof(targets[count].destination));
                    targets[count].is_realtime = false;
                    targets[count].last_known_ETA[0] = '\0';
                    targets[count].minutes_remaining = 0;
                    targets[count].no_data = false;
                    count++;
                }
            }
            start = i + 1;
        }
    }
    return count;
}
