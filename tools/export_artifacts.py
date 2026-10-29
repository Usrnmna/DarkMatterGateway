"""Build, export, and verify local ESP32-S3 development artifact packages.

Only the existing 16 MB flash layout is supported. This tool never flashes a
device. Receipts bind successful local builds to inputs; they are not signatures.
"""

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import stat
import struct
import subprocess
import sys
import tempfile

from profile_config import KINDS, generate_header, load_config, validate_config


# Deliberate allowlists: changes require review against the firmware contract.
LAYOUT = (
    ("nvs", 1, 2, 0x10000, 0x10000), ("phy_init", 1, 1, 0x20000, 0x1000),
    ("otadata", 1, 0, 0x21000, 0x2000), ("ir_store", 1, 2, 0x30000, 0x100000),
    ("ota_0", 0, 0x10, 0x130000, 0x600000), ("ota_1", 0, 0x11, 0x730000, 0x600000),
)
ROLES = {
    "bootloader": (0x0, "bootloader/bootloader.bin", 0xC000),
    "partition-table": (0xC000, "partition_table/partition-table.bin", 0x1000),
    "otadata": (0x21000, "ota_data_initial.bin", 0x2000),
    "app": (0x130000, "matter_ir_terminal.bin", 0x600000),
}
SETTINGS = {"flash_mode": "dio", "flash_freq": "80m", "flash_size": "16MB"}
EXTRA_ARGS = {"after": "hard_reset", "before": "default_reset", "stub": True, "chip": "esp32s3"}
CONFIG_VALUES = {
    "CONFIG_IDF_TARGET": '"esp32s3"', "CONFIG_ESPTOOLPY_FLASHSIZE": '"16MB"',
    "CONFIG_ESPTOOLPY_FLASHMODE": '"dio"', "CONFIG_ESPTOOLPY_FLASHFREQ": '"80m"',
    "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME": '"partitions.csv"',
}
RECEIPT = "artifact-build.json"
METADATA_FILES = ("project_description.json", "CMakeCache.txt", "sdkconfig", "flash_args",
                  "flasher_args.json", "esp-idf/main/generated/profiles.hpp")
PAYLOADS = {value[1] for value in ROLES.values()}
PACKAGE_FILES = PAYLOADS | {"flash_args", "flasher_args.json", "build-config.json",
                            "manifest.json", "SHA256SUMS.txt", "README.txt"}
USAGE = """Matter-IR-Terminal local development package

Verify before use, from a maintained source copy:
  python tools/export_artifacts.py verify /path/to/this/package
Or, from this package directory on Linux:
  sha256sum --check SHA256SUMS.txt

Flash paths are relative to this directory. After separately completing hardware
bring-up and authorizing installation, the pinned ESP-IDF environment can use:
  python -m esptool --chip esp32s3 --port <ACTUAL_PORT> write_flash @flash_args
No command has been executed on a device by the exporter.

The four images write the bootloader, partition table, OTA initialization data,
and ota_0 application. Reinitializing OTA metadata selects the packaged baseline;
this is an initial/local installation set, not an OTA update or recovery system.
No NVS or ir_store image is included. No erase-all operation is included.

Development identities may be embedded by the SDK. This package is not production
provisioning, a signed release, or distribution clearance. Raw configuration,
private keys, captured commands, ELF/map files, SDK caches, and logs are excluded.
Compiled firmware can still contain selected configuration; inspect inputs before
sharing. Checksums detect accidental changes, not an attacker replacing the set.
Identical package bytes from identical build outputs do not prove reproducible
firmware compilation. Hardware, commissioning, and appliance behavior are untested.
"""


class ExportError(ValueError):
    """An input cannot establish a safe, consistent local package."""


def require(condition, message):
    """Raise a predictable validation error without echoing input contents."""
    if not condition:
        raise ExportError(message)


def canonical(value):
    """Serialize portable metadata without timestamps or host-specific whitespace."""
    return (json.dumps(value, indent=2, sort_keys=True, allow_nan=False) + "\n").encode()


