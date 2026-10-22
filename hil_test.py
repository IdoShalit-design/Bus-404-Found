"""Hardware-in-the-loop test -- flash the ESP32, watch its serial output for a
fixed time, then exit with a pass or fail code. Made for CI, works by hand too.

Usage:
    python hil_test.py [--env esp32-s3-devkitc-1] [--port COM5]
                       [--duration 600] [--min-cycles 2] [--max-failures 2]
                       [--fail-on crash] [--max-reboots 1]
                       [--firmware-only] [--skip-flash]
                       [--log hil_serial_log.txt] [--report hil_report.md]

Steps:
    1. Build firmware and the LittleFS image, then upload both (as
       flash_device.py does). --firmware-only keeps the filesystem that is on
       the board, so the Wi-Fi credentials saved there stay. --skip-flash tests
       whatever is on the board now.
    2. Attach to the serial port and run serial_monitor.py's Monitor on every
       line for --duration seconds.
    3. Pass when all of these hold:
       - the screen updated at least --min-cycles times;
       - at most --max-failures failure events were seen, of any kind (every
         "!! FAIL", STALL and SLOW line serial_monitor.py prints counts once);
       - no failure named in --fail-on was seen, however few.

Exit codes:
    0  pass
    1  fail (the firmware ran, but broke a rule)
    2  could not test (build, upload or serial port error)

The upload resets the board, and that boot prints one "rst:" line, so
--max-reboots is 1 after a flash and 0 with --skip-flash unless set. The
allowed reboots do not count toward --max-failures.

A Markdown report is written to --report, and also appended to
$GITHUB_STEP_SUMMARY when that is set (GitHub Actions).
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path
from types import SimpleNamespace

from flash_device import DEFAULT_ENVIRONMENT, PROJECT_DIR, find_platformio_command

try:
    import serial
except ImportError:
    print("pyserial is missing: pip install pyserial "
          "(or run with PlatformIO's python, which has it)", file=sys.stderr)
    sys.exit(2)

from serial_monitor import Monitor, find_port, open_port

EXIT_PASS = 0
EXIT_FAIL = 1
EXIT_ERROR = 2

# Failure names serial_monitor.py can count, plus the ones only this script
# adds. Checked so a typo in --fail-on is an error, not a rule that never fires.
KNOWN_FAILURES = {
    "http_error", "json_error", "api_error", "tls_connect", "no_visits",
    "no_usable_eta", "line_absent", "line_no_data", "wifi_attempt",
    "wifi_reconnect", "wifi_dropped", "crash", "reboot", "row_no_data",
    "slow_cycle", "stall", "port_lost",
}


def flash(environment: str, port: str | None, firmware_only: bool) -> None:
    platformio = find_platformio_command()
    upload_port = ["--upload-port", port] if port else []

    def run(target: str | None = None, extra: list[str] = ()) -> None:
        arguments = [*platformio, "run", "-e", environment]
        if target:
            arguments += ["--target", target]
        print(f"\n>> pio run -e {environment}" + (f" -t {target}" if target else ""), flush=True)
        subprocess.run([*arguments, *extra], cwd=PROJECT_DIR, check=True)

    # Both images are built before either is written, so a compile error
    # cannot leave the board with new firmware and a stale filesystem.
    run()
    if not firmware_only:
        run("buildfs")
    run("upload", upload_port)
    if not firmware_only:
        run("uploadfs", upload_port)


def attach(port: str | None, baud: int, timeout: float):
    """Open the board's port, waiting for it to come back after the upload."""
    deadline = time.monotonic() + timeout
    last_error = "no ESP32 port found"
    while time.monotonic() < deadline:
        name = port or find_port()
        if name:
            try:
                return open_port(name, baud), name
            except serial.SerialException as error:
                last_error = str(error)
        time.sleep(1)
    raise serial.SerialException(f"could not open the serial port in {timeout:.0f}s: {last_error}")


def watch(ser, monitor: Monitor, duration: float) -> None:
    deadline = time.monotonic() + duration
    buffer = b""
    while time.monotonic() < deadline:
        try:
            chunk = ser.read(ser.in_waiting or 1)
        except serial.SerialException as error:
            # Unlike serial_monitor.py, do not wait for the port to return: on
            # a CI rig a lost port is itself a result worth failing on.
            monitor.failures["port_lost"] += 1
            monitor.alert(f"FAIL port_lost: {error}")
            return
        buffer += chunk
        while b"\n" in buffer:
            raw, buffer = buffer.split(b"\n", 1)
            line = raw.decode("utf-8", errors="replace").rstrip("\r")
            if line.strip():
                monitor.handle(line)
        monitor.tick()


