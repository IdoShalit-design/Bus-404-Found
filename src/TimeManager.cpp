#include <Arduino.h>
#include "TimeManager.h"

static const char* ntp_server = "pool.ntp.org";

void time_init_and_sync(const char* tz) {
    configTzTime(tz, ntp_server);

    Serial.print("[Time] Waiting for NTP sync");
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

bool time_is_set() {
    struct tm timeinfo;
    return getLocalTime(&timeinfo);
}

void time_get_formatted(char* buf, size_t len) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        strncpy(buf, "00:00", len);
        return;
    }
    snprintf(buf, len, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
}

int time_get_minutes_until(const char* eta) {
    struct tm now_tm;
    if (!getLocalTime(&now_tm)) return -1;

    int eta_hour = (eta[0] - '0') * 10 + (eta[1] - '0');
    int eta_min  = (eta[3] - '0') * 10 + (eta[4] - '0');

    struct tm eta_tm = now_tm;
    eta_tm.tm_hour = eta_hour;
    eta_tm.tm_min = eta_min;
    eta_tm.tm_sec = 0;

    time_t now_seconds = mktime(&now_tm);
    time_t eta_seconds = mktime(&eta_tm);

    double diff_seconds = difftime(eta_seconds, now_seconds);
    return (int)(diff_seconds / 60);
}