# Continuous integration and failure diagnosis

The [Host checks workflow](../.github/workflows/host-checks.yml) runs one coordinated verification pipeline. Local commands use the same test and reporting code as hosted jobs. A passing run establishes software evidence for its tested revision and selected configurations; it does not establish hardware qualification.

## Jobs and triggers

| Job | Coverage | Execution policy | Timeout |
|---|---|---|---|
| `change-scope` | Compare the event base with tested HEAD and select required work | Every run | 5 min |
| `repository-checks` | Actionlint, shell syntax, configuration schemas, local documentation links, retained-source hashes, PDF exclusion | Every run | 10 min |
| `python-tools` | Python tests and installed serial-client import/help checks | Ubuntu 24.04 with Python 3.10, 3.12, 3.13; Windows 2022 with Python 3.12 | 10 min per entry |
| `cpp-sanitizers` | Fourteen C++ executions with GCC and Clang, AddressSanitizer and UndefinedBehaviorSanitizer | Every full run | 15 min per compiler |
| `firmware-build` | Actual pinned ESP32-S3 build, image/partition checks, size report, dependency-lock drift | Relevant changes or full manual/scheduled runs | 90 min |
| `regression-sensitivity` | Selected IR service/storage and Matter/startup mutations | Relevant changes or full manual/scheduled runs | 20 min per family |
| `portable-core-and-configuration` | Required-result aggregation | Every full run, including after upstream failures | 5 min |

Pushes and pull requests have no workflow-level branch or path filters. Changes under `firmware/`, `config/`, `tests/`, `tools/`, and `.github/` require both expensive checks. Documentation-only and hardware-reference changes retain the regular repository, Python, and C++ checks. Other unclassified files conservatively require expensive checks. Missing comparison history, new branches, and unknown event types select full verification. Renames include both old and new paths.

The weekly schedule runs Monday at 11:23 UTC with SDK-cache restoration disabled. This is 04:23 Pacific daylight time or 03:23 Pacific standard time. A scheduled run tests the default branch. Manual dispatch can select a suite, compiler, cold SDK cache, and verbose firmware output.

A manual selection narrower than `suite=full` and `compiler=both` produces **Diagnostic summary** instead of the required check name. Passing a focused run cannot substitute for the full check. The aggregate rejects failed, cancelled, absent, incomplete, and unexpectedly skipped jobs. Only an explicitly unnecessary job may be skipped. No required job uses `continue-on-error`, and matrix failures do not cancel independent entries.

## Local reproduction

Run commands from the repository root. Linux/WSL provides the C++ compiler and SDK environment; the Python-only command also runs natively on Windows.

```bash
# Existing full host entry point; no extra Python packages required.
bash tools/test-host.sh

# Match the Python CI lane, including the optional client dependency.
python -m pip install -r tools/requirements.txt
python tools/test_host.py --phase python --serial-smoke --output build/ci/python

# Select a compiler or family: all, ir, proflame2, app, console.
CXX=clang++-18 bash tools/test-host.sh --phase cpp --suite ir --output build/ci/clang-ir

# Mutation checks only modify disposable copies of required source files.
python tools/ci_report.py --name mutations-service --timeout 1080 --output build/ci/mutations-service -- python tools/test-service-mutations.py
python tools/ci_report.py --name mutations-app --timeout 1080 --output build/ci/mutations-app -- python tools/test-app-mutations.py

# Repository checks require bash, actionlint, and validation packages.
python -m pip install -r tools/ci-requirements.txt
python tools/ci_report.py --name repository --output build/ci/repository -- python tools/check_repository.py

# Actual target compilation; default appliance profiles remain disabled.
bash tools/bootstrap-sdk.sh
source tools/activate-sdk.sh
IR_BUILD_JOBS=4 python tools/ci_firmware.py
```

Ubuntu Clang requires `clang-18` and `libclang-rt-18-dev`; the workflow installs both and selects `clang++-18` explicitly so its compiler and sanitizer runtime match. The C++ runner keeps a separate binary for each configuration, including both console selections and both X0A target-boundary cases. Compilation and execution have separate logs. Independent suites continue after a failure, but the overall exit status remains nonzero. Sanitizer errors halt the affected executable with a failure status.

