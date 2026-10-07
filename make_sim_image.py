"""Build one full flash image of the log-only firmware, for a simulator.

A simulator has no esptool and no serial port, so it cannot be handed the four
or five pieces that make up a flashed device. It needs a single image that
already contains the bootloader, the partition table and the filesystem.

PlatformIO's own firmware.factory.bin merges the bootloader, the partition
table, boot_app0 and the application -- but not LittleFS. Both the saved
configuration and the portal page live on LittleFS, so an image without it
mounts nothing and App::loadConfig() halts before the fetcher ever runs. That is
the reason this script exists rather than reusing firmware.factory.bin.

Usage:
    python make_sim_image.py

Output:
    .pio/build/sim/flash.bin    -- 8 MB, the path wokwi.toml points at
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent
BUILD_DIR = PROJECT_DIR / ".pio" / "build" / "sim"
PLATFORMIO = Path.home() / ".platformio" / "penv" / "Scripts" / "platformio.exe"
PYTHON = Path.home() / ".platformio" / "penv" / "Scripts" / "python.exe"
FRAMEWORK_PARTITIONS = (
    Path.home()
    / ".platformio"
    / "packages"
    / "framework-arduinoespressif32"
    / "tools"
    / "partitions"
)

ENVIRONMENT = "sim"
CHIP = "esp32s3"
FLASH_SIZE = "8MB"
OUTPUT_IMAGE = BUILD_DIR / "flash.bin"

# Offsets from the platform's default_8MB.csv, which is the table this board and
# flash size select. The filesystem partition is labelled "spiffs" there even
# though it carries a LittleFS image -- that is the partition name, not the
# format, and changing board_build.filesystem does not rename it.
LITTLEFS_OFFSET = 0x670000
LITTLEFS_PARTITION_SIZE = 0x180000

# The bootloader sits at 0x0 on the ESP32-S3, not at 0x1000 as on the original
# ESP32. Taken from the offsets PlatformIO prints when it builds
# firmware.factory.bin, so the two images agree.
IMAGE_PARTS: list[tuple[int, Path]] = [
    (0x0000, BUILD_DIR / "bootloader.bin"),
    (0x8000, BUILD_DIR / "partitions.bin"),
    (0xE000, FRAMEWORK_PARTITIONS / "boot_app0.bin"),
    (0x10000, BUILD_DIR / "firmware.bin"),
    (LITTLEFS_OFFSET, BUILD_DIR / "littlefs.bin"),
]


def run_platformio(target: str | None = None) -> None:
    arguments = [str(PLATFORMIO), "run", "-e", ENVIRONMENT]
    if target:
        arguments += ["--target", target]
    print(f"\n> platformio run -e {ENVIRONMENT}" + (f" -t {target}" if target else ""))
    subprocess.run(arguments, cwd=PROJECT_DIR, check=True)


def check_parts() -> None:
    missing = [str(path) for _, path in IMAGE_PARTS if not path.is_file()]
    if missing:
        raise FileNotFoundError("Missing build output:\n  " + "\n  ".join(missing))

    # Checked here rather than left to the simulator: an oversized image is
    # written into the next partition, and the resulting failure shows up as a
    # corrupt filesystem at run time with nothing pointing back to this step.
    littlefs = BUILD_DIR / "littlefs.bin"
    if littlefs.stat().st_size > LITTLEFS_PARTITION_SIZE:
        raise ValueError(
            f"littlefs.bin is {littlefs.stat().st_size} bytes, but the partition "
            f"holds {LITTLEFS_PARTITION_SIZE}. Remove files from data/."
        )


def merge_image() -> None:
    arguments = [
        str(PYTHON), "-m", "esptool",
        "--chip", CHIP,
        "merge-bin",
        "-o", str(OUTPUT_IMAGE),
        # Padded to the full flash size so the simulator sees the same geometry
        # the partition table was built for. Without this the image stops short
        # of LITTLEFS_OFFSET and the filesystem cannot be found at all.
        "--pad-to-size", FLASH_SIZE,
    ]
    for offset, path in IMAGE_PARTS:
        arguments += [hex(offset), str(path)]

    print(f"\n> merging {len(IMAGE_PARTS)} parts into {OUTPUT_IMAGE.name}")
    subprocess.run(arguments, cwd=PROJECT_DIR, check=True)


def main() -> int:
    try:
        run_platformio()
        run_platformio("buildfs")
        check_parts()
        merge_image()
    except (FileNotFoundError, ValueError, subprocess.CalledProcessError) as error:
        print(f"\nFailed to build the simulator image: {error}", file=sys.stderr)
        return 1

    size_mb = OUTPUT_IMAGE.stat().st_size / (1024 * 1024)
    print(f"\nWrote {OUTPUT_IMAGE} ({size_mb:.0f} MB).")
    print("Open the project in Wokwi, or press F1 > 'Wokwi: Start Simulator' in VS Code.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