def digest(data):
    return hashlib.sha256(data).hexdigest()


def plain_path(path):
    """Reject symbolic links and Windows junctions before following any input path."""
    path = Path(path).absolute()
    for part in (path, *path.parents):
        reparse = (getattr(part.lstat(), "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT
                   if part.exists() else 0)
        require(not part.is_symlink() and not reparse,
                "Linked input or output paths are not supported")
    return path


def read_file(root, name):
    """Read a regular file under root using a canonical package-relative name."""
    require(isinstance(name, str) and re.fullmatch(r"[A-Za-z0-9_./-]+", name)
            and not name.startswith("/") and PurePosixPath(name).as_posix() == name
            and all(part not in (".", "..") for part in name.split("/")), "Unsafe relative path")
    path = plain_path(Path(root) / name)
    require(path.is_file(), "Required file is missing or is not a regular file")
    return path.read_bytes()


def read_json(root, name):
    """Reject duplicate keys and non-finite constants instead of silently normalizing."""
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, "Duplicate JSON key")
            result[key] = value
        return result
    try:
        data = json.loads(read_file(root, name), object_pairs_hook=pairs,
                          parse_constant=lambda _: require(False, "Non-finite JSON value"))
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise ExportError("Invalid JSON metadata") from exc
    require(isinstance(data, dict), "Metadata must be a JSON object")
    return data


def sdk_identity():
    """Record local SDK HEADs; reject tracked edits, including dirty submodules."""
    require(os.environ.get("IDF_PATH") and os.environ.get("ESP_MATTER_PATH"),
            "Activate the pinned SDK environment first")
    matter = Path(os.environ["ESP_MATTER_PATH"])
    roots = {"esp-idf": Path(os.environ["IDF_PATH"]), "esp-matter": matter,
             "connectedhomeip": matter / "connectedhomeip/connectedhomeip"}
    result = {}
    for name, root in roots.items():
        def git(*args):
            process = subprocess.run(["git", "-C", str(root), *args], capture_output=True, text=True)
            require(process.returncode == 0, "Cannot inspect the selected SDK checkout")
            return process.stdout.strip()
        revision = git("rev-parse", "HEAD")
        require(re.fullmatch(r"[0-9a-f]{40}", revision), "Invalid SDK revision")
        require(not git("status", "--porcelain", "--untracked-files=no", "--ignore-submodules=untracked"),
                "Tracked SDK modifications prevent an unambiguous SDK identity")
        result[name] = revision
    return result


def source_inputs(source, profile):
    """Hash maintained build inputs and selected configuration without exporting contents.

    The optional X0A configuration is recorded only when already present in the
    workspace; packaging does not enable it or change its implementation.
    """
    names = {"firmware/CMakeLists.txt", "firmware/sdkconfig.defaults", "firmware/partitions.csv",
             "firmware/dependencies.lock", "tools/profile_config.py", "tools/export_artifacts.py"}
    for directory in ("firmware/main", "firmware/components"):
        for path in (source / directory).rglob("*"):
            plain_path(path)
            if path.is_file():
                names.add(path.relative_to(source).as_posix())
    for name in ("tools/proflame2_config.py", "config/proflame2.json"):
        if (source / name).exists():
            names.add(name)
    profile = plain_path(profile)
    require(profile.is_file(), "Selected profile is missing")
    files = {name: digest(read_file(source, name)) for name in sorted(names)}
    return {"files": files, "profile_sha256": digest(profile.read_bytes()), "sdks": sdk_identity()}


