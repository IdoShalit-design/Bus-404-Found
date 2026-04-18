#ifndef CONFIG_H
#define CONFIG_H
#include "Structs.h"

// --- Debug Flags ---
#define SCREEN_DEBUG 0     // Set to 1 to run display color tests (blocks forever)
#define MEMORY_DEBUG         0  // Set to 1 to log heap memory usage to LittleFS (heap_log.txt)

#define HEAP_UDP_PORT 12345   // UDP port for heap reports on PC

// --- TimeZone ---
#define TIME_ZONE "IST-2IDT,M3.4.4/26,M10.5.0"

#define FETCH_INTERVAL 30000  // 30 seconds
#define MAX_RUNTIME_TARGETS 3

// ------------- API'S address ---------------- //

// PC_IP_ADDRESS is injected at build time by get_ip.py (your machine's LAN IP)
#ifndef PC_IP_ADDRESS
#define PC_IP_ADDRESS "127.0.0.1"  // Fallback if not set by build script
#endif
#define COMPUTER_IP PC_IP_ADDRESS

#endif