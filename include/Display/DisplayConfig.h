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
#define R1_PIN  18
#define G1_PIN  25
#define B1_PIN  05
#define R2_PIN  17
#define G2_PIN  33
#define B2_PIN  16

#define A_PIN   04
#define B_PIN   16
#define C_PIN   00
#define D_PIN   20
#define E_PIN   32   // For 64x64 panels only, -1 if not used

// Control Pins:
#define LAT_PIN 19
#define OE_PIN  15
#define CLK_PIN 02

// --- Display Settings ---
#define DISPLAY_BRIGHTNESS 128  // 0-255

// --- Color Definitions (RGB565) ---
#define COLOR_REALTIME  0x07E0  // Green - live GPS data
#define COLOR_SCHEDULED 0xFFE0  // Yellow - scheduled time
#define COLOR_NO_DATA   0xF800  // Red - no data available
#define COLOR_LINE_NUM  0xFFFF  // White - bus line number
#define COLOR_TIME      0x07FF  // Cyan - time display

#endif
