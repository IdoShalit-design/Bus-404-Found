#ifndef CONFIG_H
#define CONFIG_H
#include "Credentials.h"  // WiFi credentials (not committed to git)
#include "Structs.h"

// --- TimeZone ---
#define TIME_ZONE "IST-2IDT,M3.4.4/26,M10.5.0"

// --- Data Fetcher ---
// Select which fetcher implementation to use:
#define FETCHER_CURLBUS  1
#define FETCHER_GOVIL    2  // Future
#define FETCHER_MOCK     3  // Future

#define FETCHER_TYPE FETCHER_CURLBUS

// --- Bus Targets ---
const BusTarget MY_TARGETS[] = {
    {"1570", "7", "", 0},   // Line 7 at Bezalel/Trumpeldor → Givat Ram
    {"3541", "19", "", 0},  // Line 19 → Ein Kerem
    {"6134", "72", "", 0}   // Line 72 → Romema
};

const int TARGETS_COUNT = sizeof(MY_TARGETS) / sizeof(MY_TARGETS[0]);

// ------------- API'S address ---------------- //


#endif