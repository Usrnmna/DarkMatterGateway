# Build and programming

## Dependencies

| Dependency | Selected revision |
|---|---|
| ESP-IDF | `v5.4.1` |
| ESP-Matter | `4cedd4ad40821933d6cc237e80ab6604f2953fb0` |
| connectedhomeip | `a6892983d6d761a9a787f2e11177a85d28bd1022`, selected by ESP-Matter |
| Host tests | Python 3.10 or later; C++17 compiler |
| Optional serial client | `pyserial==3.5` |

Linux or Ubuntu WSL is the reference build environment. Keep SDKs outside tracked project source. The selected SDK versions and API contracts are linked in the [ESP-Matter SDK reference](sdk-reference.md). Exact managed-component resolutions are retained in `firmware/dependencies.lock` after successful configuration.

The bootstrap script installs the pinned SDKs, target toolchains, Python environment, CMake, Ninja, and GN under `.cache/`. Linux x86-64, Git, Python 3 with virtual-environment support, and internet access are required. SDK downloads require several gigabytes. Existing SDK checkouts at unexpected revisions stop setup without being reset.

```bash
bash tools/bootstrap-sdk.sh
source tools/activate-sdk.sh
```

Source `tools/activate-sdk.sh` in each new build shell. Shell profiles and system-wide packages remain unchanged. SDK submodules are initialized for the ESP32 platform. Building from a WSL Linux filesystem generally avoids the filesystem overhead of a Windows-mounted workspace.

The local Ninja launcher defaults to eight parallel jobs, including the nested Matter library build. Set `IR_BUILD_JOBS=4` before building on a memory-constrained host. Explicit Ninja job arguments override that default.

## Configuration and compilation

From the project root in an activated ESP-IDF/ESP-Matter shell:

```bash
python tools/profile_config.py config/devices.json
cd firmware
idf.py set-target esp32s3
idf.py build
```

For local appliance mappings, use an absolute path so CMake can resolve the configuration independently of its build directory:

```bash
cd firmware
idf.py -DPROFILE_CONFIG="$(realpath ../config/devices.local.json)" reconfigure
idf.py build
```

The profile validator runs at CMake configuration time and generates `profiles.hpp` inside the build directory. Invalid fields, duplicate JSON keys, duplicate native command mappings, or unsupported activation metadata stop configuration. `board_config.hpp` owns runtime GPIO assignments; changes must remain synchronized with the hardware GPIO map.

The separate `PROFLAME2_CONFIG` CMake option defaults to `config/proflame2.json`. Its generator also validates the selected IR profile file and emits `proflame2_config.hpp`; identity/capability errors and unqualified activation stop configuration. The supplied X0A configuration is disabled, the reviewed-profile registry is empty, and UART/pin/inhibit assignments remain unset. See [X0A configuration and activation](proflame2/software.md#configuration-and-activation). No enabled target build is qualified or supplied.

The target module is ESP32-S3-WROOM-1-N16R8: 16 MB flash and 8 MB octal PSRAM. GPIO35–37 are reserved for the module's PSRAM. The default console uses native USB Serial/JTAG. UART console selection through `menuconfig` changes the runtime console transport; ROM UART programming remains available through the recovery header.

The local verification images are retained under ignored `build/artifacts/`, including the application, bootloader, partition table, OTA initialization data, flash arguments, ELF, map, and SHA-256 manifest. These images use disabled appliance profiles. A fresh build creates its outputs under `firmware/build/`.

For a maintained local package, follow [repeatable artifact export](artifact-export.md). The exporter uses a fresh build directory, validates the four flash images and metadata, records source/SDK identity, and writes a package with relative paths and SHA-256 checksums. Its build-local SDK configuration preserves `firmware/sdkconfig`. The [delivery requirements](delivery-requirements.md) limit this workflow to local development; production provisioning is deferred.

## Flashing and recovery

1. Complete the initial electrical checks in the [hardware bring-up procedure](bring-up.md).
2. Use a qualifying USB-C power source. Default-current USB hosts do not enable the board's main supply.
3. Select the actual serial port. In WSL, USB forwarding must be configured separately if flashing through WSL.
4. Flash and open the monitor:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

The port is an example, not an assumed connected device. A Windows native toolchain can use the corresponding COM port.

For ROM recovery, hold BOOT while resetting, then release BOOT and flash through native USB or the 3.3 V UART header. The UART header has no supply pin. Never connect a 5 V UART signal to the ESP32.

Ordinary application flashing preserves `ir_store` when its partition offset and size remain unchanged. Do not run `erase-flash` to resolve a routine build or connection failure. It erases confirmed commands as well as commissioning data. NVS initialization failures remain visible and do not trigger an automatic erase.

The partition layout reserves two application slots. The console provides no OTA update command. OTA installation and recovery require separate validation.

## Matter commissioning

After flashing, follow the [Matter commissioning procedure](operation.md#matter-commissioning). Development identities are unsuitable for production use, and default profiles expose no appliance controls. Endpoint-topology changes require rebuilding and recommissioning; saved IR commands remain separate from Matter configuration.

## Host checks

On Linux or WSL with Python and g++:

```bash
bash tools/test-host.sh
```

The checks exercise waveform decoding, malformed-capture rejection, storage record integrity, duration limits, profile validation, BOM reference consistency, power-switch pin assignments, GPIO agreement, and partition separation. Direct service and storage tests compile the actual IR service with controlled SDK/FreeRTOS dependencies, covering queue ownership, capture/transmit exclusion, cleanup, driver failures, overwrite selection, and commit/read-back failures. The [service-test harness guide](../tests/service_stubs/README.md) describes its boundaries and optional mutation checks.

Direct Matter and startup tests exercise the actual adapter, application entry point, and button task with controlled SDK, IR-service, and terminal boundaries. They cover endpoint selection, attribute mapping, fan synchronization, initialization failures, and commissioning/reset scheduling. The [Matter/startup harness guide](../tests/app_stubs/README.md) describes its boundaries and optional mutation checks.

Console tests compile the command handlers against SDK/service stubs for both transport selections and exercise the bounded line reader. Serial-client tests use a mocked port. X0A tests cover framing, ownership/transaction policy, transport failures, configuration rejection and disabled target-service boundaries. C++ checks use AddressSanitizer and UndefinedBehaviorSanitizer. These checks do not exercise ESP32 flash hardware, RMT/UART timing, electrical inhibit, CHIP networking, RF, optical range, handset priority or appliance behavior.

On Windows, the Python checks can run independently:

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
python tools/profile_config.py config/devices.json
```

## GitHub connection

Repository setup, excluded files, and licensing status are described in [repository preparation](../CONTRIBUTING.md#repository-preparation). Hosted checks, local job reproduction, and diagnostic artifacts are described in the [CI and failure-diagnosis guide](ci.md).
