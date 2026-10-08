"""Compile and flash both firmware and the LittleFS data image to the ESP32.

Usage:
    python flash_device.py                 # the normal hardware build
    python flash_device.py hw-log          # the serial-log build instead

The environment is always passed explicitly. A bare "pio run" builds and flashes
every environment in platformio.ini in turn, which is both slow and wrong -- the
board keeps whichever one happened to run last.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent
PLATFORMIO_VENV_EXECUTABLE = Path.home() / ".platformio" / "penv" / "Scripts" / "platformio.exe"
DEFAULT_ENVIRONMENT = "esp32-s3-devkitc-1"


def find_platformio_command() -> list[str]:
    platformio_path = shutil.which("platformio") or shutil.which("pio")
    if platformio_path:
        return [platformio_path]

    if PLATFORMIO_VENV_EXECUTABLE.is_file():
        return [str(PLATFORMIO_VENV_EXECUTABLE)]

    raise FileNotFoundError(
        "PlatformIO was not found. Install PlatformIO or add its CLI to PATH."
    )


def run_platformio(
    platformio_command: list[str], environment: str, target: str | None = None
) -> None:
    arguments = [*platformio_command, "run", "-e", environment]
    if target:
        arguments += ["--target", target]
        print(f"\nRunning PlatformIO target: {target} ({environment})")
    else:
        print(f"\nCompiling firmware ({environment})")
    subprocess.run(arguments, cwd=PROJECT_DIR, check=True)


def main() -> int:
    environment = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_ENVIRONMENT

    try:
        platformio_command = find_platformio_command()
        # Both images are built before either is written, so a compile error
        # cannot leave the board carrying new firmware and a stale filesystem.
        run_platformio(platformio_command, environment)
        run_platformio(platformio_command, environment, "buildfs")
        run_platformio(platformio_command, environment, "upload")
        run_platformio(platformio_command, environment, "uploadfs")
    except (FileNotFoundError, subprocess.CalledProcessError) as error:
        print(f"Deployment failed: {error}", file=sys.stderr)
        return 1

    print(f"\nFirmware and LittleFS image flashed successfully ({environment}).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