The firmware runner requires the revisions in [the build guide](build.md#dependencies). It creates a new ignored build directory, uses a build-local `sdkconfig`, and does not flash a device or create a release. Bootstrap and compilation have separate timeouts of 25 and 55 minutes, leaving time for diagnostic collection within the job limit. A narrower single-command reproduction is recorded in the corresponding result entry.

## Diagnostic output

Each reported command writes a log and an entry in `results.json` before starting. The entry records the argument list, working directory, duration, exit code, and status. Interrupted commands remain marked `running` and cannot satisfy report success. `environment.json` records the source revision, working-tree changes, platform, compiler, Python, and runner-image versions without dumping the process environment.

Python reports include test identities, tracebacks, skipped tests, and failed subtests in `python-tests.json`. Test discovery that finds no tests fails. Mutation logs retain the selected mutation, assertion, return code, and caught status. Mutation success establishes sensitivity to those selected faults, not exhaustive regression coverage.

GitHub job summaries provide a check/result table. Diagnostic uploads run after successful or failed test steps while the runner remains available. Artifacts have distinct job/matrix/attempt names and 14-day retention. Runner termination or failure before diagnostic setup can prevent an artifact; the GitHub job log remains the first source for setup failures.

Uploads are restricted to generated diagnostic directories. C++ artifacts contain test binaries and generated test headers; firmware artifacts contain logs, compilation commands, and ELF/map files when produced. The workflow does not upload private configuration directories, SDK caches, firmware flash packages, or retained PDFs. SDK/package versions and image hashes accompany target validation in its logs.

Download an artifact and start with `summary.md`, then find the failing entry in `results.json`. Reproduce with the recorded command from a source checkout at the tested revision and matching tools. Recorded absolute paths may need adjustment on another machine; uploaded test binaries are supplementary evidence and require a compatible Linux runtime.

## Dependency and reference integrity

Action versions are pinned to commit hashes. Actionlint is pinned to a release and verified against its SHA-256 hash. JSON schemas are fetched from immutable SchemaStore revisions listed in [the schema source manifest](../tools/ci-schemas.json), verified by SHA-256, and cached under ignored `build/ci-schemas/`. Repository validation requires network access when these schemas are not cached. YAML duplicate keys are rejected before schema validation.

SDK caching includes downloaded toolchains, package caches, component downloads, and GN. It excludes SDK source checkouts, Python virtual environments, and application build output. Cache keys include the OS, architecture, Python series, bootstrap/activation scripts, and managed-component lock. Pull requests restore caches but do not save them; successful default-branch pushes may save. A cold run bypasses restoration and cache saving.

The seven manufacturer PDFs remain ignored local references. CI verifies their exclusion, accepts their explicitly local-only documentation links, and neither downloads them nor requires their presence. All other retained files with manifest hashes must be publishable and match their recorded bytes. Upstream README snapshots stored as `.upstream.txt` are reference material rather than maintained Markdown guides. External web links are not fetched during documentation validation.

## Required-check setup and limits

After publication and a successful hosted run, the repository ruleset can require `portable-core-and-configuration`. Local files do not change GitHub branch protection. Focused diagnostic runs have a separate check name. A future merge queue requires adding `merge_group` to the workflow triggers before relying on the gate for queued merges.

Validation of the CI infrastructure includes failed subprocesses, timeouts, missing executables, failed Python assertions, malformed plans, incomplete full plans, and every worker's failure/cancellation/unexpected-skip state. Compiler and firmware failure probes belong in disposable source copies, never in maintained product code.

Local validation on 2026-10-28 passed 58 Python tests in each of Linux Python 3.10, 3.12, and 3.13 and native Windows Python 3.12; all 14 C++ executions with both GCC and Clang; and all 32 selected mutations. A fresh ESP32-S3 build passed SDK identity, size, image/partition, and dependency-lock checks. Deliberate repository, Python, compiler, sanitizer, and firmware errors produced failures with diagnostics. Firmware source was restored after the disposable probes, and its recovery build passed. SDK sources and downloaded tools were reused for this local validation. GitHub runner provisioning, cache transfer, artifact upload, scheduling, and a fully cold SDK download remain unverified for the expanded workflow.

Host stubs do not execute actual RTOS concurrency, driver timing, optical/RF transport, flash retention, or Matter networking. Target compilation establishes SDK/build compatibility, not physical operation. Device and appliance evidence remains governed by the [validation report](validation.md) and [bring-up procedure](bring-up.md).
