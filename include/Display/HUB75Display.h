#ifndef HUB75_DISPLAY_H
#define HUB75_DISPLAY_H

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "DisplayConfig.h"
#include "Display/IRenderer.h"

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
