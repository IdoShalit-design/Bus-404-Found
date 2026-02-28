#ifndef CONFIG_H
#define CONFIG_H
#include "Structs.h"
#include "Secrets.h"  // WiFi credentials (not committed to git)

// --- Debug Flags ---
#define SCREEN_DEBUG       0  // Set to 1 to run display color tests (blocks forever)
#define DUMMY_BUSES_DEBUG  1  // Set to 1 to skip fetching and render dummy bus data

// --- Dummy bus data (used when DUMMY_BUSES_DEBUG == 1) ---
#if DUMMY_BUSES_DEBUG
const BusTarget DUMMY_TARGETS[] = {
    // Real-time (live GPS), 5 minutes
    {"0000", "7",  "Givat Ram",  true,  "12:05", 5,  false},
    // Scheduled (no GPS), 7 minutes
    {"0000", "19", "Ein Kerem",  false, "12:07", 7,  false},
    // No data returned from API
    {"0000", "72", "Romema",     false, "",       0,  true},
};
const int DUMMY_TARGETS_COUNT = sizeof(DUMMY_TARGETS) / sizeof(DUMMY_TARGETS[0]);
#endif

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
    {"1570", "7",  "END LINE", false, "", 0},   // Line 7 at Bezalel/Trumpeldor → Givat Ram
    {"3541", "19", "END LINE", false, "", 0},  // Line 19 → Ein Kerem
    {"6134", "72", "END LINE", false, "", 0}   // Line 72 → Romema
};

const int TARGETS_COUNT = sizeof(MY_TARGETS) / sizeof(MY_TARGETS[0]);

// ------------- API'S address ---------------- //


#endif