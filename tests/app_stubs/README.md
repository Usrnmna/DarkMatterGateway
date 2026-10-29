# Matter and startup test harness

[Matter tests](../test_matter_app.cpp) compile the actual [Matter adapter](../../firmware/main/matter_app.cpp) and portable IR core. [Startup tests](../test_startup.cpp) additionally compile the actual [application entry point and button task](../../firmware/main/app_main.cpp). The [fixture](../app_fixture.hpp) resets private adapter state only between cases; no production reset API is added.

Run the full host suite on Linux or WSL:

```bash
bash tools/test-host.sh
```

The new binaries accept one named group for focused reproduction:

```bash
./build/test_matter_app queue_rejection_preserves_state_and_retry
./build/test_startup startup_stops_on_initialization_failure
```

## Controlled dependencies

[SDK declarations](sdk.hpp) project the interfaces consumed from ESP-IDF 5.4.1 and the [pinned ESP-Matter baseline](../../docs/sdk-reference.md). The common ESP/FreeRTOS scalar declarations come from the [service harness](../service_stubs/sdk.hpp). They are host declarations, not target ABI or complete SDK implementations.

[Support code](support.cpp) records endpoint creation, parent links, labels, attribute values, requested IR actions, GPIO configuration, and queued stack work. Failure injection selects an API and invocation count. The [mutable profile fixture](profiles.hpp) supplies eight synthetic profiles; production configuration remains immutable and generated.

- Attribute writes follow the SDK's documented PRE_UPDATE → database write → POST_UPDATE sequence. Rejected PRE_UPDATE callbacks preserve the old value and suppress POST_UPDATE. Dependent fan reports call the registered callbacks again, exercising the real recursion guard.
- The IR-service boundary records requested names, zones, repeats, and gaps and can reject enqueue requests. The actual IR worker is covered separately by the [direct service suite](../service_stubs/README.md); this harness does not execute it.
- Stack work remains queued until explicitly dispatched. Commissioning-window state and scheduling errors are controlled inputs. Factory reset is recorded as an SDK request; it does not erase a simulated flash device or prove partition retention.
- Startup uses the real Matter initialization path, with IR initialization and terminal startup substituted. `ESP_ERROR_CHECK` throws a host-only fatal-error object to preserve stop-on-first-error flow. Tests catch it without terminating the entire binary.
- Button input follows a supplied sequence of sampled GPIO levels. The actual task runs until a host-only exception ends the final delay. This checks sample-count behavior, held-button suppression, release/rearm, and error handling without claiming physical debounce or FreeRTOS timing accuracy.

SDK-owned fake endpoints are reclaimed at simulated power-off between cases. Production cleanup/retry behavior is not inferred from that fixture teardown.

## Regression sensitivity and limits

The optional [mutation check](../../tools/test-app-mutations.py) verifies both original baselines, changes one contract per temporary build, and requires a failing test assertion:

```bash
python3 tools/test-app-mutations.py
```

Compiler failures, timeouts, unrelated crashes, and surviving mutations fail verification. Maintained firmware is never mutated. Normal host tests use AddressSanitizer and UndefinedBehaviorSanitizer; selected mutation checks use assertions without sanitizers.

The harness does not execute CHIP networking, the actual Matter attribute database or fan delegate, ESP-IDF drivers, real RTOS concurrency, reset/reboot behavior, flash retention, controller commissioning, radio/optical transport, or appliance response. It does not establish race freedom, including changes in IR activity after a reset request is scheduled. Those limits remain explicit in the [validation report](../../docs/validation.md) and [physical checklist](../../docs/remaining-work.md#physical-hardware-testing).
