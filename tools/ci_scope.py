"""Select CI work conservatively and enforce the aggregate result contract."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess


WORKERS = ("repository-checks", "python-tools", "cpp-sanitizers",
           "firmware-build", "regression-sensitivity")
FLAG_FOR_JOB = dict(zip(WORKERS, ("repository", "python", "cpp", "firmware", "mutations")))
SUITES = ("full", "python", "cpp", "ir", "proflame2", "app", "console", "firmware", "mutations")


def plan(event_name, event, changed):
    """Unknown comparisons run everything; only full runs may use the required-check name."""
    inputs = event.get("inputs", {}) if event_name == "workflow_dispatch" else {}
    suite = inputs.get("suite", "full")
    compiler = inputs.get("compiler", "both")
    if suite not in SUITES or compiler not in ("both", "gcc", "clang"):
        raise ValueError("Invalid diagnostic selection")
    diagnostic = suite != "full" or compiler != "both"
    heavy = changed is None or any(path.startswith(("firmware/", "config/", "tests/", "tools/", ".github/"))
                                   or not (path.startswith(("docs/", "hardware/")) or path.endswith(".md"))
                                   for path in changed)
    full = suite == "full"
    return {
        "repository": True, "python": full or suite == "python",
        "cpp": full or suite in ("cpp", "ir", "proflame2", "app", "console"),
        "firmware": (full and heavy) or suite == "firmware",
        "mutations": (full and heavy) or suite == "mutations",
        "diagnostic": diagnostic, "suite": suite,
        "compilers": ["g++", "clang++"] if compiler == "both" else ["g++" if compiler == "gcc" else "clang++"],
        "cold_cache": event_name == "schedule" or str(inputs.get("cold_cache", "false")).lower() == "true",
    }


def changed_files(event_name, event):
    """Compare tested HEAD with the event base, including deleted/renamed paths.

    A new branch, absent history, schedules, or manual dispatch selects a full
    run. A PR comparison uses its base against the checked-out merge commit.
    """
    if event_name == "pull_request":
        base = event.get("pull_request", {}).get("base", {}).get("sha")
    elif event_name == "push":
        base = event.get("before")
    else:
        return None
    if not isinstance(base, str) or not re.fullmatch(r"[0-9a-fA-F]{40}", base) or set(base) == {"0"}:
        return None
    result = subprocess.run(["git", "diff", "--no-renames", "--name-only", "-z", base, "HEAD", "--"],
                            capture_output=True)
    if result.returncode:
        return None
    return [path for path in result.stdout.decode("utf-8").split("\0") if path]


def gate(needs, diagnostic=False):
    """Reject failures, cancellations, absent results, and unauthorized skips."""
    scope = needs.get("change-scope", {})
    if scope.get("result") != "success":
        return ["change-scope did not succeed"]
    try:
        selected = json.loads(scope["outputs"]["plan"])
        if any(type(selected[flag]) is not bool for flag in FLAG_FOR_JOB.values()):
            raise ValueError("Invalid flags")
        if selected["repository"] is not True:
            raise ValueError("Repository checks are mandatory")
        if selected["diagnostic"] is not diagnostic:
            raise ValueError("Diagnostic/full-check identity mismatch")
        if not diagnostic and (not selected["python"] or not selected["cpp"]
                               or selected["suite"] != "full"
                               or selected["compilers"] != ["g++", "clang++"]):
            raise ValueError("Incomplete full verification plan")
    except (KeyError, TypeError, ValueError):
        return ["change-scope produced an invalid plan"]
    errors = []
    for job, flag in FLAG_FOR_JOB.items():
        result = needs.get(job, {}).get("result")
        accepted = ("success",) if selected[flag] else ("success", "skipped")
        if result not in accepted:
            errors.append(f"{job}: {result or 'missing'} (required={selected[flag]})")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("plan", "gate"))
    args = parser.parse_args()
    if args.command == "plan":
        event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text(encoding="utf-8"))
        name = os.environ["GITHUB_EVENT_NAME"]
        selected = plan(name, event, changed_files(name, event))
        print(json.dumps(selected, indent=2))
        if os.environ.get("GITHUB_OUTPUT"):
            with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
                output.write("plan=" + json.dumps(selected) + "\n")
                for key, value in selected.items():
                    output.write(f"{key}={json.dumps(value) if not isinstance(value, str) else value}\n")
        return 0
    errors = gate(json.loads(os.environ["CI_NEEDS"]), os.environ.get("CI_DIAGNOSTIC") == "true")
    summary = "## CI aggregate\n\n" + ("\n".join(f"- FAIL: {error}" for error in errors)
                                         if errors else "All selected checks passed.") + "\n"
    print(summary)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as output:
            output.write(summary)
    return int(bool(errors))


if __name__ == "__main__":
    raise SystemExit(main())
