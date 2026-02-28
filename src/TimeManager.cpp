#include "TimeManager.h"

TimeManager::TimeManager(const char* tz) : _timezone(tz) {
}

void TimeManager::init_and_sync() {
    // Standard ESP32 NTP sync
    configTzTime(_timezone, _ntpServer);

    // Wait for NTP to actually sync (needed for HTTPS/TLS)
    Serial.print("[TimeManager] Waiting for NTP sync");
    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 20) {
        Serial.print(".");
        delay(500);
        retry++;
    }
    if (retry >= 20) {
        Serial.println(" FAILED!");
    } else {
        Serial.println(" OK");
    }
}

bool TimeManager::is_time_set() {
    struct tm timeinfo;
    return getLocalTime(&timeinfo); // Returns true if sync happened
}

void TimeManager::get_formatted_time(char* buf, size_t len) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        strncpy(buf, "00:00", len);
        buf[len - 1] = '\0';
        return;
    }
    snprintf(buf, len, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
}

    int TimeManager::get_minutes_until(const char* eta) {
        struct tm now_tm;
        if (!getLocalTime(&now_tm)) return -1; // Error if time not synced

        // 1. Parse the ETA string (HH:MM)
        int eta_hour = (eta[0] - '0') * 10 + (eta[1] - '0');
        int eta_min  = (eta[3] - '0') * 10 + (eta[4] - '0');

        // 2. Create a tm struct for the ETA (based on today's date)
        struct tm eta_tm = now_tm; 
        eta_tm.tm_hour = eta_hour;
        eta_tm.tm_min = eta_min;
        eta_tm.tm_sec = 0;

        // 3. Convert both to time_t (seconds since 1970)
        time_t now_seconds = mktime(&now_tm);
        time_t eta_seconds = mktime(&eta_tm);

        // 4. Calculate difference
        double diff_seconds = difftime(eta_seconds, now_seconds);
        
        // Return difference in minutes
        return (int)(diff_seconds / 60);
    }