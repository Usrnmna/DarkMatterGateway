#!/usr/bin/env python3
"""Check selected Matter/startup regressions in disposable Linux/WSL copies.

Both original suites must pass before mutations run. Each mutation must compile
and fail a selected assertion; crashes, timeouts, and compiler errors do not pass.
"""
import json
import os
from pathlib import Path
import resource
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MATTER = "firmware/main/matter_app.cpp"
STARTUP = "firmware/main/app_main.cpp"
MUTATIONS = [
    ("startup_callback_gate", MATTER, "matter_app", "topology_and_startup_suppression",
     "!ready || !context", "!context"),
    ("disabled_profile", MATTER, "matter_app", "topology_and_startup_suppression",
     "!profile.enabled || ", ""),
    ("endpoint_parent", MATTER, "matter_app", "topology_and_startup_suppression",
     "set_parent_endpoint(endpoint, aggregator)", "set_parent_endpoint(endpoint, endpoint)"),
    ("command_zone", MATTER, "matter_app", "light_mapping_and_missing_command",
     "send(command, profile.zone)", "send(command, 0)"),
    ("nullable_fan_value", MATTER, "matter_app", "fan_modes_and_null_rejection",
     "percent = value->val.u8;", "percent = value->val.u8 == 255 ? 0 : value->val.u8;"),
    ("synchronization_guard_release", MATTER, "matter_app", "reporting_failure_releases_recursion_guard",
     "context.synchronizing = false;", "context.synchronizing = true;"),
    ("deduplication_boundary", MATTER, "matter_app", "fan_deduplication_and_endpoint_isolation",
     "now - context->last_queued_us < 100000", "now - context->last_queued_us < 100001"),
    ("queue_error_propagation", MATTER, "matter_app", "queue_rejection_preserves_state_and_retry",
     "const auto error = ir_service::send(command, profile.zone);\n        if (error != ESP_OK) return error;",
     "const auto error = ir_service::send(command, profile.zone);\n        if (error != ESP_OK) return ESP_OK;"),
    ("rejected_request_cache", MATTER, "matter_app", "queue_rejection_preserves_state_and_retry",
     "if (error != ESP_OK) return error;\n        context->last_command = command;\n        context->last_queued_us = now;",
     "context->last_command = command;\n        context->last_queued_us = now;\n        if (error != ESP_OK) return error;"),
    ("startup_fatal_error", STARTUP, "startup", "startup_stops_on_initialization_failure",
     "ESP_ERROR_CHECK(nvs_flash_init());", "(void)nvs_flash_init();"),
    ("automatic_storage_erase", STARTUP, "startup", "startup_stops_on_initialization_failure",
     "ESP_ERROR_CHECK(nvs_flash_init());",
     "const auto error = nvs_flash_init();\n    if (error != ESP_OK) nvs_flash_erase();\n    ESP_ERROR_CHECK(error);"),
    ("commissioning_advertisement", MATTER, "startup", "commissioning_advertisement_and_schedule_failures",
     "provisioned ? chip::CommissioningWindowAdvertisement::kDnssdOnly",
     "provisioned ? chip::CommissioningWindowAdvertisement::kAllSupported"),
    ("existing_window_deadline", MATTER, "startup", "existing_windows_and_window_failure",
     "if (!manager.IsCommissioningWindowOpen())", "if (true)"),
    ("enhanced_window_pairing", MATTER, "startup", "existing_windows_and_window_failure",
     "manager.GetCommissioningMode() != chip::Dnssd::CommissioningMode::kEnabledBasic",
     "manager.GetCommissioningMode() == chip::Dnssd::CommissioningMode::kEnabledBasic"),
    ("reset_busy_gate", MATTER, "startup", "reset_gating_scheduling_and_failure",
     "if (!ready || ir_service::busy())", "if (!ready)"),
    ("recovery_replay", MATTER, "startup", "reset_gating_scheduling_and_failure",
     "const auto error = esp_matter::factory_reset();",
     "const auto error = esp_matter::factory_reset();\n    ir_service::send(\"fan_on\", 0);"),
    ("held_button_repeats", STARTUP, "startup", "button_debounce_hold_and_rearm",
     "handled = true;", "handled = false;"),
    ("button_rearm", STARTUP, "startup", "button_debounce_hold_and_rearm",
     "            handled = false;", "            handled = true;"),
]


def compile_suite(directory, suite, executable):
    """Compile actual adapter/startup code through its host fixture."""
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-pedantic", "-g", "-Itests/app_stubs", "-Ifirmware/components/ir_core/include",
                    "-Ifirmware/components/proflame2_core/include",
                    "firmware/components/ir_core/ir_core.cpp", "tests/app_stubs/support.cpp",
                    f"tests/test_{suite}.cpp", "-o", str(executable)], cwd=directory, check=True, timeout=60)


def main():
    """Stage required inputs, verify baselines, then change one contract at a time."""
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    with tempfile.TemporaryDirectory(prefix="matter-app-mutations-") as temporary:
        directory = Path(temporary)
        for relative in (MATTER, STARTUP, "firmware/main/matter_app.hpp", "firmware/main/board_config.hpp",
                         "firmware/main/ir_service.hpp", "firmware/main/terminal.hpp",
                         "firmware/main/proflame2_service.hpp",
                         "tests/app_fixture.hpp", "tests/test_matter_app.cpp", "tests/test_startup.cpp",
                         "tests/service_stubs/sdk.hpp"):
            destination = directory / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, destination)
        for relative in ("firmware/components/ir_core", "firmware/components/proflame2_core/include", "tests/app_stubs"):
            shutil.copytree(ROOT / relative, directory / relative)
        originals = {name: (directory / name).read_text() for name in (MATTER, STARTUP)}
        for name, source, _, _, before, _ in MUTATIONS:
            assert originals[source].count(before) == 1, f"Update stale mutation: {name}"
        for suite in ("matter_app", "startup"):
            executable = directory / suite
            compile_suite(directory, suite, executable)
            subprocess.run([str(executable)], check=True, timeout=30)
        for name, source, suite, case, before, after in MUTATIONS:
            for path, original in originals.items():
                (directory / path).write_text(original.replace(before, after) if path == source else original)
            executable = directory / name
            compile_suite(directory, suite, executable)
            result = subprocess.run([str(executable), case], capture_output=True, text=True, timeout=30)
            caught = result.returncode != 0 and "Assertion" in result.stderr and "failed" in result.stderr
            print(json.dumps({"mutation": name, "source": source, "case": case, "caught": caught,
                              "returncode": result.returncode, "stderr": result.stderr.strip()}), flush=True)
            if not caught:
                raise RuntimeError(f"Expected assertion did not fail: {name}")
        print(f"PASS: {len(MUTATIONS)} of {len(MUTATIONS)} selected app mutations caught", flush=True)


if __name__ == "__main__":
    main()
