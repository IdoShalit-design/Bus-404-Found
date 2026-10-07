#ifndef SERIAL_RENDERER_H
#define SERIAL_RENDERER_H

#include "Display/IRenderer.h"

/**
 * @brief Writes what the panel would show to the serial port instead of to LEDs.
 *
 * Two uses, and the second is why this exists at all:
 *
 * 1. On hardware, it puts the rendered rows in the same log as the fetch and
 *    parse messages. A line that is missing from the panel can be missing
 *    because the fetcher never filled it or because the renderer dropped it, and
 *    from the panel alone those look identical.
 *
 * 2. In a simulator. Wokwi does not implement I2S, which is the peripheral the
 *    HUB75 driver uses, so HUB75Display cannot run there at all. This class has
 *    no hardware dependency beyond Serial.
 *
 * Deliberately does not include HUB75Display.h: that header pulls in the DMA
 * library, which would defeat both uses above.
 */
class SerialRenderer : public IRenderer {
public:
    bool init() override;
    void render(const BusTarget* targets, int count) override;
    void clear() override;
    void setBrightness(uint8_t brightness) override;
    void showMessage(const char* msg) override;
};

#endif  // SERIAL_RENDERER_H
