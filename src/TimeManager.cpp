#include "TimeManager.h"

TimeManager::TimeManager(const char* tz) : _timezone(tz) {
}

void TimeManager::init_and_sync() {
    // Standard ESP32 NTP sync
    configTime(0, 0, _ntpServer);
    setenv("TZ", _timezone, 1);
    tzset();
}

bool TimeManager::is_time_set() {
    struct tm timeinfo;
    return getLocalTime(&timeinfo); // Returns true if sync happened
}

String TimeManager::get_formatted_time() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "00:00";
    
    char buf[6];
    sprintf(buf, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    return String(buf);

}

    int TimeManager::get_minutes_until(String eta) {
        struct tm now_tm;
        if (!getLocalTime(&now_tm)) return -1; // Error if time not synced

        // 1. Parse the ETA string (HH:MM)
        int eta_hour = eta.substring(0, 2).toInt();
        int eta_min = eta.substring(3, 5).toInt();

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