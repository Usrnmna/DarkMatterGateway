#!/usr/bin/env python3
"""Verify selected service regressions are caught, using temporary source copies.

This optional Linux/WSL check is deliberately separate from the normal host suite.
Each mutation must compile and fail its selected test with an assertion. Compiler
errors, unrelated crashes, and a surviving mutation fail this verification.
"""

import json
import os
from pathlib import Path
import resource
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FLAGS = ["-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic", "-g"]
# Sanitizers are exercised by test-host.sh. Assertions alone make the optional
# mutation run's expected failures specific and independent of sanitizer startup.
MUTATIONS = [
    ("queue_limit", "ir_service", "queue_limit_and_failed_insert",
     "outstanding_jobs >= kQueueDepth", "false", 2),
    ("failed_queue_accounting", "ir_service", "queue_limit_and_failed_insert",
     "        --outstanding_jobs;\n        delete job;", "        delete job;", 1),
    ("capture_while_queued", "ir_service", "queue_limit_and_failed_insert",
     "if (!lock || hardware_fault || outstanding_jobs != 0)", "if (!lock || hardware_fault)", 1),
    ("pending_copy_ownership", "ir_service", "queued_waveform_ownership",
     "ir::Waveform(pending)", "std::move(pending)", 1),
    ("teardown_ownership", "ir_service", "teardown_fault_latching",
     "if (!close_channel(channel, enabled)) return ESP_FAIL;", "close_channel(channel, enabled);", 2),
    ("fault_latch", "ir_service", "teardown_fault_latching",
     "hardware_fault = true;", "hardware_fault = false;", 1),
    ("worker_error_reporting", "ir_service", "transmit_failure_cleanup",
     "const esp_err_t error = transmit_locked(*job);",
     "transmit_locked(*job);\n        const esp_err_t error = ESP_OK;", 1),
    ("overwrite_confirmation", "ir_storage", "save_requires_pending_and_explicit_overwrite",
     "error == ESP_OK && !overwrite", "error == ESP_OK && overwrite", 1),
    ("commit_error", "ir_storage", "commit_failure_is_visible",
     "error = nvs_set_blob(storage, name, bytes.data(), bytes.size());\n"
     "    if (error == ESP_OK) error = nvs_commit(storage);",
     "error = nvs_set_blob(storage, name, bytes.data(), bytes.size());\n"
     "    if (error == ESP_OK) (void)nvs_commit(storage);", 1),
    ("readback_api", "ir_storage", "readback_api_failures",
     "error = load_locked(name, verified);", "verified = pending;", 1),
    ("readback_carrier", "ir_storage", "readback_corruption_and_mismatch",
     "verified.carrier_hz != pending.carrier_hz", "false", 1),
    ("readback_duty", "ir_storage", "readback_corruption_and_mismatch",
     "verified.duty_percent != pending.duty_percent", "false", 1),
    ("readback_durations", "ir_storage", "readback_corruption_and_mismatch",
     "verified.durations_us != pending.durations_us", "false", 1),
    ("automatic_storage_erase", "ir_storage", "initialization_failure_preserves_records",
     "if (error == ESP_OK) error = nvs_flash_init_partition(kStoragePartition);",
     "if (error == ESP_OK) error = nvs_flash_init_partition(kStoragePartition);\n"
     "    if (error != ESP_OK) nvs_flash_erase_partition(kStoragePartition);", 1),
]


def compile_suite(directory, suite, executable):
    """Compile the real service through its fixture with the selected C++ compiler."""
    command = [os.environ.get("CXX", "g++"), *FLAGS,
               "-Itests/service_stubs", "-Ifirmware/components/ir_core/include",
               "firmware/components/ir_core/ir_core.cpp", "tests/service_stubs/support.cpp",
               f"tests/test_{suite}.cpp", "-o", str(executable)]
    subprocess.run(command, cwd=directory, check=True, timeout=60)


def main():
    """Check both baselines first, then change one contract per temporary build."""
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    evidence = []
    with tempfile.TemporaryDirectory(prefix="matter-ir-mutations-") as temporary:
        directory = Path(temporary)
        for relative in ("firmware/main/ir_service.cpp", "firmware/main/ir_service.hpp",
                         "firmware/main/board_config.hpp", "tests/service_fixture.hpp",
                         "tests/test_ir_service.cpp", "tests/test_ir_storage.cpp"):
            destination = directory / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, destination)
        for relative in ("firmware/components/ir_core", "tests/service_stubs"):
            shutil.copytree(ROOT / relative, directory / relative)
        service = directory / "firmware/main/ir_service.cpp"
        original = service.read_text()
        for name, _, _, before, _, occurrences in MUTATIONS:
            assert original.count(before) == occurrences, f"Update stale mutation: {name}"
        for suite in ("ir_service", "ir_storage"):
            executable = directory / suite
            compile_suite(directory, suite, executable)
            subprocess.run([str(executable)], check=True, timeout=30)
        for name, suite, case, before, after, occurrences in MUTATIONS:
            service.write_text(original.replace(before, after))
            executable = directory / name
            compile_suite(directory, suite, executable)
            result = subprocess.run([str(executable), case], capture_output=True, text=True, timeout=30)
            caught = result.returncode != 0 and "Assertion" in result.stderr and "failed" in result.stderr
            evidence.append({"mutation": name, "case": case, "caught": caught,
                             "returncode": result.returncode, "stderr": result.stderr.strip()})
            print(json.dumps(evidence[-1]), flush=True)
            if not caught:
                raise RuntimeError(f"Mutation did not produce the expected assertion: {name}")
        print(f"PASS: {len(evidence)} of {len(MUTATIONS)} selected mutations caught", flush=True)


if __name__ == "__main__":
    main()
