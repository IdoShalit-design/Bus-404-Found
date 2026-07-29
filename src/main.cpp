#include "App.h"

// Single owner of all runtime state. Static storage so that the BusTarget
// pointers into its RuntimeConfig stay valid for the lifetime of the program.
static App app;

void setup() {
    app.setup();
}

void loop() {
    app.loop();
}
