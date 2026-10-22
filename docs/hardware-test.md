# Hardware test (CI)

`hil_test.py` flashes the ESP32, watches its serial output for a fixed time,
and exits with a pass or fail code. `.github/workflows/hardware-test.yml` runs
it on every pull request from this repository, and by hand from the Actions
tab (**Hardware test > Run workflow**).

GitHub's own runners have no USB, so the workflow runs on a **self-hosted
runner** on the PC the board is plugged into.

## Run it by hand

```powershell
~\.platformio\penv\Scripts\python.exe hil_test.py                  # flash firmware + filesystem, watch 600 s
~\.platformio\penv\Scripts\python.exe hil_test.py --firmware-only  # keep the board's saved Wi-Fi and targets
~\.platformio\penv\Scripts\python.exe hil_test.py --skip-flash --duration 120
```

| Exit code | Meaning |
| --- | --- |
| 0 | Pass |
| 1 | Fail: the firmware ran but broke a rule |
| 2 | Could not test: build, upload or serial port error |

The run **passes** when all of these hold:

- The screen updated at least `--min-cycles` times (default 2).
- At most `--max-failures` failure events were seen (default 2). Every
  `!! FAIL`, `STALL` and `SLOW` line that `serial_monitor.py` prints counts
  once, of any kind: HTTP errors, Wi-Fi drops, rows with no data, reboots.
- No failure named in `--fail-on` was seen (default `crash`), however few.

One reboot is allowed after a flash, because the upload resets the board. It
does not count toward `--max-failures`.

Output: `hil_serial_log.txt` (every line) and `hil_report.md` (summary). In
CI, the report is the job summary and both files are a run artifact.

Close any serial monitor first. The COM port takes one process only.

## Set up the runner (one time)

1. On GitHub, open **Settings > Actions > Runners > New self-hosted runner**
   and pick Windows. Follow the shown commands on the PC with the board.
2. When it asks for labels, add `esp32`. The workflow asks for
   `[self-hosted, Windows, esp32]`.
3. Run the runner as your own Windows user (not as a service under another
   account), so it finds PlatformIO in `%USERPROFILE%\.platformio`.
4. Optional: set the repository variable `HIL_PORT` (for example `COM5`).
   Without it, the script finds the port by USB vendor ID.
5. Optional: set the repository secrets `WIFI_SSID` and `WIFI_PASSWORD`. With
   them, CI writes `data/wifi_credentials.json` and flashes the full
   filesystem. Without them, CI flashes firmware only and the board keeps the
   Wi-Fi it has saved.

Pull requests from forks never run on this PC. The workflow skips them,
because they would run someone else's code on your machine.

## When the board or the PC is off

A pull request job waits in the queue until the runner is online. GitHub
cancels a job that waits more than 24 hours. If the runner is online but the
board is not plugged in, the job ends with exit code 2 (could not test).
