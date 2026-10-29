"""Send one explicit console command over USB serial without automatic retries."""

import argparse
import math
import re
import time


def invoke(port, command, timeout=15.0):
    """Return console output; transmission logs do not confirm appliance response.

    The serial connection suppresses DTR/RTS to avoid an intentional reset. Some
    USB adapters may still toggle modem lines when opened. A timeout never causes
    a second transmission because repeating a toggle could reverse the action.
    """
    if (not isinstance(command, str) or not command.strip() or len(command) > 255
            or any(not 32 <= ord(character) <= 126 for character in command)):
        raise ValueError("one printable ASCII command of 1..255 characters is required")
    if (type(timeout) not in (int, float) or not math.isfinite(timeout)
            or not 1 <= timeout <= 120):
        raise ValueError("timeout must be 1..120 seconds")

    import serial

    connection = serial.Serial(port=None, baudrate=115200, timeout=0.1,
                               write_timeout=timeout)
    connection.dtr = False
    connection.rts = False
    connection.port = port
    with connection:
        time.sleep(0.2)
        connection.reset_input_buffer()
        request = (command + "\n").encode("ascii")
        if connection.write(request) != len(request):
            raise OSError("Incomplete serial write; command was not retried")
        connection.flush()
        deadline = time.monotonic() + timeout
        output = bytearray()
        while time.monotonic() < deadline:
            output.extend(connection.read(connection.in_waiting or 1))
        return output.decode("utf-8", errors="replace")


def send_saved(port, name, zone=0):
    """Call a previously confirmed saved waveform by name, once."""
    if (not isinstance(name, str) or not re.fullmatch(r"[a-z0-9_]{1,15}", name)
            or type(zone) is not int or zone not in (0, 1, 2)):
        raise ValueError("invalid command name or zone")
    return invoke(port, f"send {name} {zone}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="COM port or /dev/ttyACM device")
    parser.add_argument("--timeout", type=float, default=15)
    parser.add_argument("command", nargs=argparse.REMAINDER, help='Example: send fan_low 0')
    args = parser.parse_args()
    if not 1 <= args.timeout <= 120:
        parser.error("timeout must be 1..120 seconds")
    if not args.command:
        parser.error("a console command is required")
    print(invoke(args.port, " ".join(args.command), args.timeout), end="")


if __name__ == "__main__":
    main()
