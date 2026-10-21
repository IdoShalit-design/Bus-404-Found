"""
Serial Failure Monitor — Watches the ESP32 serial output and flags every failed
bus data fetch, stalled screen, Wi-Fi drop and reboot.

Usage:
    python serial_monitor.py [--port COM5] [--baud 115200] [--log serial_log.txt]
                             [--stall 90] [--slow 15] [--summary 600]

Defaults:
    port    = auto-detect (CP210x / CH340 / ESP32-S3 native USB)
    baud    = 115200 (monitor_speed in platformio.ini)
    log     = serial_log.txt   every line, timestamped
    stall   = 90  s   alert when no screen update arrives for this long
    slow    = 15  s   alert when one fetch cycle (fetch -> screen) takes longer
    summary = 600 s   print running counts this often (0 = only on exit)

The port is opened with DTR and RTS held low, so attaching does not reset the
board. If the port goes away (cable pulled, "pio run -t upload" holding it),
the script waits and reattaches. Stop with Ctrl+C for a final summary.

Needs pyserial. PlatformIO's own Python already has it:
    ~/.platformio/penv/Scripts/python.exe serial_monitor.py
"""

import argparse
import re
import sys
import time
from collections import Counter
from datetime import datetime

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial is missing: pip install pyserial "
             "(or run with PlatformIO's python, which has it)")

# USB vendor IDs of the bridges this board family ships with:
# Silicon Labs CP210x, WCH CH340, Espressif native USB.
KNOWN_VIDS = {0x10C4, 0x1A86, 0x303A}

# Each failure the firmware can print, with a short category name for the
# summary. Order matters only for display; the first match wins.
FAILURE_PATTERNS = [
    ("http_error",     re.compile(r"HTTP GET failed, code: (-?\d+)")),
    ("json_error",     re.compile(r"JSON parse failed: (.*)")),
    ("api_error",      re.compile(r"API returned errors")),
    ("tls_connect",    re.compile(r"start_ssl_client: connect failed")),
    ("no_visits",      re.compile(r"No visits found for station (\S+)")),
    ("no_usable_eta",  re.compile(r"Line (\S+) at station (\S+): .*none with a usable eta")),
    ("line_absent",    re.compile(r"Line (\S+) not at station (\S+)")),
    ("line_no_data",   re.compile(r"^Line (\S+): No data")),
    ("wifi_attempt",   re.compile(r"\[WiFi\] Attempt #\d+ failed")),
    ("wifi_reconnect", re.compile(r"\[WiFi\] Reconnect failed")),
    ("wifi_dropped",   re.compile(r"\[WiFi\] Disconnected, attempting reconnect")),
    ("crash",          re.compile(r"Guru Meditation|abort\(\) was called|Backtrace:")),
    ("reboot",         re.compile(r"^rst:0x[0-9a-f]+ \((\w+)\)")),
]

WARNING_PATTERNS = [
    ("retry", re.compile(r"Retry \d+ for station (\S+)")),
]

FETCH_START = re.compile(r"--- Fetching bus arrivals ---")
SCREEN_UPDATE = re.compile(r"\[HUB75Display\] Screen update, (\d+) row")
SCREEN_ROW = re.compile(r"\[HUB75Display\]\s+row (\d+): line (\S+) \| (.*?) \| (.*)")


def now():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def find_port():
    for info in list_ports.comports():
        if info.vid in KNOWN_VIDS:
            return info.device
    return None


