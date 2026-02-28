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
#define PANEL_CHAIN 2

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
    #define XCOL_LINE_NUM    0    // Bus line number (far left)
    #define XCOL_DEST        14   // Destination text
    #define XCOL_MINUTES     110   // Minutes remaining
    #define XCOL_IMAGE_START 122  // Reserved for image (last 12px)
#else
    #define XCOL_LINE_NUM    0    // Bus line number (far left)
    #define XCOL_DEST        14   // Destination text
    #define XCOL_MINUTES     48   // Minutes remaining
    #define XCOL_IMAGE_START 58   // Reserved for image (last 6px)
#endif

// GFX default font at textSize(1): 6px per character (5px glyph + 1px spacing)
#define FONT_CHAR_W      6
#define MAX_DEST_CHARS   ((XCOL_MINUTES - XCOL_DEST) / FONT_CHAR_W)

// --- Icon dimensions (fits inside the reserved image zone at row end) ---
#define ICON_W 8   // pixels wide  (replace with your bitmap width)
#define ICON_H 8   // pixels tall  (replace with your bitmap height)

// Monochrome bus icon bitmap, ICON_W x ICON_H pixels.
// Each byte = one row; bit 7 is the leftmost pixel.

// static const uint8_t BUS_ICON[ICON_H] = {
// 0b00000000,  // ........
//     0b11110000,  // ####....
//     0b11111000,  // #####...
//     0b00011100,  // ...###..
//     0b11001110,  // ##..###.
//     0b00100110,  // ..#..##.
//     0b10010110,  // #..#..##.
//     0b11010110   // ##.#..##.
//     };

static const uint8_t BUS_ICON[ICON_H] = {
    0b00000000,  // ........
    0b00000000,  // ........
    0b11100000,  // ###.....
    0b00010000,  // ...#....
    0b11001000,  // ##..#...
    0b00100100,  // ..#..#..
    0b10010100,  // #..#.#..
    0b11010100   // ##.#.#..
    };



// --- Color Definitions (RGB565) ---
#define COLOR_REALTIME  0x07E0  // Green - live GPS data
#define COLOR_BUS_NUM   0x1A8E3D  // Dark green - bus line number (slightly darker than realtime)
#define COLOR_SCHEDULED 0xFFE0  // Yellow - scheduled time
#define COLOR_NO_DATA   0xF800  // Red - no data / arriving now
#define COLOR_DEST      0xFFFF  // White - destination text
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

    /**
     * @brief Draws a monochrome bitmap icon at (x, y) in the given color.
     * @param x      Top-left X pixel.
     * @param y      Top-left Y pixel.
     * @param color  RGB565 color for lit pixels.
     */
    void drawIcon(int x, int y, uint16_t color);

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
