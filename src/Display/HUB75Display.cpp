/**
 * @file HUB75Display.cpp
 * @brief Implementation of HUB75 LED Matrix display driver.
 * 
 * Renders bus arrival information on chained HUB75 LED matrices
 * using the ESP32-HUB75-MatrixPanel-DMA library.
 */

#include "Display/HUB75Display.h"
#include "Structs.h"

// Row height for each bus entry (pixels)
static const int ROW_HEIGHT = DISPLAY_HEIGHT / 3 + 1;  // 10px per row for 3 targets

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
    Serial.println("[HUB75Display] Starting initialization...");

    // Pin config matching ESP32-S3 defaults
    HUB75_I2S_CFG::i2s_pins _pins = {
        R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, 
        A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, 
        LAT_PIN, OE_PIN, CLK_PIN
    };

    HUB75_I2S_CFG mxconfig(
        PANEL_WIDTH,
        PANEL_HEIGHT,
        PANEL_CHAIN,
        _pins
    );

    // Use defaults: SHIFTREG driver, clkphase=true
    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;

    Serial.printf("[HUB75Display] Pins: R1=%d G1=%d B1=%d R2=%d G2=%d B2=%d\n",
                  R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN);
    Serial.printf("[HUB75Display] Addr: A=%d B=%d C=%d D=%d E=%d\n",
                  A_PIN, B_PIN, C_PIN, D_PIN, E_PIN);
    Serial.printf("[HUB75Display] Ctrl: LAT=%d OE=%d CLK=%d\n",
                  LAT_PIN, OE_PIN, CLK_PIN);

    _matrix = new MatrixPanel_I2S_DMA(mxconfig);
    if (!_matrix) {
        Serial.println("[HUB75Display] ERROR: Failed to allocate matrix");
        return false;
    }

    if (!_matrix->begin()) {
        Serial.println("[HUB75Display] ERROR: begin() failed");
        delete _matrix;
        _matrix = nullptr;
        return false;
    }

    _matrix->setBrightness8(DISPLAY_BRIGHTNESS);
    _matrix->clearScreen();
    _matrix->setTextWrap(false);
    Serial.println("[HUB75Display] Init complete!");
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
// Screen Debug Tests
// =========================================

void HUB75Display::screen_tests() {
    if (!_matrix) {
        Serial.println("[ScreenTest] ERROR: Matrix not initialized!");
        return;
    }

    Serial.println("[ScreenTest] Starting display tests (FM6126A driver)...");
    Serial.printf("[ScreenTest] Display: %dx%d (%d panels)\n", DISPLAY_WIDTH, DISPLAY_HEIGHT, PANEL_CHAIN);

    while (true) {
        Serial.println("[ScreenTest] Test 1: Fill RED");
        _matrix->fillScreenRGB888(255, 0, 0);
        delay(3000);

        Serial.println("[ScreenTest] Test 2: Fill GREEN");
        _matrix->fillScreenRGB888(0, 255, 0);
        delay(3000);

        Serial.println("[ScreenTest] Test 3: Fill BLUE");
        _matrix->fillScreenRGB888(0, 0, 255);
        delay(3000);

        Serial.println("[ScreenTest] Test 4: Fill WHITE");
        _matrix->fillScreenRGB888(255, 255, 255);
        delay(3000);

        Serial.println("[ScreenTest] Test 5: Clear (BLACK)");
        _matrix->clearScreen();
        delay(2000);

        Serial.println("[ScreenTest] === Looping ===");
    }
}

// =========================================
// Private Methods
// =========================================

void HUB75Display::drawIcon(int x, int y, uint16_t color) {
    for (int row = 0; row < ICON_H; row++) {
        uint8_t rowBits = BUS_ICON[row];
        for (int col = 0; col < ICON_W; col++) {
            if (rowBits & (0x80 >> col)) {
                _matrix->drawPixel(x + col, y + row, color);
            }
        }
    }
}

void HUB75Display::drawBusRow(const BusTarget& target, int row) {
    int y = row * ROW_HEIGHT;

    _matrix->setTextSize(1);

    // --- Line number (dark green, far left) ---
    _matrix->setTextColor(COLOR_BUS_NUM);
    _matrix->setCursor(XCOL_LINE_NUM, y + 1);
    _matrix->print(target.line);

    // --- Destination (white) ---
    _matrix->setTextColor(COLOR_DEST);
    _matrix->setCursor(XCOL_DEST, y + 1);

    size_t destLen = strnlen(target.destination, sizeof(target.destination));
    if (destLen <= MAX_DEST_CHARS) {
        _matrix->print(target.destination);
    } else {
        char truncated[MAX_DEST_CHARS + 1];
        strncpy(truncated, target.destination, MAX_DEST_CHARS);
        truncated[MAX_DEST_CHARS] = '\0';
        _matrix->print(truncated);
    }

    // --- Minutes remaining ---
    // No data: red "--"
    // Positive minutes: green (realtime) or yellow (scheduled)
    uint16_t minutesColor;
    
    if (target.no_data) {
        minutesColor = COLOR_NO_DATA;
        _matrix->setTextColor(minutesColor);
        _matrix->setCursor(XCOL_MINUTES, y + 1);
        _matrix->print("-");
    } else {
        minutesColor  = target.is_realtime ? COLOR_REALTIME : COLOR_SCHEDULED;
        _matrix->setTextColor(minutesColor);
        _matrix->setCursor(XCOL_MINUTES, y + 1);
        _matrix->printf("%d", target.minutes_remaining);
    }

    if(target.minutes_remaining  < 10){
        int currentX = _matrix->getCursorX();
        _matrix->drawPixel(currentX + 1, y + 1, minutesColor);
        _matrix->drawPixel(currentX + 1, y + 2, minutesColor);
    }
    drawIcon(XCOL_IMAGE_START, y, minutesColor);
}