def check_layout(data):
    """Decode and verify the IDF partition table, including its MD5 terminator."""
    require(len(data) == 3072, "Unexpected partition table size")
    for index, expected in enumerate(LAYOUT):
        entry = data[index * 32:(index + 1) * 32]
        magic, kind, subtype, offset, size, label, flags = struct.unpack("<HBBII16sI", entry)
        expected_name, *numbers = expected
        require(magic == 0x50AA and (kind, subtype, offset, size) == tuple(numbers)
                and label == expected_name.encode().ljust(16, b"\0") and flags == 0,
                "Partition layout differs from the approved local export layout")
    end = len(LAYOUT) * 32
    require(data[end:end + 16] == b"\xeb\xeb" + b"\xff" * 14
            and data[end + 16:end + 32] == hashlib.md5(data[:end]).digest()
            and data[end + 32:] == b"\xff" * (len(data) - end - 32),
            "Invalid partition table digest or trailing entries")


def check_source_layout(source):
    """Compare the maintained CSV to the same layout checked in the binary."""
    kinds = {"data": 1, "app": 0}
    subtypes = {"nvs": 2, "phy": 1, "ota": 0, "ota_0": 0x10, "ota_1": 0x11}
    rows = []
    try:
        for row in csv.reader(read_file(source, "firmware/partitions.csv").decode().splitlines()):
            if not row or row[0].lstrip().startswith("#"):
                continue
            name, kind, subtype, offset, size, flags = [value.strip() for value in row]
            require(not flags, "Partition flags changed")
            rows.append((name, kinds[kind], subtypes[subtype], int(offset, 0), int(size, 0)))
    except (KeyError, ValueError) as exc:
        raise ExportError("Unsupported source partition CSV") from exc
    require(tuple(rows) == LAYOUT, "Source partition layout changed")


