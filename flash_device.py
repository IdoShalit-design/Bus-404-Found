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


def run_platformio(platformio_command: list[str], target: str) -> None:
    print(f"\nRunning PlatformIO target: {target}")
    subprocess.run(
        [*platformio_command, "run", "--target", target],
        cwd=PROJECT_DIR,
        check=True,
    )


def main() -> int:
    try:
        platformio_command = find_platformio_command()
        run_platformio(platformio_command, "upload")
        run_platformio(platformio_command, "uploadfs")
    except (FileNotFoundError, subprocess.CalledProcessError) as error:
        print(f"Deployment failed: {error}", file=sys.stderr)
        return 1

    print("\nFirmware and LittleFS image compiled and flashed successfully.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
