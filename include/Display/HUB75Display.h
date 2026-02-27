#ifndef HUB75_DISPLAY_H
#define HUB75_DISPLAY_H

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "Display/IRenderer.h"

// ============================================
// HUB75 Display Configuration
// ============================================

// --- Panel dimensions (per single panel) ---
#define PANEL_WIDTH   64
#define PANEL_HEIGHT  32

// --- Number of panels chained ---
#define PANEL_CHAIN 1 

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

// --- Layout: X cursor positions per row (depends on number of panels) ---
// 2-panel layout (128px wide): generous spacing
// 1-panel layout (64px wide):  compact spacing
#if PANEL_CHAIN == 2
    #define XCOL_LINE_NUM  2    // Bus line number (left)
    #define XCOL_MINUTES   40   // Minutes remaining (center)
    #define XCOL_STATUS    100  // RT/SC indicator (right)
#elif PANEL_CHAIN == 1
    #define XCOL_LINE_NUM  2    // Bus line number (left)
    #define XCOL_MINUTES   22   // Minutes remaining (center)
    #define XCOL_STATUS    48   // RT/SC indicator (right)
#endif

// --- Color Definitions (RGB565) ---
#define COLOR_REALTIME  0x07E0  // Green - live GPS data
#define COLOR_SCHEDULED 0xFFE0  // Yellow - scheduled time
#define COLOR_NO_DATA   0xF800  // Red - no data available
#define COLOR_LINE_NUM  0xFFFF  // White - bus line number
#define COLOR_TIME      0x07FF  // Cyan - time display

/**
 * @brief HUB75 LED Matrix display driver for rendering bus arrival data.
 * 
 * Uses ESP32-HUB75-MatrixPanel-DMA library to drive chained LED matrices.
 * Displays bus line numbers, ETAs, and real-time status with color coding.
 */
class HUB75Display : public IRenderer {
private:
    MatrixPanel_I2S_DMA* _matrix;
    
    /**
     * @brief Draws a single bus target row on the display.
     * @param target The bus target data to render.
     * @param row The row index (0-based) for vertical positioning.
     */
    void drawBusRow(const BusTarget& target, int row);

public:
    /**
     * @brief Constructor - does not initialize hardware.
     */
    HUB75Display();

    /**
     * @brief Destructor - frees matrix resources.
     */
    ~HUB75Display();

    /**
     * @brief Initializes the HUB75 matrix with configured pins and settings.
     * @return true if initialization succeeded, false otherwise.
     */
    bool init() override;

    /**
     * @brief Renders all bus targets to the display.
     * @param targets Array of BusTarget structs with arrival data.
     * @param count Number of targets in the array.
     */
    void render(const BusTarget* targets, int count) override;

    /**
     * @brief Clears the display (all pixels off).
     */
    void clear() override;

    /**
     * @brief Sets display brightness.
     * @param brightness Value from 0 (off) to 255 (max).
     */
    void setBrightness(uint8_t brightness) override;

    /**
     * @brief Runs a series of display tests for debugging.
     * Tests: fill colors, pixel walk, text rendering, pin report.
     * Blocks indefinitely (loops tests forever).
     */
    void screen_tests();
};

#endif
