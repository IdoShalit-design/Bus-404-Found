#ifndef IRENDERER_H
#define IRENDERER_H

#include <stdint.h>
#include "Structs.h"

/**
 * @brief Abstract interface for rendering bus arrival data to a display.
 * 
 * Provides a common interface for different display implementations
 * (HUB75 LED matrix, OLED, LCD, etc.).
 */
class IRenderer {
public:
    /**
     * @brief Virtual destructor for proper cleanup of derived classes.
     */
    virtual ~IRenderer() = default;

    /**
     * @brief Initializes the display hardware.
     * @return true if initialization succeeded, false otherwise.
     */
    virtual bool init() = 0;

    /**
     * @brief Renders all bus targets to the display.
     * @param targets Array of BusTarget structs with arrival data.
     * @param count Number of targets in the array.
     */
    virtual void render(const BusTarget* targets, int count) = 0;

    /**
     * @brief Clears the display (all pixels off).
     */
    virtual void clear() = 0;

    /**
     * @brief Sets display brightness.
     * @param brightness Value from 0 (off) to 255 (max).
     */
    virtual void setBrightness(uint8_t brightness) = 0;

    /**
     * @brief Displays a centered status message on the display.
     * Used during boot sequence and configuration flow.
     * @param text The status text to display.
     */
    virtual void showStatus(const char* text) = 0;
};

#endif // IRENDERER_H
