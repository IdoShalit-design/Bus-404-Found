#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

// ============================================
// HUB75 Display Configuration
// ============================================

// --- Panel dimensions (per single panel) ---
#define PANEL_WIDTH   64
#define PANEL_HEIGHT  32

// --- Number of panels chained ---
#define PANEL_CHAIN 1      // Two matrices side-by-side = 128x32 total

// --- Total display resolution ---
#define DISPLAY_WIDTH  (PANEL_WIDTH * PANEL_CHAIN)  // 128
#define DISPLAY_HEIGHT PANEL_HEIGHT                  // 32

// --- HUB75 Pinout Definitions ---
// Must match physical wiring from ESP32-S3 to HUB75 connector

#define R1_PIN 4
#define G1_PIN 5
#define B1_PIN 6

#define R2_PIN 7
#define G2_PIN 15
#define B2_PIN 16


#define A_PIN 18
#define B_PIN 8
#define C_PIN 3
#define D_PIN 42
#define E_PIN -1  //--> required for 1/32 scan panels, safe to leave defined
#define LAT_PIN 40
#define OE_PIN 2
#define CLK_PIN 41

#define DISPLAY_BRIGHTNESS 128  // 0-255

// --- Color Definitions (RGB565) ---
#define COLOR_REALTIME  0x07E0  // Green - live GPS data
#define COLOR_SCHEDULED 0xFFE0  // Yellow - scheduled time
#define COLOR_NO_DATA   0xF800  // Red - no data available
#define COLOR_LINE_NUM  0xFFFF  // White - bus line number
#define COLOR_TIME      0x07FF  // Cyan - time display

#endif
