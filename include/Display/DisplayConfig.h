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
// Using ESP32-HUB75-MatrixPanel-DMA library default pins
// These are the RECOMMENDED pins from the library documentation
// If using a different adapter board, update these to match your wiring!

#define R1_PIN  25
#define G1_PIN  26
#define B1_PIN  27
#define R2_PIN  14
#define G2_PIN  12
#define B2_PIN  13

#define A_PIN   23
#define B_PIN   19
#define C_PIN   5
#define D_PIN   17
#define E_PIN   -1   // -1 for 64x32 panels (1/16 scan). Use valid GPIO for 64x64 (1/32 scan)

// Control Pins:
#define LAT_PIN 4
#define OE_PIN  15
#define CLK_PIN 16

// --- Display Settings ---
#define DISPLAY_BRIGHTNESS 128  // 0-255

// --- Color Definitions (RGB565) ---
#define COLOR_REALTIME  0x07E0  // Green - live GPS data
#define COLOR_SCHEDULED 0xFFE0  // Yellow - scheduled time
#define COLOR_NO_DATA   0xF800  // Red - no data available
#define COLOR_LINE_NUM  0xFFFF  // White - bus line number
#define COLOR_TIME      0x07FF  // Cyan - time display

#endif
