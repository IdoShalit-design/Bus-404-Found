#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

// ============================================
// HUB75 Display Configuration
// ============================================

// --- Panel dimensions (per single panel) ---
#define PANEL_WIDTH   64
#define PANEL_HEIGHT  32

// --- Number of panels chained ---
#define PANEL_CHAIN   2    // Two matrices side-by-side = 128x32 total

// --- Total display resolution ---
#define DISPLAY_WIDTH  (PANEL_WIDTH * PANEL_CHAIN)  // 128
#define DISPLAY_HEIGHT PANEL_HEIGHT                  // 32

// --- HUB75 Pinout Definitions ---
// ESP32-S3 DevKitC-1 defaults from the library's esp32s3-default-pins.hpp.
// If your wiring differs, update these to match your panel adapter.

#define R1_PIN  37
#define G1_PIN  06
#define B1_PIN  36
#define R2_PIN  35
#define G2_PIN  05
#define B2_PIN  0

#define A_PIN   45
#define B_PIN   01
#define C_PIN   48
#define D_PIN   02
#define E_PIN   04   // -1 for 64x32 panels (1/16 scan). Use valid GPIO for 64x64 (1/32 scan)

// Control Pins:
#define LAT_PIN 38
#define OE_PIN  47
#define CLK_PIN 21

// --- Display Settings ---
#define DISPLAY_BRIGHTNESS 128  // 0-255

// --- Color Definitions (RGB565) ---
#define COLOR_REALTIME  0x07E0  // Green - live GPS data
#define COLOR_SCHEDULED 0xFFE0  // Yellow - scheduled time
#define COLOR_NO_DATA   0xF800  // Red - no data available
#define COLOR_LINE_NUM  0xFFFF  // White - bus line number
#define COLOR_TIME      0x07FF  // Cyan - time display

#endif