def verdict(monitor: Monitor, args) -> list[str]:
    """Return the broken rules; an empty list is a pass."""
    problems = []
    if monitor.cycles < args.min_cycles:
        problems.append(f"only {monitor.cycles} screen update(s), need {args.min_cycles}")
    total = sum(monitor.failures.values()) - min(monitor.failures["reboot"], args.max_reboots)
    if total > args.max_failures:
        problems.append(f"{total} failure events, allowed {args.max_failures}")
    for name in args.fail_on:
        count = monitor.failures[name]
        allowed = args.max_reboots if name == "reboot" else 0
        if count > allowed:
            problems.append(f"{name}: {count}" + (f" (allowed {allowed})" if allowed else ""))
    return problems


def write_report(path: Path, monitor: Monitor, args, problems: list[str], minutes: float) -> str:
    result = "PASS" if not problems else "FAIL"
    lines = [
        f"## Hardware test: {result}",
        "",
        f"- Environment: `{args.env}`" + (" (not flashed)" if args.skip_flash else ""),
        f"- Watched for {minutes:.1f} min",
        f"- Screen updates: {monitor.cycles} ({monitor.bad_cycles} with problems), "
        f"slowest fetch {monitor.slowest_cycle:.1f}s",
        f"- Allowed failure events: {args.max_failures}",
        f"- Fails the run at once: {', '.join(args.fail_on) or 'nothing'}",
    ]
    if problems:
        lines += ["", "**Broken rules:**", *[f"- {p}" for p in problems]]
    counted = monitor.failures.most_common() + [(f"{n} (warning)", c) for n, c in monitor.warnings.most_common()]
    if counted:
        lines += ["", "| Event | Count |", "| --- | --- |", *[f"| {n} | {c} |" for n, c in counted]]
    text = "\n".join(lines) + "\n"
    path.write_text(text, encoding="utf-8")
    step_summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if step_summary:
        with open(step_summary, "a", encoding="utf-8") as summary:
            summary.write(text)
    return text


def main() -> int:
    parser = argparse.ArgumentParser(description="Flash the ESP32 and pass/fail on its serial output")
    parser.add_argument("--env", default=DEFAULT_ENVIRONMENT, help="PlatformIO environment to flash")
    parser.add_argument("--port", help="serial port, e.g. COM5 (default: auto-detect)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=600, help="seconds to watch the serial output")
    parser.add_argument("--min-cycles", type=int, default=2, help="screen updates needed to pass")
    parser.add_argument("--max-failures", type=int, default=2,
                        help="failure events of any kind allowed before the run fails")
    parser.add_argument("--fail-on", default="crash",
                        help="comma-separated failure names that fail the run even once")
    parser.add_argument("--max-reboots", type=int, help="reboot lines allowed (default 1 after a flash, else 0)")
    parser.add_argument("--stall", type=float, default=90, help="seconds without a screen update that count as a stall")
    parser.add_argument("--slow", type=float, default=15, help="seconds a fetch cycle may take")
    parser.add_argument("--attach-timeout", type=float, default=30, help="seconds to wait for the port after upload")
    parser.add_argument("--firmware-only", action="store_true", help="upload firmware, keep the board's filesystem")
    parser.add_argument("--skip-flash", action="store_true", help="test what is on the board now")
    parser.add_argument("--log", default="hil_serial_log.txt")
    parser.add_argument("--report", default="hil_report.md")
    args = parser.parse_args()

    args.fail_on = [name.strip() for name in args.fail_on.split(",") if name.strip()]
    unknown = set(args.fail_on) - KNOWN_FAILURES
    if unknown:
        parser.error(f"unknown failure name(s) in --fail-on: {', '.join(sorted(unknown))}. "
                     f"Known: {', '.join(sorted(KNOWN_FAILURES))}")
    if args.max_reboots is None:
        args.max_reboots = 0 if args.skip_flash else 1

    if not args.skip_flash:
        try:
            flash(args.env, args.port, args.firmware_only)
        except (FileNotFoundError, subprocess.CalledProcessError) as error:
            print(f"Flash failed: {error}", file=sys.stderr)
            return EXIT_ERROR

    with open(args.log, "w", encoding="utf-8") as log:
        # serial_monitor.py's Monitor reads its limits from these attributes.
        # summary=0: one summary at the end, not periodic ones.
        monitor = Monitor(SimpleNamespace(stall=args.stall, slow=args.slow, summary=0), log)
        try:
            ser, name = attach(args.port, args.baud, args.attach_timeout)
        except serial.SerialException as error:
            print(f"Serial error: {error}", file=sys.stderr)
            return EXIT_ERROR

        monitor.write(f"Attached to {name} at {args.baud} baud, watching for {args.duration:.0f}s")
        # The stall clock starts at attach, not at script start, so the
        # build and upload time does not count as a silent screen.
        monitor.last_screen = time.monotonic()
        started = time.monotonic()
        try:
            watch(ser, monitor, args.duration)
        except KeyboardInterrupt:
            monitor.write("Stopped early by Ctrl+C")
        finally:
            ser.close()
        monitor.summary()

    problems = verdict(monitor, args)
    print("\n" + write_report(Path(args.report), monitor, args, problems, (time.monotonic() - started) / 60))
    return EXIT_PASS if not problems else EXIT_FAIL


if __name__ == "__main__":
    raise SystemExit(main())
