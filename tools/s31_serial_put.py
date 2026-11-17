#!/usr/bin/env python3
"""Send one small file to the S31 SD card over the CH340 UART0 port."""

import argparse
import os
from pathlib import Path
import re
import sys
import time
import zlib

MAX_SIZE = 8 * 1024 * 1024
SAFE_NAME = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{0,94}\Z")


def file_metadata(path: Path, remote_name: str | None = None) -> tuple[str, bytes, int]:
    name = remote_name or path.name
    if not SAFE_NAME.fullmatch(name):
        raise ValueError("remote name must use only ASCII letters, digits, '.', '_' or '-'")
    data = path.read_bytes()
    if not data or len(data) > MAX_SIZE:
        raise ValueError(f"file size must be between 1 and {MAX_SIZE} bytes")
    return name, data, zlib.crc32(data) & 0xFFFFFFFF


def wait_response(port, expected: str, timeout: float) -> str:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = port.readline().decode("ascii", errors="replace").strip()
        if not line.startswith("@S31 "):
            continue
        response = line[5:]
        if response == expected:
            return response
        if response.startswith("ERR "):
            raise RuntimeError(f"board rejected transfer: {response}")
    raise TimeoutError(f"timed out waiting for @S31 {expected}")


def send_file(port, name: str, data: bytes, crc: int) -> None:
    port.reset_input_buffer()
    header = f"S31PUT {len(data)} {crc:08x} {name}\n".encode("ascii")
    port.write(header)
    port.flush()
    wait_response(port, "READY", 3.0)

    started = time.monotonic()
    for offset in range(0, len(data), 1024):
        port.write(data[offset : offset + 1024])
    port.flush()
    wait_response(port, "OK", max(10.0, len(data) / 5000.0))
    elapsed = max(time.monotonic() - started, 0.001)
    print(f"sent {len(data)} bytes as {name} in {elapsed:.1f}s ({len(data) / elapsed / 1024:.1f} KiB/s)")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path)
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--settle", type=float, default=8.0,
                        help="seconds to wait after opening CH340 (opening may reset the board)")
    parser.add_argument("--name", help="destination filename under /sdcard/roms")
    args = parser.parse_args()

    try:
        import serial
    except ImportError:
        print("pyserial is required: python -m pip install pyserial", file=sys.stderr)
        return 2

    try:
        name, data, crc = file_metadata(args.file, args.name)
        port = serial.Serial(port=None, baudrate=args.baud, timeout=0.2, write_timeout=10)
        port.dtr = False
        port.rts = False
        port.port = args.port
        port.open()
        try:
            time.sleep(max(args.settle, 0.0))
            send_file(port, name, data, crc)
        finally:
            port.close()
    except (OSError, ValueError, RuntimeError, TimeoutError, serial.SerialException) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
