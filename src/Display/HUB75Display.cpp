/**
 * @file HUB75Display.cpp
 * @brief Implementation of HUB75 LED Matrix display driver.
 * 
 * Renders bus arrival information on chained HUB75 LED matrices
 * using the ESP32-HUB75-MatrixPanel-DMA library.
 * 
 * Layout (128x32, two 64x32 panels):
 *   Left panel:  Line number + 1st arrival ETA
 *   Right panel: 2nd arrival ETA
 */

#include "Display/HUB75Display.h"

// Row height for each bus entry (pixels)
static const int ROW_HEIGHT = DISPLAY_HEIGHT / 3;  // 10px per row for 3 targets

// Panel boundary (each panel is 64px wide)
static const int PANEL_2_X = PANEL_WIDTH;  // 64

// Layout offsets within each panel
static const int LINE_NUM_X = 2;           // Bus line number left margin
static const int ETA_X = 20;               // ETA text offset from panel start
static const int RT_INDICATOR_OFFSET = 36; // RT/SC indicator offset from ETA_X
static const int PANEL_LEFT_MARGIN = 4;    // Left margin within the second panel

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

/**
 * @brief Draws a single arrival's ETA info at the given position.
 * @param arr    The arrival data to render.
 * @param x      Horizontal pixel offset for the ETA text.
 * @param y      Vertical pixel offset.
 */
void HUB75Display::drawArrival(const ArrivalInfo& arr, int x, int y) {
    uint16_t minutesColor;
    if (arr.minutes_remaining < 0) {
        minutesColor = COLOR_NO_DATA;
    } else if (arr.is_realtime) {
        minutesColor = COLOR_REALTIME;
    } else {
        minutesColor = COLOR_SCHEDULED;
    }

    // --- Draw minutes remaining ---
    _matrix->setTextColor(minutesColor);
    _matrix->setCursor(x, y);

    if (arr.minutes_remaining < 0) {
        _matrix->print("---");
    } else if (arr.minutes_remaining == 0) {
        _matrix->print("NOW");
    } else {
        _matrix->printf("%d min", arr.minutes_remaining);
    }

    // --- Draw real-time indicator ---
    if (arr.minutes_remaining >= 0) {
        _matrix->setCursor(x + RT_INDICATOR_OFFSET, y);
        _matrix->setTextColor(minutesColor);
        _matrix->print(arr.is_realtime ? "RT" : "SC");
    }
}

void HUB75Display::drawBusRow(const BusTarget& target, int row) {
    int y = row * ROW_HEIGHT;

    // --- Left panel: Line number + 1st arrival ---
    _matrix->setTextSize(1);
    _matrix->setTextColor(COLOR_LINE_NUM);
    _matrix->setCursor(LINE_NUM_X, y + 1);
    _matrix->print(target.line);

    if (target.arrival_count > 0) {
        drawArrival(target.arrivals[0], ETA_X, y + 1);
    } else {
        _matrix->setTextColor(COLOR_NO_DATA);
        _matrix->setCursor(ETA_X, y + 1);
        _matrix->print("---");
    }

    // --- Right panel: 2nd arrival ---
    if (target.arrival_count > 1) {
        drawArrival(target.arrivals[1], PANEL_2_X + PANEL_LEFT_MARGIN, y + 1);
    } else {
        _matrix->setTextColor(COLOR_NO_DATA);
        _matrix->setCursor(PANEL_2_X + PANEL_LEFT_MARGIN, y + 1);
        _matrix->print("---");
    }
}
