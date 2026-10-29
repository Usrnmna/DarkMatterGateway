# Validation evidence

## M1 and M2 local export verification, 2026-10-28

The [requirements record](delivery-requirements.md) completes M1 classification for the current scope. The [local exporter](../tools/export_artifacts.py) completes M2's defined development-package acceptance criteria. M3 is deferred and work is stopped; no provisioning implementation or production credential operation is included. S7/S8 remain unchanged.

### Results

| Check | Result | Evidence boundary |
|---|---|---|
| Exporter tests | 19 Python test methods passed, including parameterized failure cases | Synthetic images, partition tables, SDK metadata, and compiler boundary; no device credentials |
| Complete current host suite | 43 Python tests and 14 C++ test executions passed | Preserves the S1–S6 coverage; also includes the separately developed X0A tests present in the source snapshot |
| Fresh default target build | Passed; application 1,646,432 bytes, bootloader 20,880 bytes | Default appliance profiles and X0A configuration disabled; no flashing |
| Fresh synthetic light/fan target build | Passed; application 1,646,432 bytes, bootloader 20,880 bytes | Light/fan profiles enabled only as a compile fixture; no appliance compatibility claim |
| Final package verification | Both configurations passed standalone verification and `sha256sum --check` | Four flash images, exact file allowlist, metadata, source/SDK identity, image checksums, partition MD5, and region sizes checked |
| Repeatable packaging | Two final exports of each recorded build had identical file bytes | No claim of identical firmware bytes across independent builds or environments |
| Documentation and checklist | 322 local paths/anchors and formatting checks passed; 8 items complete, 22 open/deferred/conditional | S7/S8 text matches the pre-edit snapshot exactly |
| Firmware/layout preservation | Target source inputs match the staged firmware/configuration; M1/M2 change only tooling, tests, and documentation | Existing concurrent X0A work is preserved and identified separately; no GPIO, partition, hardware-role, or deployment-unit change is introduced here |

Each application fits both existing 6 MiB (`0x600000`) slots, with 4,645,024 bytes remaining per slot. The 3,072-byte partition table and 8,192-byte OTA initialization image retain the existing layout. No NVS or `ir_store` image is exported. All 14 C++ executions used warning checks, AddressSanitizer, and UndefinedBehaviorSanitizer; no sanitizer finding occurred. Expected error messages belong to negative tests.

Application SHA-256 values:

| Configuration | SHA-256 |
|---|---|
| Default | `ce07cf3f5f7060d171d2b77fb41463d09572b9c47ac3472114dc2c2f797ab3a1` |
| Synthetic light/fan | `92e7d29ec6accbab907bb170a3573c4ec406f8660efb7379dd574cd081e19c9f` |

### Failure-path evidence and corrections

Tests cover missing/unrecorded builds, source changes during and after compilation, changed SDK identity, modified outputs, duplicate JSON keys, unsafe paths and symbolic links, extra flash targets, role/flag/encryption mismatch, generated-profile mismatch, partition corruption, oversized/corrupt images, invalid OTA data, existing destinations, excluded private/unrelated files, package tampering, and deterministic re-export.

A focused failing regression reproduced acceptance of inconsistent source identity and malformed SDK revision metadata after recomputing the checksum list. The verifier now checks identity syntax and the source-map digest as well as payload integrity. Both regression cases pass. Checksums still do not authenticate a publisher or resist replacement of an entire valid package.

The initial synthetic test harness inherited an activated `IDF_PATH`, causing its deliberately fake project metadata to disagree with the environment. The fixture now isolates that path; the complete suite was rerun with the real SDK environment activated. An earlier host-run shell wrapper failed to retain its exit status after tests completed; the saved final runner records exit 0. These were tooling/test-harness corrections; no production firmware defect was found or fixed.

### Environment and evidence

Target builds used the existing pinned ESP-IDF v5.4.1 (`4c2820d377d1375e787bcef612f0c32c1427d183`), ESP-Matter (`4cedd4ad40821933d6cc237e80ab6604f2953fb0`), and connectedhomeip (`a6892983d6d761a9a787f2e11177a85d28bd1022`). Tracked SDK state was clean. Verification used Ubuntu 24.04.5 under WSL2, Python 3.12.3, host GCC 13.3.0, Xtensa GCC 14.2.0, CMake 3.30.2, and Ninja 1.11.1. Build/export commands are in the [export guide](artifact-export.md).

Ignored evidence is retained under `build/verification-m1-m2-2026-10-28/`:

