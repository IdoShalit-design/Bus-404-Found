#include "App.h"

// The Arduino default is 8 KB, which this firmware overruns: the provisioning
// portal runs inside setup(), so a form submission executes the whole WebServer
// handler chain, the config parser and LittleFS on this one stack. That path
// tripped the loopTask stack canary and rebooted the board every time the user
// pressed "Save and Apply" -- the reboot looked like the portal ignoring the
// form, since the crash happened before anything was written.
//
// The parser's own footprint was cut alongside this (see ConfigLoader.cpp), but
// the headroom is kept: the deepest path here is a nested call chain ending in
// newlib's printf family, which is stack-hungry and not ours to shrink.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

// Single owner of all runtime state. Static storage so that the BusTarget
// pointers into its RuntimeConfig stay valid for the lifetime of the program.
static App app;

void setup() {
    app.setup();
}

void loop() {
    app.loop();
}
