"""Bounded command execution and persistent diagnostics shared by CI and local runs."""

import argparse
import json
import os
from pathlib import Path
import platform
import shlex
import signal
import subprocess
import sys
import time


def version(command):
    """Return bounded version output without dumping environment variables."""
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=15)
        return result.stdout.strip() or result.stderr.strip()
    except (OSError, subprocess.SubprocessError):
        return "unavailable"


class Report:
    """Record every command before execution and retain failures across later successes.

    Output directories contain only explicit logs, JSON, and Markdown. Commands
    are argument lists, never shell-interpolated strings. Linux timeouts terminate
    the process group so compilers and their children do not outlive the check.
    """

    def __init__(self, directory, title):
        self.directory = Path(directory).resolve()
        self.directory.mkdir(parents=True, exist_ok=True)
        self.title = title
        self.results = []
        environment = {
            "python": sys.version, "platform": platform.platform(),
            "commit": version(["git", "rev-parse", "HEAD"]),
            "worktree_changes": version(["git", "status", "--short"]),
            "runner_image": os.environ.get("ImageOS", "local"),
            "runner_image_version": os.environ.get("ImageVersion", "local"),
            "compiler": version([os.environ.get("CXX", "g++"), "--version"]),
        }
        (self.directory / "environment.json").write_text(
            json.dumps(environment, indent=2) + "\n", encoding="utf-8")
        self.save()

    def save(self):
        """Persist partial progress even when a runner later stops unexpectedly."""
        (self.directory / "results.json").write_text(json.dumps({
            "title": self.title, "results": self.results,
        }, indent=2) + "\n", encoding="utf-8")

    def run(self, name, command, *, timeout=300, cwd=None, env=None):
        """Capture a command's combined output and return whether it succeeded."""
        command = [str(arg) for arg in command]
        log_path = self.directory / f"{len(self.results) + 1:02d}-{name}.log"
        entry = {"name": name, "command": command, "cwd": str(Path(cwd or '.').resolve()),
                 "log": log_path.name, "status": "running"}
        self.results.append(entry)
        self.save()
        print(f"Running {name}: {shlex.join(command)}", flush=True)
        start = time.monotonic()
        code = 127
        with log_path.open("wb") as log:
            try:
                process = subprocess.Popen(command, cwd=cwd, env=env, stdout=log,
                                           stderr=subprocess.STDOUT,
                                           start_new_session=os.name != "nt")
                try:
                    code = process.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    if os.name == "nt":
                        subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                                       stdout=log, stderr=subprocess.STDOUT, check=False)
                    else:
                        os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
                    log.write(f"\nCommand exceeded {timeout} seconds.\n".encode())
                    code = 124
            except OSError as error:
                log.write(f"Cannot execute command: {error}\n".encode())
        entry.update(returncode=code, seconds=round(time.monotonic() - start, 3),
                     status="passed" if code == 0 else "failed")
        self.save()
        print(f"{entry['status'].upper()}: {name} ({entry['seconds']}s)", flush=True)
        if code:
            print(log_path.read_text(encoding="utf-8", errors="replace")[-12000:])
        return code == 0

    def finish(self):
        """Write a concise summary; a failed or incomplete command always fails the run."""
        success = bool(self.results) and all(r["status"] == "passed" for r in self.results)
        lines = [f"## {self.title}", "", "| Check | Result | Exit | Log |",
                 "|---|---|---|---|"]
        for result in self.results:
            lines.append(f"| {result['name']} | {result['status']} | "
                         f"{result.get('returncode', 'incomplete')} | `{result['log']}` |")
        lines.extend(["", "Commands and working directories are recorded in `results.json`.", ""])
        summary = "\n".join(lines)
        (self.directory / "summary.md").write_text(summary, encoding="utf-8")
        if os.environ.get("GITHUB_STEP_SUMMARY"):
            with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as target:
                target.write(summary)
        return 0 if success else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command or not args.name.replace("-", "").replace("_", "").isalnum():
        parser.error("A safe check name and command are required")
    report = Report(args.output, args.name)
    report.run(args.name, command, timeout=args.timeout)
    return report.finish()


if __name__ == "__main__":
    raise SystemExit(main())
