"""Synthetic build fixtures exercise local packaging; no device credentials exist."""

import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import export_artifacts as exporter
from profile_config import generate_header


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


def image_bytes(payload=b"SYNTHETIC TEST IMAGE"):
    """Construct an ESP32-S3 image with one segment, checksum, and appended hash."""
    header = bytearray(24)
    header[0:4] = bytes((0xE9, 1, 2, 0x4F))
    header[12] = 9
    header[23] = 1
    image = header + struct.pack("<II", 0x3C000020, len(payload)) + payload
    checksum = 0xEF
    for value in payload:
        checksum ^= value
    image += bytes(15 - len(image) % 16) + bytes((checksum,))
    return bytes(image) + hashlib.sha256(image).digest()


def partition_bytes():
    """Literal approved layout, independent of exporter constants."""
    data = b""
    for name, kind, subtype, offset, size in (
        ("nvs", 1, 2, 0x10000, 0x10000), ("phy_init", 1, 1, 0x20000, 0x1000),
        ("otadata", 1, 0, 0x21000, 0x2000), ("ir_store", 1, 2, 0x30000, 0x100000),
        ("ota_0", 0, 0x10, 0x130000, 0x600000), ("ota_1", 0, 0x11, 0x730000, 0x600000),
    ):
        data += struct.pack("<HBBII16sI", 0x50AA, kind, subtype, offset, size,
                            name.encode(), 0)
    data += b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(data).digest()
    return data.ljust(3072, b"\xff")


class ExportTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build").mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="export-test-", dir=ROOT / "build")
        self.addCleanup(self.temporary.cleanup)
        self.source = Path(self.temporary.name) / "source"
        self.source.mkdir()
        # The synthetic build must not inherit a developer's activated SDK path.
        environment = patch.dict("os.environ", {"IDF_PATH": str(self.source / "synthetic-sdk")})
        environment.start()
        self.addCleanup(environment.stop)
        for name in ("firmware/CMakeLists.txt", "firmware/main/CMakeLists.txt",
                     "firmware/main/app_main.cpp", "firmware/sdkconfig.defaults",
                     "firmware/dependencies.lock", "tools/profile_config.py",
                     "tools/export_artifacts.py"):
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("# synthetic source\n")
        (self.source / "firmware/partitions.csv").write_bytes(
            (ROOT / "firmware/partitions.csv").read_bytes())
        self.profile = self.source / "config/devices.json"
        write_json(self.profile, json.loads((ROOT / "config/devices.json").read_text()))
        self.build_dir = self.source / "firmware/build-export"
        self.output = Path(self.temporary.name) / "package"
        self.sdks = {"esp-idf": "1" * 40, "esp-matter": "2" * 40,
                     "connectedhomeip": "3" * 40}

    def fake_build(self, *args, **kwargs):
        """Emit SDK-shaped metadata without invoking a compiler or SDK."""
        directory = self.build_dir
        directory.mkdir(parents=True, exist_ok=True)
        files = {"0x0": "bootloader/bootloader.bin", "0xc000": "partition_table/partition-table.bin",
                 "0x21000": "ota_data_initial.bin", "0x130000": "matter_ir_terminal.bin"}
        for name in files.values():
            path = directory / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(image_bytes())
        (directory / "partition_table/partition-table.bin").write_bytes(partition_bytes())
        (directory / "ota_data_initial.bin").write_bytes(b"\xff" * 8192)
        flags = ["--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "16MB"]
        metadata = {"write_flash_args": flags,
                    "flash_settings": {"flash_mode": "dio", "flash_freq": "80m", "flash_size": "16MB"},
                    "flash_files": files,
                    "extra_esptool_args": {"after": "hard_reset", "before": "default_reset",
                                           "stub": True, "chip": "esp32s3"}}
        for role, address in (("bootloader", "0x0"), ("partition-table", "0xc000"),
                              ("otadata", "0x21000"), ("app", "0x130000")):
            metadata[role] = {"offset": address, "file": files[address], "encrypted": "false"}
        write_json(directory / "flasher_args.json", metadata)
        (directory / "flash_args").write_text(" ".join(flags) + "\n" +
            "".join(f"{offset} {name}\n" for offset, name in files.items()))
        write_json(directory / "project_description.json", {
            "project_name": "matter_ir_terminal", "target": "esp32s3",
            "project_path": str(self.source / "firmware"), "build_dir": str(directory),
            "config_file": str(directory / "sdkconfig"), "app_bin": "matter_ir_terminal.bin",
            "idf_path": str(self.source / "synthetic-sdk"), "git_revision": "v5.4.1"})
        (directory / "sdkconfig").write_text(
            'CONFIG_IDF_TARGET="esp32s3"\nCONFIG_ESPTOOLPY_FLASHSIZE="16MB"\n'
            'CONFIG_ESPTOOLPY_FLASHMODE="dio"\nCONFIG_ESPTOOLPY_FLASHFREQ="80m"\n'
            'CONFIG_PARTITION_TABLE_OFFSET=0xC000\nCONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"\n'
            'CONFIG_TEST_PRIVATE_SETTING="DO-NOT-EXPORT-SYNTHETIC"\n')
        (directory / "CMakeCache.txt").write_text(
            f"PROFILE_CONFIG:FILEPATH={self.profile}\nCMAKE_HOME_DIRECTORY:INTERNAL={self.source / 'firmware'}\n")
        path = directory / "esp-idf/main/generated/profiles.hpp"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(generate_header(json.loads(self.profile.read_text())))

    def built(self, action=None):
        """Use the real build orchestration with only the external SDK calls replaced."""
        while self.build_dir.exists():
            self.build_dir = self.build_dir.with_name(self.build_dir.name + "-new")
        with patch.object(exporter, "sdk_identity", return_value=self.sdks), \
             patch.object(exporter, "run_build", side_effect=action or self.fake_build):
            exporter.build(self.source, self.build_dir, self.profile)

    def export(self, destination=None):
        with patch.object(exporter, "sdk_identity", return_value=self.sdks):
            return exporter.package(self.source, self.build_dir, self.profile,
                                    destination or self.output)

    def test_exports_only_allowed_files_with_relative_paths(self):
        self.built()
        for name in ("private.key", "captures.json", "sdkconfig.old", "firmware.elf"):
            (self.build_dir / name).write_text("DO-NOT-EXPORT-SYNTHETIC")
        self.export()
        manifest = exporter.verify(self.output)
        self.assertEqual(manifest["scope"], "local-development")
        files = {p.relative_to(self.output).as_posix() for p in self.output.rglob("*") if p.is_file()}
        self.assertEqual(files, {"bootloader/bootloader.bin", "partition_table/partition-table.bin",
            "ota_data_initial.bin", "matter_ir_terminal.bin", "flash_args", "flasher_args.json",
            "build-config.json", "manifest.json", "SHA256SUMS.txt", "README.txt"})
        content = b"".join(p.read_bytes() for p in self.output.rglob("*") if p.is_file())
        self.assertNotIn(b"DO-NOT-EXPORT-SYNTHETIC", content)
        self.assertNotIn(str(self.source).encode(), content)
        self.assertEqual(manifest["sdks"], self.sdks)
        self.assertEqual(manifest["artifacts"]["matter_ir_terminal.bin"]["size"], len(image_bytes()))

    def test_same_build_packages_are_byte_identical(self):
        self.built()
        self.export()
        other = self.output.with_name("second")
        self.export(other)
        first = {p.relative_to(self.output): p.read_bytes() for p in self.output.rglob("*") if p.is_file()}
        self.assertEqual(first, {p.relative_to(other): p.read_bytes() for p in other.rglob("*") if p.is_file()})

    def test_selected_enabled_fixture_is_recorded(self):
        config = json.loads(self.profile.read_text())
        config["devices"][0].update(enabled=True, model="SYNTHETIC", transport_verified_ir=True,
                                     discrete_commands_verified=True)
        write_json(self.profile, config)
        self.built()
        self.export()
        summary = json.loads((self.output / "build-config.json").read_text())
        self.assertEqual(summary["profile"]["enabled_kinds"], ["light"])
        self.assertEqual(summary["profile"]["sha256"], hashlib.sha256(self.profile.read_bytes()).hexdigest())

    def test_failed_build_does_not_leave_receipt(self):
        def fail(*args):
            self.fake_build()
            raise exporter.ExportError("synthetic compiler failure")
        with self.assertRaisesRegex(exporter.ExportError, "compiler failure"):
            self.built(fail)
        self.assertFalse((self.build_dir / "artifact-build.json").exists())
        self.assertFalse(self.output.exists())

    def test_changed_source_during_build_is_rejected(self):
        def change(*args):
            self.fake_build()
            (self.source / "firmware/main/app_main.cpp").write_text("changed during build")
        with self.assertRaisesRegex(exporter.ExportError, "changed"):
            self.built(change)
        self.assertFalse((self.build_dir / "artifact-build.json").exists())

    def test_unrecorded_and_stale_builds_are_rejected(self):
        self.fake_build()
        with self.assertRaises(exporter.ExportError):
            self.export()
        self.built()
        (self.source / "firmware/main/app_main.cpp").write_text("modified")
        with self.assertRaisesRegex(exporter.ExportError, "changed|stale"):
            self.export()
        self.assertFalse(self.output.exists())

    def test_changed_sdk_is_rejected(self):
        self.built()
        self.sdks["esp-idf"] = "4" * 40
        with self.assertRaisesRegex(exporter.ExportError, "changed|stale"):
            self.export()

    def test_changed_build_file_is_rejected(self):
        self.built()
        (self.build_dir / "matter_ir_terminal.bin").write_bytes(image_bytes(b"OTHER"))
        with self.assertRaisesRegex(exporter.ExportError, "changed|stale"):
            self.export()

    def test_existing_output_is_never_overwritten(self):
        self.built()
        self.output.mkdir()
        (self.output / "retain.txt").write_text("retain")
        with self.assertRaises(exporter.ExportError):
            self.export()
        self.assertEqual((self.output / "retain.txt").read_text(), "retain")

    def test_build_refuses_existing_directory(self):
        self.fake_build()
        with patch.object(exporter, "run_build") as runner:
            with self.assertRaisesRegex(exporter.ExportError, "new directory"):
                exporter.build(self.source, self.build_dir, self.profile)
            runner.assert_not_called()

    def assert_bad_build(self, mutate):
        def invalid(*args):
            self.fake_build()
            mutate()
        with self.assertRaises(exporter.ExportError):
            self.built(invalid)
        self.assertFalse((self.build_dir / "artifact-build.json").exists())

    def change_json(self, filename, mutate):
        path = self.build_dir / filename
        data = json.loads(path.read_text())
        mutate(data)
        write_json(path, data)

    def test_unsafe_flash_paths_and_additional_write_targets(self):
        for name in ("../private.key", "/tmp/private.key", "C:/private.key", "bootloader\\bootloader.bin"):
            with self.subTest(name=name):
                self.assert_bad_build(lambda: self.change_json("flasher_args.json",
                    lambda d: d["flash_files"].update({"0x0": name})))
        self.assert_bad_build(lambda: self.change_json("flasher_args.json",
            lambda d: d["flash_files"].update({"0x30000": "private.key"})))

    def test_wrong_target_role_encryption_and_flash_flags(self):
        for mutation in (
            lambda d: d["extra_esptool_args"].update(chip="esp32"),
            lambda d: d["app"].update(offset="0x730000"),
            lambda d: d["app"].update(encrypted="true"),
            lambda d: d.update(write_flash_args=["--erase-all"]),
        ):
            with self.subTest(mutation=mutation):
                self.assert_bad_build(lambda: self.change_json("flasher_args.json", mutation))

    def test_mismatched_flash_args_and_generated_profile(self):
        self.assert_bad_build(lambda: (self.build_dir / "flash_args").write_text("--erase-all\n"))
        self.assert_bad_build(lambda: (self.build_dir / "esp-idf/main/generated/profiles.hpp").write_text("stale"))

    def test_partition_corruption_and_image_limit(self):
        self.assert_bad_build(lambda: (self.build_dir / "partition_table/partition-table.bin").write_bytes(
            b"\x00" + partition_bytes()[1:]))
        self.assert_bad_build(lambda: (self.build_dir / "matter_ir_terminal.bin").write_bytes(
            image_bytes(b"x" * 0x600000)))

    def test_corrupted_image_checksum_and_ota_data(self):
        data = bytearray(image_bytes())
        data[32] ^= 1
        self.assert_bad_build(lambda: (self.build_dir / "matter_ir_terminal.bin").write_bytes(data))
        self.assert_bad_build(lambda: (self.build_dir / "ota_data_initial.bin").write_bytes(b"\0" * 8192))

    def test_duplicate_metadata_keys_and_missing_payload(self):
        self.assert_bad_build(lambda: (self.build_dir / "flasher_args.json").write_text('{"app":{},"app":{}}'))
        self.assert_bad_build(lambda: (self.build_dir / "bootloader/bootloader.bin").unlink())

    def test_package_tampering_and_extra_files(self):
        self.built()
        self.export()
        (self.output / "private.key").write_text("synthetic")
        with self.assertRaises(exporter.ExportError):
            exporter.verify(self.output)
        (self.output / "private.key").unlink()
        (self.output / "matter_ir_terminal.bin").write_bytes(image_bytes(b"different"))
        with self.assertRaises(exporter.ExportError):
            exporter.verify(self.output)

    def test_inconsistent_provenance_is_rejected_even_with_updated_checksums(self):
        self.built()
        for index, mutate in enumerate((
            lambda m: m["source"].update(sha256="0" * 64),
            lambda m: m["sdks"].update({"esp-idf": "not-a-revision"}),
        )):
            with self.subTest(index=index):
                destination = self.output.with_name(f"inconsistent-{index}")
                self.export(destination)
                path = destination / "manifest.json"
                manifest = json.loads(path.read_text())
                mutate(manifest)
                write_json(path, manifest)
                lines = []
                for item in sorted(destination.rglob("*")):
                    if item.is_file() and item.name != "SHA256SUMS.txt":
                        name = item.relative_to(destination).as_posix()
                        lines.append(f"{hashlib.sha256(item.read_bytes()).hexdigest()}  {name}\n")
                (destination / "SHA256SUMS.txt").write_text("".join(lines))
                with self.assertRaises(exporter.ExportError):
                    exporter.verify(destination)

    def test_symbolic_link_payload_is_rejected(self):
        self.fake_build()
        link = self.build_dir / "linked.bin"
        try:
            link.symlink_to(self.build_dir / "matter_ir_terminal.bin")
        except OSError:
            self.skipTest("symbolic links unavailable on this host")
        def linked(*args):
            path = self.build_dir / "matter_ir_terminal.bin"
            path.unlink()
            path.symlink_to(link)
        self.assert_bad_build(linked)


if __name__ == "__main__":
    unittest.main()
