#ifndef CONFIG_H
#define CONFIG_H
#include "Structs.h"
#include "Secrets.h"  // WiFi credentials (not committed to git)

// --- Debug Flags ---
#define SCREEN_DEBUG 0     // Set to 1 to run display color tests (blocks forever)
#define DUMMY_BUSES_DEBUG 0    // Set to 1 to skip fetching and render dummy bus data
#define MEMORY_DEBUG         1  // Set to 1 to log heap memory usage to LittleFS (heap_log.txt)

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

#define FETCH_INTERVAL 30000  // 30 seconds


// --- Bus Targets ---
const BusTarget MY_TARGETS[] = {
    {"1570", "7",  "END LINE", false, "", 0},   // Line 7 at Bezalel/Trumpeldor → Givat Ram
    {"3541", "19", "END LINE", false, "", 0},  // Line 19 → Ein Kerem
    {"6134", "72", "END LINE", false, "", 0}   // Line 72 → Romema
};

const int TARGETS_COUNT = sizeof(MY_TARGETS) / sizeof(MY_TARGETS[0]);

// ------------- API'S address ---------------- //

// PC_IP_ADDRESS is injected at build time by get_ip.py (your machine's LAN IP)
#ifndef PC_IP_ADDRESS
#define PC_IP_ADDRESS "127.0.0.1"  // Fallback if not set by build script
#endif
#define COMPUTER_IP PC_IP_ADDRESS

#endif