def check_image(data):
    """Check the ESP32-S3 header, segment bounds, XOR checksum, and SHA-256 trailer."""
    require(len(data) >= 24 and data[0] == 0xE9 and 1 <= data[1] <= 16
            and int.from_bytes(data[12:14], "little") == 9 and data[23] == 1,
            "Invalid or unsupported ESP32-S3 image header")
    position, checksum = 24, 0xEF
    for _ in range(data[1]):
        require(position + 8 <= len(data), "Truncated image segment header")
        length = struct.unpack_from("<I", data, position + 4)[0]
        position += 8
        require(position + length <= len(data), "Truncated image segment")
        for value in data[position:position + length]:
            checksum ^= value
        position += length
    checksum_end = (position // 16 + 1) * 16
    require(len(data) == checksum_end + 32 and data[checksum_end - 1] == checksum
            and hashlib.sha256(data[:checksum_end]).digest() == data[checksum_end:],
            "Invalid image checksum, digest, or trailing data")


def flag_map(tokens):
    """Accept only the three selected esptool flash settings, in any order."""
    require(isinstance(tokens, list) and len(tokens) == 6
            and all(isinstance(token, str) for token in tokens), "Invalid flash arguments")
    result = dict(zip(tokens[::2], tokens[1::2]))
    require(result == {"--" + key: value for key, value in SETTINGS.items()}, "Unsafe flash arguments")
    return result


def inspect_flash(root):
    """Cross-check both flash metadata formats and the four allowed payloads."""
    metadata = read_json(root, "flasher_args.json")
    flag_map(metadata.get("write_flash_args"))
    require(metadata.get("flash_settings") == SETTINGS and metadata.get("extra_esptool_args") == EXTRA_ARGS,
            "Unsupported flash settings or target")
    expected = {hex(offset): name for offset, name, _ in ROLES.values()}
    require(metadata.get("flash_files") == expected, "Unexpected flash file, offset, or path")
    for role, (offset, name, limit) in ROLES.items():
        require(metadata.get(role) == {"offset": hex(offset), "file": name, "encrypted": "false"},
                "Flash role metadata mismatch or encryption selected")
        data = read_file(root, name)
        require(0 < len(data) <= limit, "Artifact exceeds its flash region or is empty")
        if role in ("app", "bootloader"):
            check_image(data)
        elif role == "partition-table":
            check_layout(data)
        else:
            require(data == b"\xff" * 8192, "Unexpected OTA initialization data")
    try:
        tokens = shlex.split(read_file(root, "flash_args").decode())
    except (UnicodeError, ValueError) as exc:
        raise ExportError("Invalid flash argument file") from exc
    flag_map(tokens[:6])
    require(len(tokens) == 14 and dict(zip(tokens[6::2], tokens[7::2])) == expected,
            "Flash argument file disagrees with JSON")
    return metadata


def inspect_build(source, build_dir, profile):
    """Bind SDK metadata and generated profiles to the selected project inputs."""
    check_source_layout(source)
    inspect_flash(build_dir)
    description = read_json(build_dir, "project_description.json")
    require(description.get("project_name") == "matter_ir_terminal"
            and description.get("target") == "esp32s3"
            and description.get("app_bin") == "matter_ir_terminal.bin", "Project identity mismatch")
    for key, path in (("project_path", source / "firmware"), ("build_dir", build_dir),
                      ("config_file", build_dir / "sdkconfig")):
        require(isinstance(description.get(key), str)
                and Path(description[key]).resolve() == path.resolve(), "Build path metadata mismatch")
    if os.environ.get("IDF_PATH"):
        require(Path(description.get("idf_path", "")).resolve() == Path(os.environ["IDF_PATH"]).resolve(),
                "Build used a different ESP-IDF checkout")
    cache = {}
    for line in read_file(build_dir, "CMakeCache.txt").decode().splitlines():
        match = re.match(r"([^:#/][^:=]*):[^=]+=(.*)", line)
        if match:
            cache[match[1]] = match[2]
    require(Path(cache.get("PROFILE_CONFIG", "")).resolve() == profile.resolve()
            and Path(cache.get("CMAKE_HOME_DIRECTORY", "")).resolve() == (source / "firmware").resolve(),
            "CMake selected different project or profile inputs")
    config = {}
    for line in read_file(build_dir, "sdkconfig").decode().splitlines():
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            require(key not in config, "Duplicate SDK configuration value")
            config[key] = value
    require(all(config.get(key) == value for key, value in CONFIG_VALUES.items())
            and config.get("CONFIG_PARTITION_TABLE_OFFSET", "").lower() == "0xc000",
            "SDK configuration disagrees with flash contract")
    devices = load_config(profile)
    require(read_file(build_dir, "esp-idf/main/generated/profiles.hpp").decode().replace("\r\n", "\n")
            == generate_header(devices), "Generated profile differs from selected configuration")
    build_files = list(METADATA_FILES) + sorted(PAYLOADS)
    if (source / "config/proflame2.json").exists():
        require(Path(cache.get("PROFLAME2_CONFIG", "")).resolve() == (source / "config/proflame2.json").resolve(),
                "Export currently supports only the workspace's default X0A configuration")
        build_files.append("esp-idf/main/generated/proflame2_config.hpp")
    hashes = {name: digest(read_file(build_dir, name)) for name in sorted(build_files)}
    return hashes, {"target": "esp32s3", "flash": SETTINGS, "partition_table_offset": "0xc000",
                    "sdkconfig_sha256": hashes["sdkconfig"],
                    "profile": {"sha256": digest(profile.read_bytes()), "count": len(validate_config(devices)),
                                "enabled_kinds": [p["kind"] for p in devices["devices"] if p["enabled"]]}}


def run_build(source, build_dir, profile):
    """Invoke idf.py with a build-local sdkconfig; preserve firmware/sdkconfig."""
    require(os.environ.get("IDF_PATH"), "Activate the pinned SDK environment first")
    command = [sys.executable, str(Path(os.environ["IDF_PATH"]) / "tools/idf.py"),
               "-B", str(build_dir), "-DIDF_TARGET=esp32s3",
               "-DSDKCONFIG=" + str(build_dir / "sdkconfig"), "-DPROFILE_CONFIG=" + str(profile), "build"]
    # The log is local evidence only and is never copied into an export.
    with (build_dir / "artifact-build.log").open("wb") as log:
        process = subprocess.run(command, cwd=source / "firmware", stdout=log, stderr=subprocess.STDOUT)
    require(process.returncode == 0, "SDK build failed; inspect the local artifact-build.log")


def build(source, build_dir, profile):
    """Build in a fresh directory and record provenance only after all checks pass."""
    source, build_dir, profile = map(plain_path, (source, build_dir, profile))
    require(not build_dir.exists(), "Build requires a new directory; existing builds are never overwritten")
    before = source_inputs(source, profile)
    check_source_layout(source)
    build_dir.mkdir(parents=True)
    run_build(source, build_dir, profile)
    require(source_inputs(source, profile) == before, "Source or SDK inputs changed during build")
    hashes, summary = inspect_build(source, build_dir, profile)
    (build_dir / RECEIPT).write_bytes(canonical({"schema": 1, "inputs": before,
                                               "build_files": hashes, "configuration": summary}))


def checksums(files):
    """Return the standard SHA-256 text format with canonical path ordering."""
    return "".join(f"{digest(data)}  {name}\n" for name, data in sorted(files.items())).encode()


def package(source, build_dir, profile, output):
    """Validate a successful build receipt, then atomically publish a new directory."""
    source, build_dir, profile, output = map(plain_path, (source, build_dir, profile, output))
    require(not output.exists(), "Output already exists; nothing was overwritten")
    require(not output.is_relative_to(build_dir), "Export must be outside the build directory")
    receipt = read_json(build_dir, RECEIPT)
    inputs = source_inputs(source, profile)
    require(receipt.get("schema") == 1 and receipt.get("inputs") == inputs, "Source or SDK changed; stale build")
    hashes, summary = inspect_build(source, build_dir, profile)
    require(receipt.get("build_files") == hashes and receipt.get("configuration") == summary,
            "Build outputs or configuration changed; stale build")
    files = {name: read_file(build_dir, name) for name in sorted(PAYLOADS)}
    for name, data in files.items():
        require(digest(data) == hashes[name], "Build changed while reading payloads")
    metadata = {"write_flash_args": [item for key, value in SETTINGS.items() for item in ("--" + key, value)],
                "flash_settings": SETTINGS, "extra_esptool_args": EXTRA_ARGS,
                "flash_files": {hex(offset): name for offset, name, _ in ROLES.values()}}
    metadata.update({role: {"offset": hex(offset), "file": name, "encrypted": "false"}
                     for role, (offset, name, _) in ROLES.items()})
    files["flasher_args.json"] = canonical(metadata)
    files["flash_args"] = (" ".join(metadata["write_flash_args"]) + "\n" +
                           "".join(f"{hex(offset)} {name}\n" for offset, name, _ in ROLES.values())).encode()
    files["build-config.json"] = canonical(summary)
    files["README.txt"] = USAGE.encode()
    manifest = {"schema": 1, "scope": "local-development", "target": "esp32s3",
                "exporter_sha256": digest(Path(__file__).read_bytes()),
                "source": {"files": inputs["files"], "sha256": digest(canonical(inputs["files"]))},
                "sdks": inputs["sdks"], "configuration_sha256": digest(files["build-config.json"]),
                "artifacts": {name: {"size": len(data), "sha256": digest(data)} for name, data in files.items()}}
    files["manifest.json"] = canonical(manifest)
    files["SHA256SUMS.txt"] = checksums(files)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".artifact-export-", dir=output.parent) as temporary:
        staged = Path(temporary) / "package"
        staged.mkdir()
        for name, data in files.items():
            path = staged / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        verify(staged)
        require(not output.exists(), "Output was created concurrently; nothing was overwritten")
        staged.rename(output)
    return manifest


