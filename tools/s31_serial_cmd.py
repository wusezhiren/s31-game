#!/usr/bin/env python3
"""Run or stop an S31 game through the UART0 test command interface."""

import argparse
import re
import sys
import time

SAFE_NAME = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{0,94}\Z")


def send_command(port, command: str, name: str | None = None, timeout: float = 20.0) -> str:
    if command == "run":
        if not name or not SAFE_NAME.fullmatch(name):
            raise ValueError("ROM name must use only ASCII letters, digits, '.', '_' or '-'")
        request = f"S31RUN {name}\n"
        prefix = "RUN "
    else:
        request = "S31STOP\n"
        prefix = "STOP "
    port.reset_input_buffer()
    port.write(request.encode("ascii"))
    port.flush()
    deadline = time.monotonic() + timeout
    responses = ""
    while time.monotonic() < deadline:
        line = port.readline().decode("ascii", errors="replace").strip()
        responses += line
        compact = "".join(responses.split())
        expected = "@S31" + prefix.strip()
        if expected + "OK" in compact:
            return prefix + "OK"
        error_at = compact.find(expected + "ERR")
        if error_at >= 0:
            raise RuntimeError(compact[error_at + 4:])
        if not line.startswith("@S31 "):
            continue
        response = line[5:]
        if response.startswith(prefix):
            if " ERR " in response:
                raise RuntimeError(response)
            return response
    raise TimeoutError(f"timed out waiting for {prefix.strip()} response")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("run", "stop"))
    parser.add_argument("name", nargs="?")
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--settle", type=float, default=8.0,
                        help="seconds to wait after opening CH340 (opening may reset the board)")
    args = parser.parse_args()
    try:
        import serial
        port = serial.Serial(port=None, baudrate=args.baud, timeout=0.2, write_timeout=10)
        port.dtr = False
        port.rts = False
        port.port = args.port
        port.open()
        try:
            time.sleep(max(args.settle, 0.0))
            print(send_command(port, args.command, args.name))
        finally:
            port.close()
    except (OSError, ValueError, RuntimeError, TimeoutError, serial.SerialException) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
