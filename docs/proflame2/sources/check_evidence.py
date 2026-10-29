"""Reproduce published-data arithmetic locally; no device or network access.

Python 3.10 or later is required. Archived upstream code is never imported.
Results are printed as JSON; this script does not modify files.
"""

from collections import defaultdict
import csv
import json
from pathlib import Path


SOURCE_DIRECTORY = Path(__file__).resolve().parent
PACKET_COLUMNS = (
    "serial1", "serial2", "version", "cmd1", "cmd2", "checksum1", "checksum2"
)

# Published proposal from rtl_433 issue 1905, comment 2944061060.
# This matrix is retained to reproduce its failure, not as a working encoder.
PROPOSED_MASKS = (
    0x800000C000, 0x1080006200, 0x7000003100, 0xC080001000,
    0x2000008000, 0xC080004000, 0x9000002200, 0x6000001100,
    0x90800000C4, 0x0000000062, 0xD000000031, 0x1000000010,
    0x2000000088, 0x2080000044, 0x9080000022, 0xE000000011,
)

# Selected documented message examples from Bonaparte System Information.
UART_EXAMPLES = (
    "5e0000001648", "ae00000016b8", "5dc1a000162a", "adc1a00016da",
    "ad00000016bb", "5e0000001749", "ae00610017d8", "5e0000001846",
    "ae00000018b6", "5d000100174b", "ad00010017bb", "5d00800018c5",
    "ad0080001835",
)


def mix_command(command):
    """Evaluate the reported command-byte relation without upstream code."""
    upper = command >> 4
    lower = command & 15
    return ((command & 240) ^ lower ^ (lower << 5) ^ upper ^ (upper << 5)) & 255


def xor_bytes(values):
    """Return the XOR of an iterable of byte values."""
    checksum = 0
    for value in values:
        checksum ^= value
    return checksum


def analyze():
    """Compare independent arithmetic with the retained public packet evidence.

    One row per identity calibrates its constants. Every remaining row is held
    out from that calibration. No conclusion about new physical hardware follows.
    """
    with (SOURCE_DIRECTORY / "community/aetf/cmd.csv").open(encoding="utf-8") as source:
        rows = list(csv.DictReader(source))
    groups = defaultdict(list)
    for row in rows:
        packet = [int(row[column], 16) for column in PACKET_COLUMNS]
        groups[tuple(packet[:3])].append(packet)

    failures = []
    keys = {}
    for identity, packets in groups.items():
        first = packets[0]
        key1 = mix_command(first[3]) ^ first[5]
        key2 = mix_command(first[4]) ^ first[6]
        keys[bytes(identity).hex()] = [f"{key1:02x}", f"{key2:02x}"]
        for packet in packets[1:]:
            expected = [mix_command(packet[3]) ^ key1, mix_command(packet[4]) ^ key2]
            if expected != packet[5:]:
                failures.append(bytes(packet).hex())

    challenge = bytes.fromhex("4b 52 02 01 06")
    value = int.from_bytes(challenge, "big")
    prediction = 0
    for mask in PROPOSED_MASKS:
        prediction = (prediction << 1) | ((value & mask).bit_count() & 1)

    return {
        "access_date_pacific": "2026-10-28",
        "method": "Offline independent arithmetic over published data only; no upstream code executed.",
        "csv_rows": len(rows),
        "identities": len(groups),
        "calibration_rows": len(groups),
        "held_out_rows": len(rows) - len(groups),
        "held_out_checksum_bytes": 2 * (len(rows) - len(groups)),
        "checksum_mismatches": failures,
        "derived_keys_hex": keys,
        "matrix_counterexample": {
            "data": challenge.hex(),
            "predicted": f"{prediction:04x}",
            "reported_observed": "1de5",
            "xor_difference": f"{prediction ^ 0x1de5:04x}",
        },
        "uart_examples": len(UART_EXAMPLES),
        "uart_xor_failures": [frame for frame in UART_EXAMPLES if xor_bytes(bytes.fromhex(frame))],
        "rf_complete_symbols": 7 * 26,
        "duration_us_at_450": 7 * 26 * 450,
        "aetf_final_mark_duration_us_at_450": (7 * 26 - 1) * 450,
        "rf_sample_51": {"power": 0x51 & 1, "thermostat": (0x51 >> 1) & 1},
    }


if __name__ == "__main__":
    print(json.dumps(analyze(), indent=2))
