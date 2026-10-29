"""Configuration rejection tests; synthetic values are not deployable profiles."""
import copy
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import profile_config
import proflame2_config as config


class ProflameConfigTests(unittest.TestCase):
    def setUp(self):
        self.data = profile_config.load_config(ROOT / "config/proflame2.json")
        self.ir = profile_config.validate_config(profile_config.load_config(ROOT / "config/devices.json"))

    def test_default_is_disabled_and_unqualified(self):
        config.validate_config(self.data, self.ir)
        header = config.generate_header(self.data, self.ir)
        self.assertIn("kEnabled = false", header)
        self.assertIn("kQualifiedForControl = false", header)
        self.assertIn("kTimings{0, 0, 0, 0, 0}", header)
        self.assertEqual(config.QUALIFIED_PROFILES, {})

    def test_boolean_cannot_qualify_an_unmeasured_target(self):
        for key in ("enabled", "qualified_for_control"):
            with self.subTest(key=key):
                data = copy.deepcopy(self.data)
                data[key] = True
                with self.assertRaises(ValueError):
                    config.validate_config(data, self.ir)
        data = copy.deepcopy(self.data)
        data.update(enabled=True, qualified_for_control=True,
                    qualification_record="synthetic-not-a-record", fields=["power"])
        data["identity"] = {key: "synthetic" for key in data["identity"]}
        data["timings"] = dict(response_timeout_ms=100, inter_request_ms=10,
                              resync_quiet_ms=200, snapshot_max_age_ms=2000, inter_byte_ms=20)
        with self.assertRaisesRegex(ValueError, "reviewed"):
            config.validate_config(data, self.ir)

    def test_strict_keys_types_and_capabilities(self):
        for key, value in (("schema_version", True), ("enabled", 1), ("device_id", "bad name"),
                           ("qualified_for_control", "false"), ("qualification_record", 3),
                           ("fields", ["pilot"]), ("fields", ["thermostat"]),
                           ("fields", ["power", "power"]), ("fields", "power"),
                           ("fields", [{}]), ("identity", {}), ("timings", {}), ("extra", 1)):
            with self.subTest(key=key, value=value):
                data = copy.deepcopy(self.data)
                data[key] = value
                with self.assertRaises(ValueError):
                    config.validate_config(data, self.ir)
        for value in (True, 0, -1, 1.1, "100", 1000000):
            data = copy.deepcopy(self.data)
            data["timings"]["response_timeout_ms"] = value
            with self.subTest(value=value), self.assertRaises(ValueError):
                config.validate_config(data, self.ir)
        data = copy.deepcopy(self.data)
        data["identity"]["ifc"] = 'bad\nidentifier'
        with self.assertRaises(ValueError):
            config.validate_config(data, self.ir)

    def test_ir_id_conflict_and_duplicate_json(self):
        self.ir[2]["enabled"] = True
        with self.assertRaisesRegex(ValueError, "IR"):
            config.validate_config(self.data, self.ir)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "duplicate.json"
            path.write_text('{"enabled":false,"enabled":true}', encoding="utf-8")
            with self.assertRaises(ValueError):
                config.load_config(path)

    def test_unknowns_cannot_escape_into_generated_code(self):
        self.data["identity"]["ifc"] = 'quoted " board'
        header = config.generate_header(self.data, self.ir)
        self.assertNotIn('quoted " board', header)
        self.assertIn("kEnabled = false", header)


if __name__ == "__main__":
    unittest.main()
