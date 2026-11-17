import importlib.util
from pathlib import Path
import tempfile
import unittest
import zlib


TOOL_PATH = Path(__file__).parents[1] / "tools" / "s31_serial_put.py"
SPEC = importlib.util.spec_from_file_location("s31_serial_put", TOOL_PATH)
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)
CMD_PATH = Path(__file__).parents[1] / "tools" / "s31_serial_cmd.py"
CMD_SPEC = importlib.util.spec_from_file_location("s31_serial_cmd", CMD_PATH)
CMD = importlib.util.module_from_spec(CMD_SPEC)
CMD_SPEC.loader.exec_module(CMD)


class FakePort:
    def __init__(self, responses):
        self.responses = [response.encode("ascii") for response in responses]
        self.writes = []

    def reset_input_buffer(self):
        pass

    def readline(self):
        return self.responses.pop(0) if self.responses else b""

    def write(self, data):
        self.writes.append(bytes(data))
        return len(data)

    def flush(self):
        pass


class SerialUploadToolTest(unittest.TestCase):
    def test_metadata_and_transfer(self):
        payload = bytes(range(256)) * 5
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.nes"
            path.write_bytes(payload)
            name, data, crc = TOOL.file_metadata(path)

        self.assertEqual(name, "test.nes")
        self.assertEqual(data, payload)
        self.assertEqual(crc, zlib.crc32(payload) & 0xFFFFFFFF)

        port = FakePort(["boot log\n", "@S31 READY\r\n", "@S31 OK\r\n"])
        TOOL.send_file(port, name, data, crc)
        self.assertEqual(port.writes[0], f"S31PUT {len(data)} {crc:08x} test.nes\n".encode())
        self.assertEqual(b"".join(port.writes[1:]), payload)

    def test_rejects_unsafe_name(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.nes"
            path.write_bytes(b"test")
            with self.assertRaises(ValueError):
                TOOL.file_metadata(path, "../test.nes")

    def test_reports_board_error(self):
        port = FakePort(["@S31 ERR BUSY\n"])
        with self.assertRaisesRegex(RuntimeError, "ERR BUSY"):
            TOOL.send_file(port, "test.nes", b"test", zlib.crc32(b"test"))

    def test_run_and_stop_commands(self):
        run_port = FakePort(["log\n", "@S31 RUN OK\n"])
        self.assertEqual(CMD.send_command(run_port, "run", "test.gb"), "RUN OK")
        self.assertEqual(run_port.writes, [b"S31RUN test.gb\n"])
        stop_port = FakePort(["@S31 STOP OK\n"])
        self.assertEqual(CMD.send_command(stop_port, "stop"), "STOP OK")
        self.assertEqual(stop_port.writes, [b"S31STOP\n"])


if __name__ == "__main__":
    unittest.main()
