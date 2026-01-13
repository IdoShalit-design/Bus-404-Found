#ifndef CONFIG_H
#define CONFIG_H
#include "BusType.h"
// --- WiFi Settings ---
#define WIFI_SSID "Littlebluedoor"
#define WIFI_PASS "Mennashe"

// --- TimeZone ---
#define TIME_ZONE "IST-2IDT,M3.4.4/26,M10.5.0"


const BusTarget MY_TARGETS[] = {
    {"1570", "7", "", 0}, // 7 to giv'at ram
    {"3541", "19", "", 0},  // 19 to ein carem
    {"6134", "72", "", 0}  // 72 to romema(I think)
};

const int TARGETS_COUNT = sizeof(MY_TARGETS) / sizeof(MY_TARGETS[0]);

// ------------- API'S address ---------------- //


#endif