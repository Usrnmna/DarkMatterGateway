"""Configuration contracts protect disabled placeholders and absolute command mappings."""

import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from profile_config import generate_header, load_config, validate_config


class ProfileTests(unittest.TestCase):
    def setUp(self):
        self.config = json.loads((ROOT / "config/devices.json").read_text())

    def test_default_placeholders_remain_disabled(self):
        self.assertTrue(all(not p["enabled"] for p in validate_config(self.config)))
        self.assertIn("GasFireplace", generate_header(self.config))
        self.assertIn('"portable_ac", "cool_preset", "ac_cool"', generate_header(self.config))

    def test_enabling_requires_model_and_verified_transport(self):
        for index in range(len(self.config["devices"])):
            data = copy.deepcopy(self.config)
            data["devices"][index]["enabled"] = True
            with self.assertRaises(ValueError):
                validate_config(data)

    def test_valid_native_profile(self):
        device = self.config["devices"][0]
        device.update(enabled=True, model="Bench fixture", transport_verified_ir=True,
                      discrete_commands_verified=True)
        self.assertTrue(validate_config(self.config)[0]["enabled"])
        device["commands"]["off"] = device["commands"]["on"]
        with self.assertRaises(ValueError):
            validate_config(self.config)

    def test_invalid_zone_and_duplicate_ids(self):
        self.config["devices"][0]["zone"] = True
        with self.assertRaises(ValueError):
            validate_config(self.config)
        self.config["devices"][0]["zone"] = 0
        self.config["devices"][1]["id"] = self.config["devices"][0]["id"]
        with self.assertRaises(ValueError):
            validate_config(self.config)

    def test_unknown_fields_fail_instead_of_silent_misconfiguration(self):
        self.config["devices"][0]["enabeld"] = True
        with self.assertRaises(ValueError):
            validate_config(self.config)

    def test_fireplace_manual_activation_requires_permission_metadata(self):
        fireplace = self.config["devices"][2]
        fireplace.update(enabled=True, model="Unqualified fixture", transport_verified_ir=True)
        with self.assertRaises(ValueError):
            validate_config(self.config)
        fireplace["remote_operation_permitted"] = True
        self.assertEqual(validate_config(self.config)[2]["kind"], "gas_fireplace")

    def test_malformed_kind_and_action_are_configuration_errors(self):
        for kind in ([], {}, None, 1):
            with self.subTest(kind=kind):
                data = copy.deepcopy(self.config)
                data["devices"][0]["kind"] = kind
                with self.assertRaises(ValueError):
                    validate_config(data)
        self.config["devices"][0]["commands"] = {1: "light_on"}
        with self.assertRaises(ValueError):
            validate_config(self.config)

    def test_json_repeated_keys_and_nonfinite_numbers_are_rejected(self):
        documents = (
            '{"schema_version": 1, "schema_version": 2, "devices": []}',
            '{"devices": [{"enabled": false, "enabled": true}]}',
            '{"temperature_min": NaN}',
            '{"temperature_max": Infinity}',
        )
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "devices.json"
            for document in documents:
                with self.subTest(document=document):
                    path.write_text(document, encoding="utf-8")
                    with self.assertRaises(ValueError):
                        load_config(path)
            path.write_text(json.dumps(self.config), encoding="utf-8")
            self.assertEqual(load_config(path), self.config)


if __name__ == "__main__":
    unittest.main()
