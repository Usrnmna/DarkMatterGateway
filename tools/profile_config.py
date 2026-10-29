"""Validate appliance mappings and generate firmware constants without dependencies."""

import argparse
import json
from pathlib import Path
import re

NAME = re.compile(r"[a-z0-9_]{1,15}\Z")
KINDS = {"light": "Light", "fan": "Fan", "gas_fireplace": "GasFireplace",
         "portable_ac": "PortableAc", "raw": "Raw"}
REQUIRED = {"light": ("on", "off"), "fan": ("off", "low", "medium", "high")}
COMMON = {"id", "kind", "enabled", "zone", "manufacturer", "model", "remote_model",
          "transport_verified_ir", "discrete_commands_verified", "commands"}
EXTRA = {"gas_fireplace": {"remote_operation_permitted"},
         "portable_ac": {"temperature_unit", "temperature_min", "temperature_max",
                         "state_frame_verified"}}


def validate_config(data):
    """Reject ambiguous commands and unsupported native profile assumptions."""
    if not isinstance(data, dict) or set(data) != {"schema_version", "devices"}:
        raise ValueError("root requires only schema_version and devices")
    if type(data["schema_version"]) is not int or data["schema_version"] != 1:
        raise ValueError("schema_version must be 1")
    devices = data["devices"]
    if not isinstance(devices, list) or not 1 <= len(devices) <= 8:
        raise ValueError("devices must contain 1..8 profiles")
    seen = set()
    for device in devices:
        if not isinstance(device, dict):
            raise ValueError("each profile must be an object")
        kind = device.get("kind")
        if not isinstance(kind, str) or kind not in KINDS:
            raise ValueError("unsupported kind")
        if set(device) != COMMON | EXTRA.get(kind, set()):
            raise ValueError(f"{device.get('id')}: missing or unknown fields")
        name = device["id"]
        if not isinstance(name, str) or not NAME.fullmatch(name) or name in seen:
            raise ValueError("profile ids must be unique lower-case names of 1..15 characters")
        seen.add(name)
        if type(device["zone"]) is not int or device["zone"] not in (0, 1, 2):
            raise ValueError(f"{name}: zone must be 0, 1, or 2")
        for key in ("enabled", "transport_verified_ir", "discrete_commands_verified"):
            if type(device[key]) is not bool:
                raise ValueError(f"{name}: {key} must be boolean")
        for key in ("manufacturer", "model", "remote_model"):
            value = device[key]
            if value is not None and (not isinstance(value, str) or not value.strip() or len(value) > 120):
                raise ValueError(f"{name}: {key} must be null or a nonempty string up to 120 characters")
        commands = device["commands"]
        if not isinstance(commands, dict) or not commands or len(commands) > 32:
            raise ValueError(f"{name}: commands must contain 1..32 actions")
        for action, command in commands.items():
            if (not isinstance(action, str) or not NAME.fullmatch(action)
                    or not isinstance(command, str) or not NAME.fullmatch(command)):
                raise ValueError(f"{name}: action and command names require [a-z0-9_] and at most 15 characters")
        if kind in REQUIRED:
            if set(commands) != set(REQUIRED[kind]):
                raise ValueError(f"{name}: required commands are {REQUIRED[kind]}")
            if len(set(commands.values())) != len(commands):
                raise ValueError(f"{name}: native controls require separate absolute commands, not one toggle")
        if kind == "gas_fireplace" and type(device["remote_operation_permitted"]) is not bool:
            raise ValueError(f"{name}: remote_operation_permitted must be boolean")
        if kind == "portable_ac":
            if device["temperature_unit"] not in (None, "C", "F"):
                raise ValueError(f"{name}: temperature_unit must be null, C, or F")
            if type(device["state_frame_verified"]) is not bool:
                raise ValueError(f"{name}: state_frame_verified must be boolean")
            low, high = device["temperature_min"], device["temperature_max"]
            if (low is None) != (high is None):
                raise ValueError(f"{name}: supply both temperature bounds or neither")
            if low is not None and (type(low) not in (int, float) or type(high) not in (int, float)
                                    or not -50 <= low <= high <= 150):
                raise ValueError(f"{name}: invalid temperature bounds")
        if device["enabled"]:
            if not device["transport_verified_ir"] or not device["model"]:
                raise ValueError(f"{name}: enabled profiles require model and verified IR transport")
            if kind in REQUIRED and not device["discrete_commands_verified"]:
                raise ValueError(f"{name}: native control requires verified absolute commands")
            if kind == "gas_fireplace" and not device["remote_operation_permitted"]:
                raise ValueError(f"{name}: manufacturer-permitted remote operation must be verified")
    return devices


def generate_header(data):
    """Generate constants and manual action aliases; arbitrary strings never become C++ code."""
    devices = validate_config(data)
    rows = []
    actions = []
    for device in devices:
        fields = [json.dumps(device["id"]), f"ir::Kind::{KINDS[device['kind']]}",
                  str(device["enabled"]).lower(), str(device["zone"])]
        fields += [json.dumps(device["commands"].get(action, ""))
                   for action in ("on", "off", "low", "medium", "high")]
        rows.append("    {" + ", ".join(fields) + "}")
        for action, command in device["commands"].items():
            actions.append("    {" + ", ".join(json.dumps(item) for item in
                           (device["id"], action, command)) + "}")
    return ("// Generated appliance mappings; edit the selected PROFILE_CONFIG file.\n"
            '#pragma once\n#include "ir_core.hpp"\n#include <cstddef>\n'
            "namespace configured {\ninline constexpr ir::Profile profiles[] = {\n" +
            ",\n".join(rows) + "\n};\n"
            "struct Action { const char *device; const char *action; const char *code; };\n"
            "inline constexpr Action actions[] = {\n" + ",\n".join(actions) + "\n};\n}\n")


def load_config(path):
    """Reject repeated keys rather than silently replacing configuration values."""
    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"duplicate JSON key: {key}")
            result[key] = value
        return result

    def invalid_constant(value):
        raise ValueError(f"non-JSON numeric constant: {value}")

    return json.loads(Path(path).read_text(encoding="utf-8"),
                      object_pairs_hook=unique_object, parse_constant=invalid_constant)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", type=Path)
    parser.add_argument("--header", type=Path)
    args = parser.parse_args()
    try:
        data = load_config(args.config)
        header = generate_header(data)
    except (OSError, ValueError, TypeError) as exc:
        parser.exit(1, f"Configuration error: {exc}\n")
    if args.header:
        args.header.parent.mkdir(parents=True, exist_ok=True)
        args.header.write_text(header, encoding="utf-8")
    print(f"Validated {len(data['devices'])} appliance profiles")


if __name__ == "__main__":
    main()
