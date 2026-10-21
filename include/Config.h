#ifndef CONFIG_H
#define CONFIG_H
#include "BusTarget.h"

#include <stdint.h>

// --- Debug Flags (kept as macros: used in #if conditionals) ---
#define SCREEN_DEBUG 0     // Set to 1 to run display color tests (blocks forever)
#define MEMORY_DEBUG         0  // Set to 1 to log heap memory usage to LittleFS (heap_log.txt)

// Defaulted with #ifndef rather than #define, so an environment in
// platformio.ini can set them with -D without a redefinition warning.
//
// USE_SERIAL_RENDERER: write the panel rows to the serial log instead of the
// LED matrix. Useful on hardware for telling a missing fetch apart from a
// missing render, and required under a simulator, which cannot drive HUB75.
#ifndef USE_SERIAL_RENDERER
#define USE_SERIAL_RENDERER 0
#endif

// SKIP_PORTAL: go straight to the saved config instead of opening the
// provisioning portal. On by default: the portal blocks boot for its full join
// window on every start, which is only worth paying when the config needs
// changing. Set it to 0 to provision the device. A simulator needs it on
// regardless -- the portal waits there on a client that cannot exist.
#ifndef SKIP_PORTAL
#define SKIP_PORTAL 1
#endif

// SIM_WIFI: join the simulator's virtual access point instead of the network
// named in the saved config. The saved SSID is a real home network, which does
// not exist inside a simulator, so without this every run spends its retries on
// a network that cannot answer and never reaches the code under test.
#ifndef SIM_WIFI
#define SIM_WIFI 0
#endif

// Wokwi's open access point, and the channel it runs on. Passing the channel to
// WiFi.begin() skips the scan phase, which Wokwi documents as saving about four
// seconds per connect -- paid on every run of the simulator.
#define SIM_WIFI_SSID "Wokwi-GUEST"
#define SIM_WIFI_PASSWORD ""
#define SIM_WIFI_CHANNEL 6

constexpr uint16_t HEAP_UDP_PORT = 12345;  // UDP port for heap reports on PC

// --- TimeZone ---
#define TIME_ZONE "IST-2IDT,M3.4.4/26,M10.5.0"

constexpr unsigned long FETCH_INTERVAL_MS = 30000UL;
constexpr size_t MAX_RUNTIME_TARGETS = 3;

// ------------- API'S address ---------------- //

// PC_IP_ADDRESS is injected at build time by get_ip.py (your machine's LAN IP)
#ifndef PC_IP_ADDRESS
#define PC_IP_ADDRESS "127.0.0.1"  // Fallback if not set by build script
#endif
#define COMPUTER_IP PC_IP_ADDRESS

#endif