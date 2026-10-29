"""Compile and inspect the disabled ESP32-S3 baseline without flashing or publishing."""

import argparse
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

from ci_report import Report
import export_artifacts


ROOT = Path(__file__).resolve().parents[1]
# These revisions match tools/bootstrap-sdk.sh; CHIP is selected by ESP-Matter.
SDK_REVISIONS = {"esp-idf": "4c2820d377d1375e787bcef612f0c32c1427d183",
                 "esp-matter": "4cedd4ad40821933d6cc237e80ab6604f2953fb0",
                 "connectedhomeip": "a6892983d6d761a9a787f2e11177a85d28bd1022"}
DIAGNOSTIC_FILES = ("matter_ir_terminal.elf", "matter_ir_terminal.map", "compile_commands.json")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("build/ci/firmware"))
    parser.add_argument("--verify-sdk", action="store_true")
    parser.add_argument("--inspect", type=Path, help="Inspect an existing build without recompiling")
    args = parser.parse_args()
    os.chdir(ROOT)
    if args.verify_sdk:
        actual = export_artifacts.sdk_identity()
        if actual != SDK_REVISIONS:
            raise ValueError("SDK revisions differ from the pinned CI baseline")
        print(json.dumps(actual, indent=2))
        return 0
    if args.inspect:
        hashes, configuration = export_artifacts.inspect_build(ROOT, args.inspect.resolve(), ROOT / "config/devices.json")
        print(json.dumps({"build_hashes": hashes, "configuration": configuration}, indent=2))
        return 0
    report = Report(args.output, "ESP32-S3 target build")
    if not report.run("sdk-identity", [sys.executable, __file__, "--verify-sdk"]):
        return report.finish()
    (ROOT / "build").mkdir(exist_ok=True)
    build = Path(tempfile.mkdtemp(prefix="ci-firmware-", dir=ROOT / "build"))
    idf = str(Path(os.environ["IDF_PATH"]) / "tools/idf.py")
    command = [sys.executable, idf, "-C", str(ROOT / "firmware"), "-B", str(build),
               "-DIDF_TARGET=esp32s3", "-DSDKCONFIG=" + str(build / "sdkconfig"),
               "-DPROFILE_CONFIG=" + str(ROOT / "config/devices.json"),
               "-DPROFLAME2_CONFIG=" + str(ROOT / "config/proflame2.json")]
    if os.environ.get("CI_VERBOSE") == "true":
        command.append("-v")
    report.run("python-packages", [sys.executable, "-m", "pip", "list", "--format=json"])
    built = report.run("firmware-build", command + ["build"], timeout=3300)
    if built:
        report.run("firmware-size", command + ["size"])
        report.run("firmware-layout", [sys.executable, __file__, "--inspect", str(build)])
    # A failed build can still provide useful map or compilation-command output.
    for name in DIAGNOSTIC_FILES:
        path = build / name
        if path.is_file():
            shutil.copy2(path, report.directory / name)
    report.run("dependency-lock", ["git", "diff", "--exit-code", "--", "firmware/dependencies.lock"])
    return report.finish()


if __name__ == "__main__":
    raise SystemExit(main())