def verify(output):
    """Check exact contents, checksums, flash paths, image integrity, and slot limits.

    This is consistency verification, not authentication of who built a package.
    Source inputs and SDKs are checked separately when exporting from a receipt.
    """
    output = plain_path(output)
    require(output.is_dir(), "Package directory is missing")
    names = set()
    for path in output.rglob("*"):
        plain_path(path)
        if path.is_file():
            names.add(path.relative_to(output).as_posix())
        else:
            require(path.is_dir() and path.relative_to(output).as_posix() in ("bootloader", "partition_table"),
                    "Unexpected package directory or special file")
    require(names == PACKAGE_FILES, "Missing or unexpected package files")
    files = {name: read_file(output, name) for name in names}
    require(files["SHA256SUMS.txt"] == checksums({name: data for name, data in files.items() if name != "SHA256SUMS.txt"}),
            "Package checksum mismatch")
    manifest = read_json(output, "manifest.json")
    require(manifest.get("schema") == 1 and manifest.get("scope") == "local-development"
            and manifest.get("target") == "esp32s3", "Unsupported package manifest")
    expected = {name: {"size": len(data), "sha256": digest(data)} for name, data in files.items()
                if name not in ("manifest.json", "SHA256SUMS.txt")}
    require(manifest.get("artifacts") == expected
            and manifest.get("configuration_sha256") == digest(files["build-config.json"]), "Manifest mismatch")
    inspect_flash(output)
    summary = read_json(output, "build-config.json")
    require(summary.get("target") == "esp32s3" and summary.get("flash") == SETTINGS
            and summary.get("partition_table_offset") == "0xc000", "Configuration summary mismatch")
    check_provenance(manifest, summary)
    return manifest


