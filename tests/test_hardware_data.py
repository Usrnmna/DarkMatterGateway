"""Check hardware procurement and wiring data for internally inconsistent records."""

import csv
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def rows(name):
    with (ROOT / "hardware" / name).open(encoding="utf-8-sig", newline="") as source:
        return list(csv.DictReader(source))


class HardwareDataTests(unittest.TestCase):
    def test_bom_references_are_unique_and_quantities_match(self):
        seen = set()
        for row in rows("bom.csv"):
            references = row["references"].split()
            self.assertEqual(len(references), int(row["quantity"]), row)
            for reference in references:
                self.assertNotIn(reference, seen)
                seen.add(reference)

    def test_application_gpio_contract_matches_wiring(self):
        wiring = {row["net"]: int(row["gpio"]) for row in rows("gpio-map.csv")
                  if row["gpio"].isdigit()}
        expected = {"IR_TX_ARRAY": 4, "IR_TX_EXT1": 5, "IR_TX_EXT2": 6,
                    "IR_RX_38": 8, "IR_RX_56": 9, "IR_RX_CARRIER": 10,
                    "TX_OE_N": 18, "COMMISSION_N": 14}
        for net, pin in expected.items():
            self.assertEqual(wiring[net], pin)
        header = (ROOT / "firmware/main/board_config.hpp").read_text()
        for name, pin in (("kReceive38", 8), ("kReceive56", 9), ("kLearnCarrier", 10),
                          ("kTransmitDisable", 18), ("kCommissionButton", 14)):
            self.assertRegex(header, rf"{name}\s*=\s*GPIO_NUM_{pin}\s*;")

    def test_current_limited_switch_matches_component_pin_contract(self):
        # TPS2553 DBV pin 4 is FAULT and pin 5 is ILIM. The ILIM resistor
        # determines current protection; FAULT requires the specified pull-up.
        bom = {reference: row for row in rows("bom.csv")
               for reference in row["references"].split()}
        self.assertEqual(bom["U5"]["manufacturer_part_number"], "TPS2553DBVR")
        self.assertTrue(bom["R6"]["value_or_rating"].startswith("21.5kOhm1percent"))
        self.assertNotIn("C6", bom)
        connections = rows("connections.csv")
        switch = {row["terminal"]: row["net_or_connection"]
                  for row in connections if row["reference"] == "U5"}
        self.assertEqual(switch, {"1 IN": "VBUS_FUSED", "2 GND": "GND",
                                 "3 EN": "MAIN_ON", "4 FAULT": "MAIN_FAULT_N",
                                 "5 ILIM": "R6 toGND", "6 OUT": "+5V_MAIN"})
        bleed = next(row for row in connections if row["reference"] == "R42")
        self.assertEqual(bleed["net_or_connection"], "+5V_MAIN/GND")
        fault = next(row for row in connections
                     if row["reference"] == "U5" and row["terminal"] == "4 FAULT")
        self.assertIn("R43 100k to3V3_AON", fault["notes"])

    def test_x0a_hardware_assignments_remain_unqualified(self):
        header = (ROOT / "firmware/main/board_config.hpp").read_text()
        self.assertRegex(header, r"kX0aHardwareQualified\s*=\s*false\s*;")
        for name in ("kX0aUart", "kX0aTx", "kX0aRx", "kX0aTxInhibit", "kX0aInhibitLevel"):
            self.assertRegex(header, rf"{name}\s*=\s*-1\s*;")

    def test_ir_storage_has_separate_aligned_nonoverlapping_partition(self):
        partitions = []
        with (ROOT / "firmware/partitions.csv").open() as source:
            for row in csv.reader(line for line in source if not line.startswith("#")):
                if not row:
                    continue
                name, kind, subtype, offset, size = row[:5]
                start, length = int(offset, 0), int(size, 0)
                self.assertEqual(start % (0x10000 if kind == "app" else 0x1000), 0)
                self.assertLessEqual(start + length, 16 * 1024 * 1024)
                partitions.append((start, start + length, name))
        for left, right in zip(sorted(partitions), sorted(partitions)[1:]):
            self.assertLessEqual(left[1], right[0])
        self.assertIn("ir_store", {row[2] for row in partitions})


if __name__ == "__main__":
    unittest.main()