- `current/staged-inputs.json`, `host-inputs.json`, native stage path records, and `current/environment.log` identify the final target and host snapshots, each containing 105 maintained inputs.
- `current/target-default.log`, `current/target-light-fan.log`, `current/receipt-*.json`, and `current/export-*.exit` record fresh actual SDK builds using the final exporter.
- `current/packages/default/`, `current/packages/light-fan/`, and their `-copy` counterparts contain the final packages. Their manifests record build-input and exporter identity; `current/exports.log` records independent checks and repeated-byte comparisons. Earlier successful builds/exports remain in the parent evidence directory as superseded snapshots.
- `export-tests-red.log`, `provenance-regression-red.log`, `export-tests-final.log`, `activated-sdk-test-first.log`, `host-checks-final.log`, and `host-checks-final.exit` preserve the failed-before-fix and passing evidence. The complete final host log is authoritative for the final test fixture.
- `verification-manifest.json` and `documentation-checks.json` record source/evidence hashes, results, link/format checks, checklist counts, and unchanged S7/S8 text.

An initial successful target snapshot preceded a concurrent X0A framing-error handling change. Both target configurations were subsequently rebuilt in a new isolated stage using the final exporter and updated source. The final host suite and target packages cover that updated snapshot; the separate X0A implementation is preserved without modification by M1/M2.

M1/M2 execution performed no repository initialization/publication, hosted CI, commit, push, or release operation. During final checks, concurrent workspace activity introduced Git metadata and a `LICENSE` file: observed HEAD `a8a9162f7c27845c1bd3c01737afa33ba1806939` contained only `LICENSE`, an `origin` remote existed, and project directories were untracked. Read-only inspection recorded this state; it was left untouched. Package identity remains based on recorded source hashes, not that unrelated commit.

No device was flashed, provisioned, or physically tested. Development identities, physical startup and retention, IR/RMT timing, commissioning, networking, and appliance response retain their existing limits. M4–M7 implementation is outside this task.

## Wired X0A software verification, 2026-10-28

The [X0A software](proflame2/software.md) adds the portable codec/controller/session, strict configuration, dedicated UART service, explicit console routing and atomic reset quiescence. The default production configuration remains disabled. No hardware, serial port, accessory, appliance, or archived upstream controller was operated.

| Check | Result | Evidence boundary |
|---|---|---|
| Extended host suite | 43 Python tests and 14 C++ test runs passed with existing IR/Matter and concurrent artifact-export coverage retained | Synthetic clocks, replies and SDK/transport boundaries; C++ AddressSanitizer/UndefinedBehaviorSanitizer enabled |
| Protocol/controller | Literal six-byte examples; malformed replies; all six field masks; denial/loss, stale baseline, uncertain writes, no retry, busy rejection and read-only recovery exercised | Does not establish physical framing, response timing or handset precedence |
| Session/transport | Partial/extra bytes, inter-byte timeout, read errors, short TX, failed initialization and sticky quiescence exercised | Fake UART; electrical inhibit and peripheral behavior remain untested |
| Actual UART adapter | Framing-error interrupt enabled explicitly; setup/cleanup failures, frame/parity/overflow events and TX errors exercised | Host-only synthetic pins and SDK calls; no production pin allocation |
| Target activation boundary | Actual service exercised with generated disabled constants and a host-only forged activation assertion; neither makes UART/GPIO/worker calls | Physical board constants remain false/unassigned; no synthetic production profile created |
| Console/startup/reset | Both console selections, exact grammar, disabled/busy/unsupported results, optional-service failure and no automatic requests exercised | SDK scheduling and physical reset are substituted |
| ESP32-S3 target compilation | Passed; application image 1,646,640 bytes (`0x192030`), with 74% of the existing 6 MiB application slot free | Default IR/X0A configurations disabled; no enabled target profile, flashing or hardware validation |

A fresh code review identified the pinned SDK's default omission of the framing-error interrupt. The adapter regression test failed before the fix and passed afterward; the corrected four X0A sources also passed syntax checks using the configured ESP32-S3 compiler. The existing Matter/startup mutation harness retained all 18/18 detections after adding its new header-staging dependencies. The review's stale plan-only completion paragraph was corrected. Physical qualification remains outside these checks.

