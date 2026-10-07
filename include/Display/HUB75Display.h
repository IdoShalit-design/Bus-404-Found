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

//#define DISPLAY_BRIGHTNESS 128  // 0-255
#define DISPLAY_BRIGHTNESS 32  // 0-255


// --- Layout: X cursor positions per row (depends on number of panels) ---
// 2-panel layout (128px wide): generous spacing
// 1-panel layout (64px wide):  compact spacing
// A row shows two arrivals, so the minutes zone is two columns wide, separated
// by a comma. The imminent arrival sits on the right, nearest the bus icon, and
// the one after it on the left, so the pair reads outward as time runs out.
// Each column is 12px (two digits) and the comma between them is one 6px glyph.
#if PANEL_CHAIN == 2
    #define XCOL_LINE_NUM      0    // Bus line number (far left)
    #define XCOL_DEST          14   // Destination text
    #define XCOL_MINUTES_LATER 88   // Minutes to the arrival after the next one
    #define XCOL_MINUTES_COMMA 100  // Comma separating the two arrivals
    #define XCOL_MINUTES       108  // Minutes to the next arrival
    #define XCOL_IMAGE_START   122  // Reserved for image (last 6px)
#else
    #define XCOL_LINE_NUM      0    // Bus line number (far left)
    #define XCOL_DEST          14   // Destination text
    #define XCOL_MINUTES_LATER 28   // Minutes to the arrival after the next one
    #define XCOL_MINUTES_COMMA 40   // Comma separating the two arrivals
    #define XCOL_MINUTES       46   // Minutes to the next arrival
    #define XCOL_IMAGE_START   58   // Reserved for image (last 6px)
#endif

// GFX default font at textSize(1): 6px per character (5px glyph + 1px spacing)
#define FONT_CHAR_W      6
// ...and 8px per line, which is also its baseline-to-baseline advance.
#define FONT_LINE_H      8
// What actually fits on one line of showMessage(). A status string longer than
// this is clipped on the panel, so keep messages within it or split them on '\n'.
#define MAX_MESSAGE_CHARS (DISPLAY_WIDTH / FONT_CHAR_W)
#define MAX_DEST_CHARS   ((XCOL_MINUTES_LATER - XCOL_DEST) / FONT_CHAR_W)

// --- Icon dimensions (fits inside the reserved image zone at row end) ---
#define ICON_W 8   // pixels wide  (replace with your bitmap width)
#define ICON_H 8   // pixels tall  (replace with your bitmap height)

// Monochrome bus icon bitmap, ICON_W x ICON_H pixels.
// Each byte = one row; bit 7 is the leftmost pixel.


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

static const uint8_t ARRIVING_ICON[ICON_H] = {
    0b00011000,  // ...##...
    0b00011000,  // ...##...
    0b00011000,  // ...##...
    0b11111111,  // ########
    0b11111111,  // ########
    0b01111110,  // .######.
    0b00111100,  // ..####..
    0b00011000   // ...##...
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
     * @brief Draws one arrival's minutes (or the arriving-now icon at zero).
     * @param x       Left X pixel of this arrival's column.
     * @param y       Top Y pixel of the row.
     * @param minutes Minutes remaining; <= 0 renders the arriving-now icon.
     * @param isRealtime True for live GPS data, false for scheduled.
     * @return The colour the arrival was drawn in, so the caller can match it.
     */
    uint16_t drawArrivalMinutes(int x, int y, int minutes, bool isRealtime);

    /**
     * @brief Draws a monochrome bitmap icon at (x, y) in the given color.
     * @param x      Top-left X pixel.
     * @param y      Top-left Y pixel.
     * @param color  RGB565 color for lit pixels.
     * @param bitmap Pointer to icon bitmap (defaults to BUS_ICON).
     */
    void drawIcon(int x, int y, uint16_t color, const uint8_t* bitmap);

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
     * @brief Displays a centered status message on the screen.
     * @param msg The message string to display.
     */
    void showMessage(const char* msg) override;

    /**
     * @brief Runs a series of display tests for debugging.
     * Tests: fill colors, pixel walk, text rendering, pin report.
     * Blocks indefinitely (loops tests forever).
     */
    void screen_tests();
};


#endif
