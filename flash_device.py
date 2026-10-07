"""Compile and flash both firmware and the LittleFS data image to the ESP32."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent
PLATFORMIO_VENV_EXECUTABLE = Path.home() / ".platformio" / "penv" / "Scripts" / "platformio.exe"


def find_platformio_command() -> list[str]:
    platformio_path = shutil.which("platformio") or shutil.which("pio")
    if platformio_path:
        return [platformio_path]

    if PLATFORMIO_VENV_EXECUTABLE.is_file():
        return [str(PLATFORMIO_VENV_EXECUTABLE)]

    raise FileNotFoundError(
        "PlatformIO was not found. Install PlatformIO or add its CLI to PATH."
    )


def run_platformio(platformio_command: list[str], target: str | None = None) -> None:
    arguments = [*platformio_command, "run"]
    if target:
        arguments += ["--target", target]
        print(f"\nRunning PlatformIO target: {target}")
    else:
        print("\nCompiling firmware")
    subprocess.run(arguments, cwd=PROJECT_DIR, check=True)


def main() -> int:
    try:
        platformio_command = find_platformio_command()
        # Build everything before touching the device, so a compile error
        # never leaves the board with new firmware but a stale filesystem.
        run_platformio(platformio_command)
        run_platformio(platformio_command, "uploadfs")
        run_platformio(platformio_command, "buildfs")
        run_platformio(platformio_command, "upload")
        print("Done flashing!")
    except (FileNotFoundError, subprocess.CalledProcessError) as error:
        print(f"Deployment failed: {error}", file=sys.stderr)
        return 1

    print("\nFirmware and LittleFS image compiled and flashed successfully.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
