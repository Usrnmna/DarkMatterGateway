"""Validate the separate X0A profile and generate firmware constants."""
import argparse
import json
from pathlib import Path
from profile_config import NAME, load_config, validate_config as validate_ir

FIELDS = ("power", "flame", "fan", "light", "aux", "split_flow")
IDENTITY = {"appliance", "ifc", "handset", "controller_board", "interface_revision"}
TIMINGS = ("response_timeout_ms", "inter_request_ms", "resync_quiet_ms",
           "snapshot_max_age_ms", "inter_byte_ms")
KEYS = {"schema_version", "enabled", "device_id", "identity", "qualification_record",
        "qualified_for_control", "fields", "timings"}

# No physical qualification exists. Future entries must be reviewed source changes
# matching the complete configuration contract (excluding enabled), linked to real
# records. Configuration input cannot add an entry to this registry.
QUALIFIED_PROFILES = {}


def optional_text(value, label):
    """Accept an unknown value or a bounded identifier without control characters."""
    if value is not None and (not isinstance(value, str) or not value.strip()
                              or len(value) > 240 or any(ord(c) < 32 or ord(c) == 127 for c in value)):
        raise ValueError(f"{label} must be null or a nonempty bounded string")


def validate_config(data, ir_devices):
    """Validate syntax, IR ownership conflicts and the reviewed hardware contract."""
    if not isinstance(data, dict) or set(data) != KEYS:
        raise ValueError("X0A root contains missing or unknown fields")
    if type(data["schema_version"]) is not int or data["schema_version"] != 1:
        raise ValueError("schema_version must be 1")
    for key in ("enabled", "qualified_for_control"):
        if type(data[key]) is not bool:
            raise ValueError(f"{key} must be boolean")
    name = data["device_id"]
    if not isinstance(name, str) or not NAME.fullmatch(name):
        raise ValueError("device_id must be a lower-case name of 1..15 characters")
    if any(device["id"] == name and device["enabled"] for device in ir_devices):
        raise ValueError("the IR profile with the same device_id must remain disabled")
    identity = data["identity"]
    if not isinstance(identity, dict) or set(identity) != IDENTITY:
        raise ValueError("identity contains missing or unknown fields")
    for key, value in identity.items():
        optional_text(value, key)
    optional_text(data["qualification_record"], "qualification_record")
    fields = data["fields"]
    if (not isinstance(fields, list) or any(not isinstance(f, str) or f not in FIELDS for f in fields)
            or len(set(fields)) != len(fields)):
        raise ValueError("fields must be a unique ordinary-control allowlist")
    timings = data["timings"]
    if not isinstance(timings, dict) or set(timings) != set(TIMINGS):
        raise ValueError("timings contains missing or unknown fields")
    for key, value in timings.items():
        maximum = 120000 if key in ("resync_quiet_ms", "snapshot_max_age_ms") else 60000
        if value is not None and (type(value) is not int or not 1 <= value <= maximum):
            raise ValueError(f"{key} must be null or an integer in 1..{maximum}")
    if all(value is not None for value in timings.values()):
        if (timings["inter_byte_ms"] > timings["response_timeout_ms"]
                or timings["resync_quiet_ms"] < timings["response_timeout_ms"] + timings["inter_byte_ms"]
                or timings["snapshot_max_age_ms"] <= timings["inter_request_ms"]):
            raise ValueError("inconsistent timing bounds")
    if data["enabled"] and not data["qualified_for_control"]:
        raise ValueError("enabled control requires a reviewed qualification record")
    if data["qualified_for_control"]:
        if (not all(identity.values()) or not data["qualification_record"] or not fields
                or any(value is None for value in timings.values())):
            raise ValueError("qualification requires complete identity, fields and measured timings")
        contract = {key: value for key, value in data.items() if key != "enabled"}
        if QUALIFIED_PROFILES.get(data["qualification_record"]) != contract:
            raise ValueError("no matching reviewed hardware profile; activation remains blocked")
    return data


def generate_header(data, ir_devices):
    """Emit typed constants; unknown production values remain zero or disabled."""
    validate_config(data, ir_devices)
    enabled = data["enabled"] and data["qualified_for_control"]
    flag = lambda value: str(bool(value)).lower()
    values = ", ".join(str(data["timings"][key] or 0) for key in TIMINGS)
    fields = ", ".join(flag(enabled and key in data["fields"]) for key in FIELDS)
    return ("// Generated X0A configuration; edit the selected PROFLAME2_CONFIG file.\n"
            '#pragma once\n#include "proflame2_controller.hpp"\n'
            "namespace proflame2_config {\n"
            f"inline constexpr bool kEnabled = {flag(enabled)};\n"
            f"inline constexpr bool kQualifiedForControl = {flag(data['qualified_for_control'])};\n"
            f"inline constexpr const char *kDeviceId = {json.dumps(data['device_id'])};\n"
            f"inline constexpr proflame2::Timings kTimings{{{values}}};\n"
            f"inline constexpr proflame2::Capabilities kCapabilities{{{fields}}};\n"
            "}\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", type=Path)
    parser.add_argument("--ir-profile", required=True, type=Path)
    parser.add_argument("--header", type=Path)
    args = parser.parse_args()
    try:
        data = load_config(args.config)
        devices = validate_ir(load_config(args.ir_profile))
        header = generate_header(data, devices)
    except (OSError, ValueError, TypeError) as exc:
        parser.exit(1, f"Configuration error: {exc}\n")
    if args.header:
        args.header.parent.mkdir(parents=True, exist_ok=True)
        args.header.write_text(header, encoding="utf-8")
    print(f"Validated X0A configuration: enabled={data['enabled']}")


if __name__ == "__main__":
    main()
