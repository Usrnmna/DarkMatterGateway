# Direct IR-service test harness

[Service tests](../test_ir_service.cpp) and [storage tests](../test_ir_storage.cpp) compile the actual [IR service](../../firmware/main/ir_service.cpp) through [service_fixture.hpp](../service_fixture.hpp) and link the actual portable IR core. Production functions are not replaced. The declarations in [sdk.hpp](sdk.hpp) cover the ESP-IDF 5.4.1 APIs consumed by this service; [support.cpp](support.cpp) supplies controlled SDK and FreeRTOS substitutes.

Run the complete host suite from the project root on Linux or WSL:

```bash
bash tools/test-host.sh
```

Each service binary also accepts one named group, for example:

```bash
./build/ci/host/binaries/ir_service/test teardown_fault_latching
./build/ci/host/binaries/ir_storage/test commit_failure_is_visible
```

## Controlled behavior

- Failure injection selects an API and its invocation count. The tests inspect returned errors, logs, stored records, transmitted symbols, and live resource counts.
- The captured task entry point runs the real worker. An empty fake queue raises a host-only exception outside the worker's critical section, ending that deterministic run. Hooks attempt competing operations while capture or transmission holds the service mutex. This does not simulate concurrent threads or establish race freedom.
- RMT substitutes retain DMA/callback pointers until channel deletion, reject premature frees, and read transmit data at completion. Tests assert output inhibition and both normal cleanup and deliberate retention after a teardown fault. Fixture cleanup then simulates power-off; it does not expose a recovery API to firmware.
- NVS substitutes distinguish currently visible values from a modeled durable snapshot. Commit failures can occur before or after copying that snapshot. Tests require error reporting in both cases and make no rollback guarantee. Independent literal `IRW1` records check storage contents; read-back mutations separately exercise CRC and waveform-field comparisons.

## Regression sensitivity

The optional [mutation check](../../tools/test-service-mutations.py) copies only the required sources into a temporary directory, verifies both unmodified baselines, and introduces one regression per compilation:

```bash
python3 tools/test-service-mutations.py
```

Each changed program must compile and fail a selected test assertion. Compiler failures, timeouts, unrelated crashes, and surviving mutations fail the check. Temporary copies are removed automatically; maintained firmware is never mutated. Normal host runs use AddressSanitizer and UndefinedBehaviorSanitizer; the mutation check uses assertions without sanitizers. The selected mutations establish sensitivity to those regressions, not exhaustive coverage.

These tests do not execute ESP-IDF drivers, reproduce the target ABI or ISR scheduling, inject every standard-library allocation failure, or verify physical RMT timing, optical output, flash retention, brownouts, Matter reset, or appliance responses. Those limits remain in the [validation report](../../docs/validation.md) and [hardware checklist](../../docs/remaining-work.md#physical-hardware-testing).