Host command: `bash tools/test-host.sh`. Full target compilation used `idf.py -C <Linux-copy>/firmware -DIDF_TARGET=esp32s3 build` after copying the firmware, configuration, generators, pinned SDKs and toolchains into a unique Linux cache directory. Every maintained firmware/configuration/generator input compared byte-for-byte with the workspace after compilation. SDK revisions remained ESP-IDF `v5.4.1`, ESP-Matter `4cedd4ad40821933d6cc237e80ab6604f2953fb0`, and CHIP `a6892983d6d761a9a787f2e11177a85d28bd1022`.

Logs, pre-edit snapshots, the copied disabled image/ELF/generated header, source hashes and `verification-manifest.json` are retained under ignored `build/x0a-implementation/`. `linux-build-path.txt` identifies the retained Linux build directory. The Windows-mounted build was interrupted for excessive filesystem overhead; an initial Linux-copy attempt lacked cached SDK constraints and failed before configuration. Those diagnostic logs are retained separately from the successful `target-linux.log`. Existing upstream `SEC_CERT_DAC_PROVIDER` configuration warnings remain; no dependency change was made to suppress them.

Concurrent artifact-export/documentation changes and newly present Git metadata were preserved. This implementation performed no project Git initialization, commit or push. The separate [hardware qualification record](proflame2/hardware-qualification.md) keeps G0–G5 pending. No enabled target build is qualified, and successful host checks cannot populate the empty reviewed-profile registry.

