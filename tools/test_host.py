"""Run selectable host checks with separate compile/run logs and strict exit status."""

import argparse
import os
from pathlib import Path
import sys

from ci_report import Report


ROOT = Path(__file__).resolve().parents[1]
CORE = "firmware/components/ir_core/ir_core.cpp"
CORE_INCLUDE = "firmware/components/ir_core/include"
X0A_INCLUDE = "firmware/components/proflame2_core/include"
X0A_SOURCES = [f"firmware/components/proflame2_core/{name}.cpp"
               for name in ("protocol", "controller", "session")]
FLAGS = ["-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
         "-fsanitize=address,undefined", "-g", "-O1", "-fno-omit-frame-pointer"]


def cpp_suites(generated):
    """Describe the existing fourteen executions without duplicating compile logic."""
    suites = []

    def add(group, name, sources, includes, extra=()):
        suites.append((group, name, sources, includes, list(extra)))

    for name in ("protocol", "controller", "service"):
        add("proflame2", f"proflame2-{name}",
            X0A_SOURCES + [f"tests/test_proflame2_{name}.cpp"], [X0A_INCLUDE])
    for name, config in (("disabled", generated), ("unqualified", "tests/x0a_unqualified")):
        add("proflame2", f"proflame2-target-{name}",
            X0A_SOURCES + ["tests/service_stubs/support.cpp", "tests/test_proflame2_target.cpp"],
            [str(config), "tests/x0a_stubs", "tests/service_stubs", X0A_INCLUDE])
    add("proflame2", "proflame2-uart", X0A_SOURCES + ["tests/test_proflame2_uart.cpp"],
        ["tests/x0a_unqualified", "tests/x0a_stubs", "tests/service_stubs", X0A_INCLUDE])
    add("ir", "ir-core", [CORE, "tests/test_ir_core.cpp"], [CORE_INCLUDE])
    for name in ("ir_service", "ir_storage"):
        add("ir", name, [CORE, "tests/service_stubs/support.cpp", f"tests/test_{name}.cpp"],
            ["tests/service_stubs", CORE_INCLUDE])
    for name in ("matter_app", "startup"):
        add("app", name, [CORE, "tests/app_stubs/support.cpp", f"tests/test_{name}.cpp"],
            ["tests/app_stubs", CORE_INCLUDE, X0A_INCLUDE])
    for usb in (0, 1):
        add("console", f"terminal-{usb}", ["tests/test_terminal.cpp"],
            ["tests/terminal_stubs", CORE_INCLUDE, X0A_INCLUDE],
            [f"-DCONFIG_ESP_CONSOLE_USB_SERIAL_JTAG={usb}"])
    add("console", "console-line", ["firmware/main/console_line.cpp", "tests/test_console_line.cpp"],
        ["firmware/main"], ["-Wl,--wrap=linenoise", "-Wl,--wrap=linenoiseProbe"])
    return suites


def run_cpp(report, suite, compiler):
    """Run independent suites after failures; every failure remains in the report."""
    generated = report.directory / "generated"
    report.run("profile-config", [sys.executable, "tools/profile_config.py", "config/devices.json",
                                 "--header", str(generated / "profiles.hpp")])
    config_ok = report.run("proflame2-config", [sys.executable, "tools/proflame2_config.py",
        "config/proflame2.json", "--ir-profile", "config/devices.json",
        "--header", str(generated / "proflame2_config.hpp")])
    environment = dict(os.environ, ASAN_OPTIONS="halt_on_error=1:detect_leaks=1",
                       UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    for group, name, sources, includes, extra in cpp_suites(generated):
        if suite not in ("all", group):
            continue
        if name == "proflame2-target-disabled" and not config_ok:
            continue  # The failed generator already fails the report; no stale header is used.
        directory = report.directory / "binaries" / name
        directory.mkdir(parents=True, exist_ok=True)
        executable = directory / "test"
        command = [compiler, *FLAGS, *[f"-I{item}" for item in includes], *sources,
                   *extra, "-o", str(executable)]
        if report.run(f"compile-{name}", command, timeout=120):
            report.run(f"run-{name}", [str(executable)], timeout=60, env=environment)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phase", choices=("all", "python", "cpp"), default="all")
    parser.add_argument("--suite", choices=("all", "ir", "proflame2", "app", "console"), default="all")
    parser.add_argument("--output", type=Path, default=Path("build/ci/host"))
    parser.add_argument("--serial-smoke", action="store_true", help="Require installed pyserial; no port is opened")
    args = parser.parse_args()
    os.chdir(ROOT)
    report = Report(args.output, f"Host checks: {args.phase} / {args.suite}")
    if args.phase in ("all", "python"):
        report.run("python-tests", [sys.executable, "tools/test_python.py", "--output",
                                   str(report.directory / "python-tests.json")], timeout=480)
        if args.serial_smoke:
            report.run("serial-import", [sys.executable, "-c",
                       "import serial; print(serial.__version__); assert callable(serial.Serial)"])
            report.run("serial-cli", [sys.executable, "tools/ir_terminal.py", "--help"])
    if args.phase in ("all", "cpp"):
        run_cpp(report, args.suite, os.environ.get("CXX", "g++"))
    return report.finish()


if __name__ == "__main__":
    raise SystemExit(main())
