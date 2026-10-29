"""Exercise serial command boundaries without opening a physical serial port."""

import contextlib
import io
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import MagicMock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import ir_terminal


class SerialClientTests(unittest.TestCase):
    def serial_fixture(self, request):
        connection = MagicMock()
        connection.__enter__.return_value = connection
        connection.write.return_value = len(request)
        connection.in_waiting = 0
        connection.read.side_effect = (b"queued\n", b"complete\n")
        factory = MagicMock(return_value=connection)
        return connection, factory

    def test_one_write_collects_output_and_closes_connection(self):
        request = b"send light_on 0\n"
        connection, factory = self.serial_fixture(request)
        with patch.dict(sys.modules, {"serial": SimpleNamespace(Serial=factory)}), \
                patch.object(ir_terminal.time, "sleep"), \
                patch.object(ir_terminal.time, "monotonic", side_effect=(0, 0.1, 0.2, 1.1)):
            output = ir_terminal.invoke("COM7", "send light_on 0", timeout=1)
        self.assertEqual(output, "queued\ncomplete\n")
        factory.assert_called_once_with(port=None, baudrate=115200, timeout=0.1,
                                        write_timeout=1)
        connection.write.assert_called_once_with(request)
        self.assertFalse(connection.dtr)
        self.assertFalse(connection.rts)
        self.assertEqual(connection.port, "COM7")
        connection.__exit__.assert_called_once()

    def test_partial_write_fails_without_retry(self):
        connection, factory = self.serial_fixture(b"send light_on 0\n")
        connection.write.return_value = 4
        with patch.dict(sys.modules, {"serial": SimpleNamespace(Serial=factory)}), \
                patch.object(ir_terminal.time, "sleep"):
            with self.assertRaisesRegex(OSError, "not retried"):
                ir_terminal.invoke("COM7", "send light_on 0")
        connection.write.assert_called_once()
        connection.__exit__.assert_called_once()

    def test_x0a_timeout_never_repeats_an_actuation(self):
        request = b"x0a set power off\n"
        connection, factory = self.serial_fixture(request)
        connection.read.side_effect = (b"X0A accepted local_id=1; result pending\n",)
        with patch.dict(sys.modules, {"serial": SimpleNamespace(Serial=factory)}), \
                patch.object(ir_terminal.time, "sleep"), \
                patch.object(ir_terminal.time, "monotonic", side_effect=(0, 0.1, 1.1)):
            output = ir_terminal.invoke("COM7", "x0a set power off", timeout=1)
        self.assertIn("result pending", output)
        self.assertNotIn("ReadbackMatched", output)
        connection.write.assert_called_once_with(request)
        connection.__exit__.assert_called_once()

    def test_invalid_input_never_opens_serial(self):
        factory = MagicMock()
        with patch.dict(sys.modules, {"serial": SimpleNamespace(Serial=factory)}):
            for command in ("", " ", "send x 0\nsend y 0", "send x 0\r", "\x1b[A",
                            "show\x00", "caf\u00e9", "x" * 256, None):
                with self.subTest(command=command), self.assertRaises(ValueError):
                    ir_terminal.invoke("COM7", command)
            for timeout in (0, 121, float("nan"), float("inf"), True, "15"):
                with self.subTest(timeout=timeout), self.assertRaises(ValueError):
                    ir_terminal.invoke("COM7", "codes", timeout)
        factory.assert_not_called()

    def test_saved_helper_requires_integer_zone_and_valid_name(self):
        with patch.object(ir_terminal, "invoke", return_value="queued") as dispatch:
            self.assertEqual(ir_terminal.send_saved("COM7", "fan_low", 2), "queued")
            dispatch.assert_called_once_with("COM7", "send fan_low 2")
            dispatch.reset_mock()
            for zone in (True, 1.0, "1", -1, 3):
                with self.subTest(zone=zone), self.assertRaises(ValueError):
                    ir_terminal.send_saved("COM7", "fan_low", zone)
            for name in (None, "", "Fan", "x" * 16, "x\n"):
                with self.subTest(name=name), self.assertRaises(ValueError):
                    ir_terminal.send_saved("COM7", name)
            dispatch.assert_not_called()

    def test_cli_preserves_explicit_confirmation(self):
        arguments = ("ir_terminal.py", "--port", "COM7", "save", "fan_low",
                     "--confirmed", "--overwrite")
        with patch.object(sys, "argv", arguments), \
                patch.object(ir_terminal, "invoke", return_value="saved\n") as dispatch, \
                contextlib.redirect_stdout(io.StringIO()) as output:
            ir_terminal.main()
        dispatch.assert_called_once_with("COM7", "save fan_low --confirmed --overwrite", 15)
        self.assertEqual(output.getvalue(), "saved\n")


if __name__ == "__main__":
    unittest.main()
