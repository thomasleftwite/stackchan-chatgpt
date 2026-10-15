"""Flash the approved application only, preserving bootloader/table/data.

Requires a preceding inspect_device_partitions.py run. This is deliberately not
a replacement for PlatformIO upload when migrating to the project's own layout.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--esptool", type=Path, required=True)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--partition-table", type=Path,
                        help="Reviewed table already installed on this device; defaults to the pre-migration table")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    image = root / ".pio/build/atoms3r/firmware.bin"
    content = image.read_bytes()
    if hashlib.sha256(content).hexdigest().lower() != args.sha256.lower():
        raise SystemExit("Firmware changed; review and approve the new image first")
    table_path = args.partition_table or (root / ".pio/device-check/partitions-before.bin")
    table = table_path.read_bytes()
    matches = []
    for offset in range(0, len(table), 32):
        magic, kind, subtype, start, size, label, flags = struct.unpack_from("<HBBLL16sL", table, offset)
        if magic != 0x50AA:
            break
        if kind == 0 and subtype in (0, 0x10) and start == 0x10000:
            matches.append(size)
    if len(matches) != 1 or len(content) > matches[0]:
        raise SystemExit("Expected factory/app0 partition at 0x10000 was not found or is too small")
    print(f"Application only: offset=0x10000, bytes={len(content)}", flush=True)
    command = [sys.executable, str(args.esptool), "--chip", "esp32s3", "--port", args.port,
               "--baud", "460800", "--before", "default_reset", "--after", "hard_reset",
               "write_flash", "--flash_mode", "keep", "--flash_freq", "keep", "--flash_size", "keep",
               "0x10000", str(image)]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    # Do not expose MAC addresses or arbitrary tool output.
    for line in result.stdout.splitlines():
        if line.startswith(("Chip is ", "Features:", "Flash will be erased from", "Wrote ",
                            "Hash of data verified", "Hard resetting", "A fatal error occurred:")):
            print(line)
    if result.returncode:
        raise SystemExit(result.returncode)
    if "Hash of data verified" not in result.stdout:
        raise SystemExit("Upload finished without a verification confirmation")


if __name__ == "__main__":
    main()
