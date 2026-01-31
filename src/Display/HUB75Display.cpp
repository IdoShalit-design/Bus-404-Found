/**
 * @file HUB75Display.cpp
 * @brief Implementation of HUB75 LED Matrix display driver.
 * 
 * Renders bus arrival information on chained HUB75 LED matrices
 * using the ESP32-HUB75-MatrixPanel-DMA library.
 */

#include "Display/HUB75Display.h"

// Row height for each bus entry (pixels)
static const int ROW_HEIGHT = DISPLAY_HEIGHT / 3;  // 10px per row for 3 targets

// =========================================
// Constructor & Destructor
// =========================================

HUB75Display::HUB75Display() : _matrix(nullptr) {
}

HUB75Display::~HUB75Display() {
    if (_matrix) {
        delete _matrix;
        _matrix = nullptr;
    }
}

// =========================================
// Public Methods
// =========================================

bool HUB75Display::init() {
    // Configure HUB75 panel settings
    HUB75_I2S_CFG::i2s_pins pins = {
        R1_PIN, G1_PIN, B1_PIN,
        R2_PIN, G2_PIN, B2_PIN,
        A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
        LAT_PIN, OE_PIN, CLK_PIN
    };

    HUB75_I2S_CFG config(
        PANEL_WIDTH,    // Panel width
        PANEL_HEIGHT,   // Panel height
        PANEL_CHAIN,    // Chain length
        pins            // Pin mapping
    );

    // Optional: Adjust for specific panel types
    // config.driver = HUB75_I2S_CFG::FM6126A;  // Uncomment if using FM6126A driver chip
    // config.clkphase = false;                  // Adjust clock phase if colors are wrong

    _matrix = new MatrixPanel_I2S_DMA(config);

    if (!_matrix->begin()) {
        #ifdef DEBUG
        Serial.println("[HUB75Display] Matrix initialization failed!");
        #endif
        return false;
    }

    _matrix->setBrightness8(DISPLAY_BRIGHTNESS);
    _matrix->fillScreen(0);  // Clear display

    #ifdef DEBUG
    Serial.printf("[HUB75Display] Initialized %dx%d display (%d panels)\n",
                  DISPLAY_WIDTH, DISPLAY_HEIGHT, PANEL_CHAIN);
    #endif

    return true;
}

void HUB75Display::render(const BusTarget* targets, int count) {
    if (!_matrix) return;

    _matrix->fillScreen(0);  // Clear before redraw

    // Render each bus target as a row
    int maxRows = (count < 3) ? count : 3;  // Max 3 rows on 32px height
    for (int i = 0; i < maxRows; i++) {
        drawBusRow(targets[i], i);
    }
}

void HUB75Display::clear() {
    if (_matrix) {
        _matrix->fillScreen(0);
    }
}

void HUB75Display::setBrightness(uint8_t brightness) {
    if (_matrix) {
        _matrix->setBrightness8(brightness);
    }
}

// =========================================
// Private Methods
// =========================================

void HUB75Display::drawBusRow(const BusTarget& target, int row) {
    int y = row * ROW_HEIGHT;

    // Determine color based on data availability and real-time status
    uint16_t minutesColor;
    if (target.minutes_remaining < 0) {
        minutesColor = COLOR_NO_DATA;
    } else if (target.is_realtime) {
        minutesColor = COLOR_REALTIME;
    } else {
        minutesColor = COLOR_SCHEDULED;
    }

    // --- Draw line number (left side) ---
    _matrix->setTextSize(1);
    _matrix->setTextColor(COLOR_LINE_NUM);
    _matrix->setCursor(2, y + 1);
    _matrix->print(target.line);

    // --- Draw minutes remaining (center/right) ---
    _matrix->setTextColor(minutesColor);
    _matrix->setCursor(40, y + 1);
    
    if (target.minutes_remaining < 0) {
        _matrix->print("---");
    } else if (target.minutes_remaining == 0) {
        _matrix->print("NOW");
    } else {
        _matrix->printf("%d min", target.minutes_remaining);
    }

    // --- Draw real-time indicator (far right) ---
    if (target.minutes_remaining >= 0) {
        _matrix->setCursor(100, y + 1);
        _matrix->setTextColor(minutesColor);
        _matrix->print(target.is_realtime ? "RT" : "SC");
    }
}
