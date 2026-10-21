#include "Display/SerialRenderer.h"

#include <Arduino.h>

namespace {

// Mirrors the row limit in HUB75Display::render(). Duplicated rather than shared
// because including HUB75Display.h would pull in the DMA library, which this
// class exists to avoid. The number is reported, not enforced: a target past the
// limit is still printed, and marked as one the panel would not show.
constexpr int kPanelRows = 3;

// computeMinutesRemaining() returns this when the clock is not set yet, which is
// a different condition from "no arrival" and worth seeing as itself.
constexpr int kMinutesUnknown = -1;

// Prints one arrival as "HH:MM (N min, realtime)". Shared by both arrivals on a
// row so the two always read the same way.
void printArrival(const char* eta, int minutes, bool isRealtime) {
    Serial.printf("%s (", (eta != nullptr && eta[0] != '\0') ? eta : "--:--");

    if (minutes == kMinutesUnknown) {
        Serial.print("clock unset");
    } else {
        Serial.printf("%d min", minutes);
    }

    Serial.printf(", %s)", isRealtime ? "realtime" : "scheduled");
}

}  // namespace

bool SerialRenderer::init() {
    Serial.println("[SerialRenderer] Active - panel output goes to this log");
    return true;
}

void SerialRenderer::render(const BusTarget* targets, int count) {
    if (targets == nullptr || count <= 0) {
        Serial.println("[SerialRenderer] render: no targets");
        return;
    }

    Serial.printf("[SerialRenderer] render: %d target(s)\n", count);

    for (int i = 0; i < count; i++) {
        const BusTarget& target = targets[i];

        // stationId is printed per row because the by-lines build can point each
        // row at a different station, and a row that is wrong because it is
        // reading the wrong station looks the same as one with no arrivals.
        Serial.printf("[SerialRenderer]   row %d: line='%s' station='%s' dest='%s' ", i,
                      target.line, target.stationId, target.destination);

        if (target.no_data) {
            // The fetcher ran and came back empty. Distinct from a row that was
            // never filled, which shows as an empty line and station above.
            Serial.print("NO DATA");
        } else {
            printArrival(target.last_known_ETA, target.minutes_remaining, target.is_realtime);

            Serial.print(" next=");
            if (target.has_next_arrival) {
                printArrival(target.next_ETA, target.next_minutes_remaining,
                             target.next_is_realtime);
            } else {
                Serial.print("none");
            }
        }

        if (i >= kPanelRows) {
            Serial.print("  [above the panel's row limit - not shown on hardware]");
        }

        Serial.println();
    }
}

void SerialRenderer::clear() {
    Serial.println("[SerialRenderer] clear");
}

void SerialRenderer::setBrightness(uint8_t brightness) {
    Serial.printf("[SerialRenderer] setBrightness(%u)\n", brightness);
}

void SerialRenderer::showMessage(const char* msg) {
    if (msg == nullptr) {
        Serial.println("[SerialRenderer] message: <null>");
        return;
    }

    // Newlines are shown as "\n" rather than printed, so a multi-line panel
    // message stays one line in the log and cannot be mistaken for separate
    // messages arriving at different times.
    Serial.print("[SerialRenderer] message: \"");
    for (const char* cursor = msg; *cursor != '\0'; cursor++) {
        if (*cursor == '\n') {
            Serial.print("\\n");
        } else {
            Serial.print(*cursor);
        }
    }
    Serial.println("\"");
}