class Monitor:
    def __init__(self, args, log):
        self.args = args
        self.log = log
        self.started = time.monotonic()
        self.failures = Counter()
        self.warnings = Counter()
        self.cycles = 0
        self.bad_cycles = 0
        self.slowest_cycle = 0.0
        self.last_screen = time.monotonic()
        self.stall_reported = False
        self.fetch_started = None
        self.cycle_problems = []
        self.rows_left = 0
        self.last_summary = time.monotonic()

    def write(self, text):
        line = f"[{now()}] {text}"
        print(line)
        self.log.write(line + "\n")
        self.log.flush()

    def alert(self, text):
        self.write(f"!! {text}")

    def handle(self, line):
        self.log.write(f"[{now()}]    {line}\n")
        self.log.flush()

        for name, pattern in FAILURE_PATTERNS:
            if pattern.search(line):
                self.failures[name] += 1
                self.cycle_problems.append(name)
                self.alert(f"FAIL {name}: {line}")
                return

        for name, pattern in WARNING_PATTERNS:
            if pattern.search(line):
                self.warnings[name] += 1
                self.write(f"~~ WARN {name}: {line}")
                return

        if FETCH_START.search(line):
            self.fetch_started = time.monotonic()
            self.cycle_problems = []
            return

        match = SCREEN_UPDATE.search(line)
        if match:
            self.on_screen_update(int(match.group(1)))
            return

        match = SCREEN_ROW.search(line)
        if match and self.rows_left > 0:
            self.rows_left -= 1
            row, line_no, dest, arrivals = match.groups()
            if arrivals.strip() == "no data":
                self.failures["row_no_data"] += 1
                self.alert(f"FAIL row_no_data: row {row}, line {line_no} ({dest}) shows no data")
            self.write(f"   row {row}: line {line_no} | {dest} | {arrivals}")

    def on_screen_update(self, rows):
        moment = time.monotonic()
        self.cycles += 1
        self.rows_left = rows
        self.last_screen = moment
        if self.stall_reported:
            self.write("OK screen updates resumed")
            self.stall_reported = False

        duration = moment - self.fetch_started if self.fetch_started else None
        self.fetch_started = None
        if duration is not None:
            self.slowest_cycle = max(self.slowest_cycle, duration)

        if self.cycle_problems:
            self.bad_cycles += 1

        took = f"{duration:.1f}s" if duration is not None else "?"
        status = "with problems: " + ", ".join(self.cycle_problems) if self.cycle_problems else "OK"
        self.write(f"== cycle {self.cycles}, fetch took {took}, {status}")
        if duration is not None and duration > self.args.slow:
            self.failures["slow_cycle"] += 1
            self.alert(f"SLOW cycle: {took} (limit {self.args.slow}s)")

    def tick(self):
        moment = time.monotonic()
        silent = moment - self.last_screen
        if silent > self.args.stall and not self.stall_reported:
            self.failures["stall"] += 1
            self.stall_reported = True
            self.alert(f"STALL: no screen update for {silent:.0f}s")

        if self.args.summary and moment - self.last_summary >= self.args.summary:
            self.last_summary = moment
            self.summary()

    def summary(self):
        minutes = (time.monotonic() - self.started) / 60
        self.write(f"-- summary after {minutes:.1f} min: {self.cycles} cycles, "
                   f"{self.bad_cycles} with problems, slowest {self.slowest_cycle:.1f}s")
        for name, count in self.failures.most_common():
            self.write(f"--   {name}: {count}")
        for name, count in self.warnings.most_common():
            self.write(f"--   {name} (warning): {count}")


def open_port(port, baud):
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = baud
    ser.timeout = 0.5
    # Held low before open: on these boards RTS drives EN and DTR drives IO0,
    # so the defaults would reset the chip every time the monitor attaches.
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def main():
    parser = argparse.ArgumentParser(description="Flag bus data failures in the ESP32 serial output")
    parser.add_argument("--port", help="serial port, e.g. COM5 (default: auto-detect)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--log", default="serial_log.txt")
    parser.add_argument("--stall", type=float, default=90)
    parser.add_argument("--slow", type=float, default=15)
    parser.add_argument("--summary", type=float, default=600)
    args = parser.parse_args()

    with open(args.log, "a", encoding="utf-8") as log:
        monitor = Monitor(args, log)
        monitor.write(f"Monitoring, logging to {args.log}. Ctrl+C to stop.")
        ser = None
        buffer = b""
        try:
            while True:
                if ser is None:
                    port = args.port or find_port()
                    try:
                        if port is None:
                            raise serial.SerialException("no ESP32 port found")
                        ser = open_port(port, args.baud)
                        monitor.write(f"Attached to {port} at {args.baud} baud")
                    except serial.SerialException as error:
                        monitor.tick()
                        print(f"[{now()}] waiting for port: {error}", end="\r")
                        time.sleep(2)
                        continue

                try:
                    chunk = ser.read(ser.in_waiting or 1)
                except serial.SerialException as error:
                    monitor.alert(f"PORT LOST: {error}")
                    ser.close()
                    ser = None
                    continue

                buffer += chunk
                while b"\n" in buffer:
                    raw, buffer = buffer.split(b"\n", 1)
                    line = raw.decode("utf-8", errors="replace").rstrip("\r")
                    if line.strip():
                        monitor.handle(line)
                monitor.tick()
        except KeyboardInterrupt:
            pass
        finally:
            if ser is not None:
                ser.close()
            monitor.summary()
            monitor.write("Stopped.")


if __name__ == "__main__":
    main()