The codec is based on [S14](proflame2/sources.md#s14) and [S15](proflame2/sources.md#s15), with no wholesale code import. Local operation IDs do not correlate wire replies. Arbitrarily late valid responses and the ownership-check/write race remain physical validation limits, not guarantees established by this test suite.

## S5 and S6 verification, 2026-10-28

The [remaining-work checklist](remaining-work.md#software-verification) items S5 and S6 completed with direct host tests of `matter_app.cpp` and `app_main.cpp`. The actual adapter, startup sequence, button task, and portable IR core execute against controlled dependencies. No production firmware fix was required. The architecture remains unchanged; changes are limited to tests, test tooling, and documentation.

### Results and coverage

| Check | Result | Evidence boundary |
|---|---|---|
| Complete host suite | 17 Python tests, default profile generation, and eight C++ binaries passed | Existing core/service/storage/console checks plus two new suites |
| [Matter adapter tests](../tests/test_matter_app.cpp) | Ten named groups passed, including parameterized mappings and failures | Actual adapter and IR core; SDK attribute/endpoint behavior and IR enqueue boundary are substituted |
| [Startup/recovery tests](../tests/test_startup.cpp) | Eight named groups passed, including parameterized failure and discovery cases | Actual startup, button task, and Matter adapter; IR initialization, terminal startup, and SDK execution are substituted |
| [Selected mutation checks](../tools/test-app-mutations.py) | Both original baselines passed; 18 of 18 deliberate regressions compiled and failed test assertions | Isolated temporary copies; not exhaustive branch or mutation coverage |
| Production-input integrity | Firmware, configuration, and hardware-data hashes match the pre-S5/S6 snapshot and recorded S1/S2 inputs | No target code changed; target builds were not rerun |

All eight C++ binaries used `-std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g`. No sanitizer finding occurred. Expected console error output belongs to negative tests; the full script exited 0. The optional mutation check uses assertions and the same warning settings without sanitizers. Compiler errors, timeouts, unrelated crashes, and surviving mutations fail verification.

S5 covers enabled/disabled/native-kind endpoint selection, aggregator parent links and labels, initialization suppression of IR callbacks, the supported eight-profile limit, and node/endpoint/cluster-add/parent/label/start failures. Literal expectations check light ON/OFF routing, fan percentage boundaries, supported mode normalization, unsupported modes, nullable-null rejection, three dependent fan attributes, recursion suppression, deduplication at the 100 ms boundary, and independence between endpoints. Queue/storage/allocation errors returned by the IR boundary reject PRE_UPDATE without changing the modeled attribute database; a rejected request does not enter the duplicate cache. Reporting failures are logged, and the synchronization guard is released so subsequent commands remain usable. Identification controls only the status LED in the exercised path.

S6 covers startup ordering; NVS, IR initialization, GPIO, Matter-start, and terminal failures; initial LED configuration; and button-task creation failure with console commissioning still available. Scheduled operations execute only when explicitly dispatched by the harness. Tests check scheduling failures, 300-second windows, Wi-Fi-dependent discovery, BLE/network pairing flags, existing-window deadline retention, enhanced-window pairing-code handling, window-open failure, reset requests while unready/busy, and SDK reset error reporting. Sampled button sequences exercise short presses, one action per held press, release/rearm, and failed scheduling without repeated requests during the same hold.

The exercised startup, button, commissioning, and reset-request paths make no IR replay or direct storage-erase calls. Explicit factory reset remains an SDK request; its implementation and physical effects are not executed by this harness. The actual IR service's initialization and storage behavior remain covered separately by S3/S4.

The 18 mutations exercise startup callback suppression, disabled-profile filtering, parent links, command zones, nullable rejection, synchronization-guard release, deduplication boundaries, queue-error propagation, rejected-request caching, fatal initialization handling, automatic erasure, discovery selection, existing/enhanced windows, busy-reset rejection, recovery replay, held-button repetition, and release/rearm. One initial mutation run stopped because two unused fixture slots lacked names and the deliberately broken profile filter crashed before an assertion. Fully specified disabled fixtures replaced those slots; the final run caught all 18 mutations through assertions. Maintained firmware was never mutated.

### Environment, reproduction, and retained evidence

Verification used Ubuntu 24.04.5 LTS under WSL2, Python 3.12.3, and GCC 13.3.0. From the project root:

```bash
bash tools/test-host.sh
python3 tools/test-app-mutations.py
```

The [harness guide](../tests/app_stubs/README.md) describes projected SDK declarations, mutable synthetic profiles, nested callbacks, fatal-error and task-stop exceptions, scheduling, and boundaries. PRE_UPDATE → database write → POST_UPDATE follows the pinned SDK's documented contract; a rejected PRE_UPDATE suppresses both the modeled write and POST_UPDATE. This checks application behavior under that contract rather than testing the real Matter database or fan delegate.

Local evidence is retained under ignored `build/verification-s5-s6-2026-10-28/`:

- `host-checks.log`, `host-environment.log`, and `mutations.log` retain final output and environment details. `host-first.log` and `mutations-first.log` retain the earlier runs.
- `inputs-before.json`, `verification-manifest.json`, `matter_app.before.cpp`, and `app_main.before.cpp` retain source identity, input comparisons, result counts, original production sources, and hashes of final test/document inputs and binaries.
- `documentation-checks.json` records local-link/anchor and formatting checks for affected documents and new test/tool files.

Unchanged production source identities:

| File | SHA-256 |
|---|---|
| `firmware/main/matter_app.cpp` | `70050242d6996e5d78a399d10765379d060d2bb8bca3543ab8ea9fd704d49a22` |
| `firmware/main/app_main.cpp` | `29d3b0b7cfec83273e5f15e4b4642a4b228c8576e85ab4c1ecdebf01b8a268c6` |

Project Git metadata remains absent; file hashes identify the tested source. Earlier target-build exports remain separate and were not regenerated or replaced.

### Remaining limits

These results close S5 and S6 at the stated host-test scope. They do not execute real ESP-Matter/CHIP networking, controller commissioning, the target attribute database/fan delegate, ESP-IDF drivers, FreeRTOS concurrency, or physical reset/reboot behavior. They do not establish race freedom, including changes in IR activity after scheduling a reset request. Electrical startup inhibition, physical button timing, flash retention through reset/interruption, radio/optical delivery, and appliance response remain under H3–H8. Factory-reset partition retention requires H6. No target was flashed, hosted CI was not run, and S7/S8 remain open.

## S3 and S4 verification, 2026-10-28

The [remaining-work checklist](remaining-work.md#software-verification) items S3 and S4 completed with direct tests of the actual IR service and storage orchestration. The tests include `ir_service.cpp` and link the real IR core; only ESP-IDF and FreeRTOS dependencies are substituted. No production firmware fix was required. Firmware, configuration, architecture, targets, GPIO assignments, and partition layout remain unchanged.

### Results and coverage

| Check | Result | Evidence boundary |
|---|---|---|
| Complete host suite | 17 Python tests, default profile generation, and all six C++ binaries passed | Existing checks plus two new direct service/storage suites; no complete ESP32 emulation |
| [Direct IR service](../tests/test_ir_service.cpp) | Ten named groups passed, including parameterized failure cases | Actual capture, queue, worker, and transmission code with controlled GPIO/RMT/FreeRTOS APIs |
| [Storage failure paths](../tests/test_ir_storage.cpp) | Ten named groups passed, including parameterized failure cases | Actual NVS orchestration and record codec with modeled API failures and persistence outcomes |
| [Selected mutation checks](../tools/test-service-mutations.py) | Both unmodified baselines passed; 14 of 14 deliberate regressions compiled and triggered test assertions | Temporary source copies; sensitivity to selected regressions, not exhaustive mutation or branch coverage |
| Firmware/configuration integrity | Input hashes match the pre-S3/S4 snapshot and the recorded S1/S2 build inputs | No target code changed; target builds were not rerun for this test-only change |

All six C++ binaries used `-std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g`. No sanitizer finding occurred. Expected console error messages exercise negative cases and do not indicate a suite failure. The optional mutation runner uses the same warning settings and assertions without sanitizers; compile failures, timeouts, unrelated crashes, and surviving mutations do not count as successful checks.

The service suite covers:

- Uninitialized/invalid requests; synthetic carrier, 38 kHz, and 56 kHz captures; RAM-only learning until explicit save; idle rearming; incomplete/full/wrong-buffer/idle-low rejection; timeout and decoder-error propagation; pending-data invalidation after a failed capture.
- A four-job limit, failed queue insertion accounting, capture exclusion while work is queued, competing requests while capture/transmission holds the mutex, independent queued data after record replacement, and preservation of pending data across repeated sends.
- Zone routing, carrier/duty propagation, long-span symbol packing, bounded repeats and gaps, allocation/driver/output-enable/completion failures, error logs without automatic retry, and capacity recovery after completed or failed jobs.
- Output inhibition before teardown, normal resource cleanup, and buffer/callback/encoder retention after RX or TX channel teardown failure. A latched fault rejects new hardware work and prevents subsequent queued jobs from reaching the TX driver.

The storage suite covers initialization failures, explicit overwrite selection, query/write/commit errors, read-back size/data errors, CRC rejection, independent carrier/duty/duration mismatches, malformed/oversized records, missing records, erase/commit failures, and iterator cleanup. Errors remain visible; unrelated records remain intact and no automatic erasure occurs in the exercised paths. A failed commit is tested both before and after a modeled durable write. Neither outcome establishes a rollback guarantee or actual flash persistence.

The 14 selected regressions remove queue bounds, failed-insertion accounting, queued-capture exclusion, pending-copy ownership, teardown ownership protection, fault latching, worker error reporting, overwrite confirmation, commit error propagation, read-back API use, each of the three waveform-field comparisons, or the prohibition on automatic storage erasure. Each corresponding assertion failed in an isolated temporary build. Two early mutation-runner match-count guards stopped verification because expressions occurred in multiple functions; the source matches were narrowed and the complete check then passed. The maintained firmware was never mutated.

### Environment, reproduction, and retained evidence

Verification ran under Ubuntu 24.04.5 LTS in WSL2, with Python 3.12.3 and GCC 13.3.0. From the project root:

```bash
bash tools/test-host.sh
python3 tools/test-service-mutations.py
```

The [harness guide](../tests/service_stubs/README.md) describes failure injection, named-group execution, resource ownership checks, and deterministic scheduling. It uses a host-only exception to stop the real worker when its fake queue becomes empty, outside the worker's critical section. Fixture teardown reclaims deliberately retained fault resources only after assertions, modeling power-off rather than firmware recovery.

Local evidence is retained under ignored `build/verification-s3-s4-2026-10-28/`:

- `host-checks.log`, `host-environment.log`, and `mutations.log` record final results and the execution environment. `host-first.log` and `mutations-first.log`/`mutations-second.log` retain earlier diagnostic runs.
- `inputs-before.json`, `verification-manifest.json`, and `ir_service.before.cpp` record source identity, input comparisons, final test/document hashes, result counts, and the original service implementation.
- `documentation-checks.json` records local-link/anchor and formatting checks for affected documents and new test/tool files.

The unchanged `firmware/main/ir_service.cpp` SHA-256 is `ab16fddf8032b96807583d5afb9c98faf9c2fbc2a1d22ed7ba7809f0d830e7a5`. Project Git metadata remains absent, so evidence uses file hashes rather than a commit. The earlier S1/S2 target-build artifacts remain separate and were not regenerated or replaced.

### Remaining limits

These results close S3 and S4. The substitutes do not execute ESP-IDF drivers, reproduce the target ABI, schedule concurrent threads/interrupts, establish race freedom, or inject every standard-library allocation failure. Real RMT/DMA timing, optical output, physical flash retention, interrupted writes, brownouts, reset isolation, appliance responses, and controller/network operation remain unverified. At this stage S5/S6 remained open; their subsequent Matter-adapter and application-startup results are recorded above. No target was flashed and hosted CI was not run.

## S1 and S2 verification, 2026-10-28

The [remaining-work checklist](remaining-work.md#software-verification) items S1 and S2 completed successfully against the current source. No test or target-build failure required a source fix. The architecture, firmware targets, GPIO assignments, partition layout, default appliance profiles, and workspace SDK configuration are unchanged.

### Results

| Check | Result | Evidence boundary |
|---|---|---|
| Python unit tests | 17 passed; no failures | Hardware-table/partition consistency, profile validation, and mocked serial-client behavior |
| Default profile generation | Passed; all four profiles disabled | Configuration parsing and generated constants; no appliance qualification |
| C++17 IR core tests | Passed with AddressSanitizer and UndefinedBehaviorSanitizer | Synthetic capture decoding, bounds, record corruption rejection, and fan mapping |
| Console command-handler tests | USB and UART selections both passed with both sanitizers | Actual handlers with SDK/service substitutes |
| Console line-reader tests | Passed with both sanitizers | Actual bounded reader and probe wrapper using host streams |
| Default ESP32-S3 build | Exit 0; application and bootloader generated | Fresh build directory; disabled appliance profiles |
| Synthetic light/fan ESP32-S3 build | Exit 0; application and bootloader generated | Separate fresh build directory; only light and fan profiles enabled |
| Artifact inspection | Passed for both builds | Generated profile contents, application-slot fit, ELF wrapper symbols, linker cross-references, relative flash-file paths, and SHA-256 hashes |

The four C++ test binaries were built with `-Wall -Wextra -Werror -pedantic` and `-fsanitize=address,undefined`. Expected `ERROR HOST_TEST_ERROR` messages came from negative console cases; the test processes and the complete host script exited successfully.

### Environment and method

Verification used Ubuntu 24.04.5 LTS under WSL2, kernel `5.15.153.1-microsoft-standard-WSL2`, Python 3.12.3, and host GCC 13.3.0. Target builds used Xtensa GCC 14.2.0 (`esp-14.2.0_20241119`), CMake 3.30.2, and Ninja reporting `1.11.1.git.kitware.jobserver-1`.

| SDK | Verified Git HEAD |
|---|---|
| ESP-IDF v5.4.1 | `4c2820d377d1375e787bcef612f0c32c1427d183` |
| ESP-Matter | `4cedd4ad40821933d6cc237e80ab6604f2953fb0` |
| connectedhomeip | `a6892983d6d761a9a787f2e11177a85d28bd1022` |

S1 ran from the project workspace:

```bash
bash tools/test-host.sh
```

S2 used a temporary Linux-filesystem source copy at `/var/tmp/matter-ir-s1-s2-idj0ppvr`, reusing the existing pinned Linux-local SDK cache. SHA-256 comparisons verified all 38 recorded firmware, configuration, tooling, test, and hardware-data inputs before staging and after both builds. The workspace and staged inputs remained identical. The existing workspace `firmware/sdkconfig` was also compared byte-for-byte and remained unchanged. Project Git metadata is absent; source identity is recorded through the input hashes rather than a commit.

The temporary `config/light-fan.fixture.json` enables only the light and fan profiles and supplies synthetic model/verification metadata for compilation. Fireplace and A/C profiles remain disabled. The fixture is retained with the local evidence, not installed as `config/devices.local.json`. It establishes no appliance compatibility or physical verification.

After activating `tools/activate-sdk.sh` in the staged copy, the following commands ran from its `firmware/` directory with `IR_BUILD_JOBS=8`. Each build generated its own SDK configuration from the maintained defaults:

```bash
idf.py -B build-default -DIDF_TARGET=esp32s3 \
  -DSDKCONFIG="$PWD/sdkconfig-default" \
  -DPROFILE_CONFIG="$PWD/../config/devices.json" build

idf.py -B build-light-fan -DIDF_TARGET=esp32s3 \
  -DSDKCONFIG="$PWD/sdkconfig-light-fan" \
  -DPROFILE_CONFIG="$PWD/../config/light-fan.fixture.json" build
```

Both generated `profiles.hpp` files match their selected JSON inputs. Both ELFs retain `__wrap_linenoise` and `__wrap_linenoiseProbe`; the linker cross-reference tables show references from `libconsole.a(esp_console_common.c.obj)` to the application wrappers.

### Artifacts and retained evidence

| Configuration | Application bytes | Application SHA-256 |
|---|---:|---|
| Default, four disabled profiles | 1,639,392 (`0x1903e0`) | `8cb9aba0bcac6ef6b914914278078acae454340b32d56f9cc72d38c4dcb9d900` |
| Synthetic light/fan enabled | 1,639,392 (`0x1903e0`) | `f7c1a3e2ff63046398eee1a36ca8782455efc486cb691e37156289670f3d6722` |

Each application fits the 6 MiB (`0x600000`) slot with 4,652,064 bytes (`0x46fc20`) remaining. Each bootloader is 20,880 bytes. The binary partition tables confirm the two 6 MiB application slots. Generated flash metadata uses relative paths to existing exported files.

Local evidence is retained under ignored `build/verification-2026-10-28/`; it is not a published release or part of the maintained source distribution:

- `host-checks.log`, `host-environment.log`, `target-environment.log`, and the two `target-*.log`/`target-*.exit` pairs record the commands' results and environment.
- `source-inputs-before.json` and `verification-manifest.json` record the 38 input hashes, SDK revisions, profile selections, artifact hashes/sizes, and completed integrity checks.
- `default-profile.json` and `light-fan.fixture.json` retain the tested configuration inputs.
- `default/` and `light-fan/` contain their respective application BIN/ELF/map, bootloader, partition table, OTA initialization data, flash arguments, effective SDK configuration, generated profile header, and console-wrapper evidence.
- One-off preparation, build, and artifact-check scripts are retained alongside the evidence. They do not constitute maintained release automation.

The earlier `build/artifacts/` exports were not replaced. Matching image sizes do not imply identical binaries; the hashes above distinguish the two new builds. Bit-for-bit reproducibility across repeated builds was not tested.

### Warnings and remaining limits

The restricted execution environment initially denied WSL access; verification ran successfully after the required execution access was granted. Build warnings remain in the pinned upstream SDK, including Kconfig choice definitions and C++ compiler warnings. The Python-environment warning reflects two paths resolving to the same reused environment through a symlink. No SDK source or dependency version was changed to suppress warnings.

These results close S1 and S2 only. At this stage, direct IR-service, storage-failure, Matter-integration, and startup coverage in S3–S6 remained open; the subsequent S3/S4 results are recorded above. No board was connected or flashed, no radio/optical transmission was performed, and no physical storage, appliance, controller, commissioning, or network-recovery behavior was validated. Hosted CI was not run.

## README visual checks, 2026-10-17

The README includes a local SVG overview of the proposed control paths, a prominent unbuilt-hardware notice, section navigation, and a task-based documentation table. The diagram represents component roles rather than a physical prototype.

- All 76 local links, image references, and section anchors across the 12 Markdown documents resolve. Fenced blocks, final newlines, and trailing whitespace checks pass.
- The SVG parses as XML and includes an accessible title, description, and Markdown alternative text. It contains no scripts, external resources, or embedded device photographs.
- A 1200 × 540 raster preview was rendered and visually inspected for readable labels, unclipped text, and clear arrows. The GitHub-hosted README rendering was not tested.
- This revision changes only README presentation, its diagram asset, and this validation record. Firmware, configuration, and executable behavior are unchanged; no software tests, target builds, or physical tests were rerun for the visual revision.

## Documentation and comment checks, 2026-10-17

The review covered all project-maintained Markdown documents, source comments and docstrings, script/configuration annotations, and hardware-table notes. Downloaded SDKs, managed components, caches, and generated build output were excluded as upstream or derived material. The resulting documentation set contains 12 Markdown files, including the documentation index.

- All 73 local Markdown links in the initial documentation revision, including section anchors, resolved. Fenced blocks, final newlines, and trailing whitespace checks passed. External URLs and rendered Mermaid layout were not validated in that review.
- A file comparison against a 52-file pre-edit baseline confirms byte-identical firmware, configuration, hardware schedules, and tooling. The sole source-file edit replaces a hardware-test comment; its parsed Python syntax tree is unchanged.
- `python -m unittest discover -s tests -p "test_*.py" -v` passes all 17 tests. Initial sandboxed runs encountered Windows temporary-directory permission errors; the run with the required execution permission passed.
- `python tools/profile_config.py config/devices.json` passes with all four profiles disabled.

No firmware logic or architecture changed. C++ sanitizer tests and ESP32-S3 builds were not rerun for these documentation/comment edits. The software-build evidence below remains dated 2026-10-16; no additional physical qualification is claimed.

## Completed software checks

Checks run on 2026-10-16 from the project workspace:

| Check | Result | Scope |
|---|---|---|
| Python unit tests | 17 passed | Strict profile parsing, mocked serial-client input/write behavior, BOM reference counts, power-switch pin contract, GPIO agreement, partition alignment and isolation |
| C++17 core tests | Passed with AddressSanitizer and UndefinedBehaviorSanitizer | Capture decoding, final carrier pulse timing, quantized carrier-band boundaries, malformed pulse rejection, waveform bounds, versioned record encoding, CRC corruption rejection, fan command selection |
| Console command handlers | USB and UART selections passed with both sanitizers | Actual command handlers compiled against SDK/service stubs; numeric bounds, explicit confirmations, service failures, disabled aliases, and REPL creation failure |
| Console line reader | Passed with both sanitizers | Actual bounded reader; line limits, invalid/control characters, cancellation, editing, CR/LF termination, incomplete input rejection, and probe wrapper |
| Default profile validation | Passed; four disabled profiles | Light, fan, gas fireplace, portable A/C configuration |
| SDK shell scripts | Bash syntax and environment activation passed | Local dependency setup and exported tool paths |

Portable checks run with `bash tools/test-host.sh`. All four C++ test binaries use `-Wall -Wextra -Werror -pedantic` and both sanitizers. These checks execute host code; they do not emulate flash recovery, the Matter stack, or peripheral hardware. Expected error messages from negative console tests do not indicate suite failures.

## Target compilation

ESP32-S3 compilation and linkage passed using ESP-IDF v5.4.1, Xtensa GCC 14.2.0, and ESP-Matter commit `4cedd4ad40821933d6cc237e80ab6604f2953fb0`.

| Configuration | Result |
|---|---|
| Default four disabled profiles | Bootloader, application, and flash images generated successfully |
| Temporary light/fan activation fixture | Application compiled and linked with both native endpoint types enabled |

The activation fixture used synthetic model metadata solely for compilation. It provides no evidence of appliance compatibility. Exported images use the default disabled profiles. Final artifact hashes and relative flash-file paths were checked; the linker cross-reference table resolves SDK console input and probing to the application wrappers.

The application image is 1,639,376 bytes (`0x1903d0`), within its 6 MiB application slot. The bootloader image is 20,880 bytes. Compilation used a temporary native Linux build copy after Windows-mounted WSL file access slowed SDK processing. Firmware source hashes were compared with the workspace before compilation and artifact export. Both target linker maps retain the console wrappers.

Default artifacts are retained in ignored `build/artifacts/`. `build-manifest.json` records SHA-256 values for 20 source/configuration inputs, exact SDK revisions, and all exported artifacts. Application SHA-256:

```text
c2e1901b2f8a7ce60e9d35fb72914cc5b89d626e8071aa009fb2c5af4675c44e
```

SDK warnings occurred in upstream sources. Compilation success establishes toolchain and link compatibility; startup, commissioning, peripheral behavior, and appliance control remain untested on hardware.

## Review corrections

- Carrier decoding no longer counts the final OFF interval twice when capture ends on a short HIGH sample. The regression fixture previously decoded a 1,000 µs mark as 1,017 µs.
- Console input uses a bounded reader without ANSI probing. Invalid lines cannot execute a valid prefix, and stored-command inspection and listing propagate service errors.
- Profile parsing rejects duplicate JSON keys and nonstandard numeric constants. Serial commands reject invalid input before opening a port and do not retry incomplete writes.
- The main power switch is TPS2553DBVR with a 21.5 kΩ current-limit resistor. The previous unprotected switch relied on a fuse whose trip current exceeded the switch rating. BOM, pin connections, startup calculations, and bench procedures use the replacement circuit. Datasheet review and connection tests establish consistency; electrical protection remains subject to measurement.

## Physical verification

No physical hardware is available for testing. No board has been connected or flashed. The circuit package contains wiring tables, component selections, calculations, and layout requirements. Schematic capture, PCB layout, ERC/DRC, assembly inspection, and electrical qualification remain outstanding.

The [bench procedure](bring-up.md) covers power negotiation, rail startup, GPIO reset behavior, emitter currents, thermal conditions, carrier accuracy, receiver noise, appliance operation, Matter commissioning, and power-interruption recovery. Each result requires a board identifier, firmware revision, test conditions, and measured evidence.

Flash persistence follows the dedicated NVS partition and commit/read-back implementation. Host record tests do not establish retention on a physical flash device. Successful local transmission does not establish optical reception or appliance response. Matter attributes represent requested state without independent appliance feedback.

GitHub Actions is configured for portable checks. No hosted workflow run has occurred in this workspace.