def check_provenance(manifest, summary):
    """Validate identity syntax and internal consistency without claiming authenticity."""
    def hex_digest(value, length=64):
        return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{" + str(length) + r"}", value)
    source = manifest.get("source")
    sdks = manifest.get("sdks")
    require(isinstance(source, dict) and isinstance(source.get("files"), dict) and source["files"],
            "Missing source identity")
    for name, value in source["files"].items():
        require(re.fullmatch(r"(?:firmware|tools|config)/[A-Za-z0-9_./-]+", name)
                and all(part not in ("", ".", "..") for part in name.split("/")) and hex_digest(value),
                "Invalid source identity entry")
    require(source.get("sha256") == digest(canonical(source["files"])), "Source identity digest mismatch")
    require(isinstance(sdks, dict) and set(sdks) == {"esp-idf", "esp-matter", "connectedhomeip"}
            and all(hex_digest(value, 40) for value in sdks.values()), "Invalid SDK identity")
    require(hex_digest(manifest.get("exporter_sha256")), "Invalid exporter identity")
    profile = summary.get("profile")
    require(hex_digest(summary.get("sdkconfig_sha256")) and isinstance(profile, dict)
            and hex_digest(profile.get("sha256")) and type(profile.get("count")) is int
            and 1 <= profile["count"] <= 8 and isinstance(profile.get("enabled_kinds"), list)
            and len(profile["enabled_kinds"]) <= profile["count"]
            and all(isinstance(kind, str) and kind in KINDS for kind in profile["enabled_kinds"]),
            "Invalid configuration identity summary")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("build", "package"):
        command = commands.add_parser(name, help="Build and export" if name == "build" else "Re-export a recorded build")
        command.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[1])
        command.add_argument("--build-dir", type=Path, required=True)
        command.add_argument("--profile", type=Path, help="Default: SOURCE/config/devices.json")
        command.add_argument("--output", type=Path, required=True)
    commands.add_parser("verify", help="Verify a standalone package").add_argument("directory", type=Path)
    args = parser.parse_args()
    try:
        if args.command == "verify":
            verify(args.directory)
        else:
            profile = args.profile or args.source_root / "config/devices.json"
            require(not plain_path(args.output).exists(), "Output already exists; nothing was overwritten")
            if args.command == "build":
                build(args.source_root, args.build_dir, profile)
            package(args.source_root, args.build_dir, profile, args.output)
    except (OSError, ValueError, RuntimeError) as exc:
        # Do not print arbitrary input values, paths, profile contents, or SDK output.
        message = str(exc) if isinstance(exc, ExportError) else "Input or filesystem error; no package was accepted"
        parser.exit(1, f"Artifact export error: {message}\n")
    print("Local development package verified; no device operation performed.")


if __name__ == "__main__":
    main()
