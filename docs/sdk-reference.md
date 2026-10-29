# ESP-Matter SDK reference

## Source baseline

Dependency revisions checked on 2026-10-16:

| Dependency | Revision |
| --- | --- |
| ESP-Matter | `4cedd4ad40821933d6cc237e80ab6604f2953fb0`, from `release/v1.4.2` |
| ESP-IDF | `v5.4.1`, recommended by the pinned ESP-Matter README |
| ESP-Matter connectedhomeip submodule | `a6892983d6d761a9a787f2e11177a85d28bd1022` |
| Xtensa compiler | GCC 14.2.0, `esp-14.2.0_20241119`, selected by ESP-IDF |
| GN | Revision `487f8353f15456474437df32bb186187b0940b45`, version 2234, selected by the pinned Pigweed manifest |
| CMake / Ninja used for local verification | 3.30.2 / 1.11.1 |

The ESP-Matter revision is a commit on a release branch, not a Git tag. These versions are a selected compatibility baseline; they are not a claim that Matter 1.4.2 or ESP-IDF 5.4.1 is the latest release. Recursive submodule initialization must preserve the revisions selected by ESP-Matter.

Sources: [pinned README](https://github.com/espressif/esp-matter/blob/4cedd4ad40821933d6cc237e80ab6604f2953fb0/README.md), [pinned connectedhomeip tree](https://github.com/espressif/connectedhomeip/tree/a6892983d6d761a9a787f2e11177a85d28bd1022).

Tool pins: [ESP-IDF tool manifest](https://github.com/espressif/esp-idf/blob/v5.4.1/tools/tools.json), [Pigweed GN manifest](https://github.com/google/pigweed/blob/3fe6ddbf82e623b47f23fa5db4d0ab2d5c683cca/pw_env_setup/py/pw_env_setup/cipd_setup/pigweed.json).

## Build integration

The pinned examples define `ESP_MATTER_PATH` from the environment and `MATTER_SDK_PATH` as `${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip`. The top-level CMake file includes `$ENV{IDF_PATH}/tools/cmake/project.cmake`, supplies ESP-Matter and CHIP component directories through `EXTRA_COMPONENT_DIRS`, and defines `CHIP_HAVE_CONFIG_H` for C++ compilation. ESP-Matter supplies transitive CHIP dependencies. A project with its own GPIO drivers does not require the example light-board driver.

The bridge CLI example selects `device_hal/device/hollow` rather than a populated board HAL. Its component search directories additionally include `examples/common` and `device_hal/device`. The light example is a useful source of networking and commissioning defaults, but its default configuration explicitly disables the Fan Control cluster. That exclusion must not be carried into a build exposing fans.

Required configuration review includes BLE/NimBLE commissioning, IPv6 autoconfiguration and routing hooks, sufficient dynamic endpoint count, enabled Fan Control support, 16 MB flash, the selected partition table, and octal PSRAM for the N16R8 module. Board-specific settings and the application component dependencies must be validated by an actual ESP-IDF build.

Sources: [light root CMake](https://github.com/espressif/esp-matter/blob/4cedd4ad40821933d6cc237e80ab6604f2953fb0/examples/light/CMakeLists.txt), [bridge root CMake](https://github.com/espressif/esp-matter/blob/4cedd4ad40821933d6cc237e80ab6604f2953fb0/examples/bridge_apps/bridge_cli/CMakeLists.txt), [example defaults](https://github.com/espressif/esp-matter/blob/4cedd4ad40821933d6cc237e80ab6604f2953fb0/examples/light/sdkconfig.defaults).

## Console integration

ESP-IDF retains USB/UART transport initialization, command registration, and REPL dispatch. The application wraps `linenoiseProbe` and `linenoise` at link time to suppress ANSI negotiation and provide bounded ASCII line input. Explicit undefined-symbol linker options retain both wrapper functions from the application component archive. Oversized or invalid lines are drained and rejected without executing a prefix. The SDK constructor registers the built-in help command.

Source: [ESP-IDF console REPL](https://github.com/espressif/esp-idf/blob/v5.4.1/components/console/esp_console_common.c). Input behavior and limits: [console operation guide](operation.md#console-connection).

## Endpoint and callback contracts

The pinned API provides:

```cpp
esp_matter::endpoint::on_off_light::config_t light_config;
esp_matter::endpoint::fan::config_t fan_config;

// Each create function accepts node, configuration, flags, and private data.
esp_matter::endpoint::on_off_light::create(node, &light_config,
                                         esp_matter::ENDPOINT_FLAG_NONE, context);
esp_matter::endpoint::fan::create(node, &fan_config,
                                esp_matter::ENDPOINT_FLAG_NONE, context);
```

The light configuration includes `on_off.on_off`. The fan configuration includes `fan_control.fan_mode`, `fan_control.fan_mode_sequence`, nullable `fan_control.percent_setting`, `fan_control.percent_current`, and an optional delegate pointer. A fan endpoint contains Fan Control, Identify, and Groups; it does not automatically contain an On/Off cluster.

The application attribute callback has this signature:

```cpp
esp_err_t attribute_callback(esp_matter::attribute::callback_type_t type,
                             uint16_t endpoint_id, uint32_t cluster_id,
                             uint32_t attribute_id,
                             esp_matter_attr_val_t *value, void *context);
```

`PRE_UPDATE` can reject a change. `POST_UPDATE` occurs after a successful change. Irrelevant attributes must return `ESP_OK`. A `PRE_UPDATE` callback must not assume that returning success proves optical transmission or appliance response.

Sources: [endpoint definitions](https://github.com/espressif/esp-matter/blob/4cedd4ad40821933d6cc237e80ab6604f2953fb0/components/esp_matter/data_model/esp_matter_endpoint.h), [endpoint creation](https://github.com/espressif/esp-matter/blob/4cedd4ad40821933d6cc237e80ab6604f2953fb0/components/esp_matter/data_model/esp_matter_endpoint.cpp), [callback implementation](https://github.com/espressif/esp-matter/blob/4cedd4ad40821933d6cc237e80ab6604f2953fb0/components/esp_matter/esp_matter_attribute_utils.cpp).

## Optional fan mapping

A three-speed profile requires explicit Off, Low, Medium, and High command mappings. `FanModeSequence=0` represents Off/Low/Medium/High. The SDK default sequence is 2 and includes Auto, so an application without an Auto command must replace the default.

`FanMode` values are Off=0, Low=1, Medium=2, High=3, On=4, Auto=5, and Smart=6. The pinned CHIP server normalizes On to High and normalizes Smart according to Auto support. An initial three-speed mapping can use percent bands 1–33, 34–66, and 67–100, with zero mapped to Off. These bands are a configurable application convention, not an appliance specification.

The pinned Fan Control server automatically synchronizes Off to zero percent and zero percent to Off. It does not automatically assign Low/Medium/High for nonzero percentages or update `PercentCurrent` for ordinary nonzero settings. Application synchronization must therefore cover those attributes. A recursion guard and canonical-command deduplication are necessary to prevent nested attribute callbacks from transmitting the same IR action repeatedly. `attribute::update()` provides normal attribute reporting; directly changing backing storage with `set_val()` is not an equivalent reporting operation.

`PercentSetting` is nullable. Its encoded null value must not be interpreted as a 255 percent command. A profile without Auto support can reject null settings. Read `FanMode` and `PercentSetting` through the correct unsigned-byte representation, `value->val.u8`; use `esp_matter_enum8`, `esp_matter_nullable_uint8`, and `esp_matter_uint8` for the corresponding updates. Do not write SpeedSetting or SpeedCurrent unless the MultiSpeed feature and its attributes have actually been added.

Initialize the nullable configuration field with an explicitly typed byte, such as `uint8_t{0}`. A plain integer literal `0` is ambiguous between the pinned SDK's byte and null-pointer assignment operators.

Any current-state values maintained from IR transmission are estimates. No optical acknowledgement or appliance-state feedback is implied.

Sources: [fan configuration](https://github.com/espressif/esp-matter/blob/4cedd4ad40821933d6cc237e80ab6604f2953fb0/components/esp_matter/data_model/esp_matter_cluster.h), [fan attribute types](https://github.com/espressif/esp-matter/blob/4cedd4ad40821933d6cc237e80ab6604f2953fb0/components/esp_matter/data_model/esp_matter_attribute.cpp), [CHIP Fan Control server](https://github.com/espressif/connectedhomeip/blob/a6892983d6d761a9a787f2e11177a85d28bd1022/src/app/clusters/fan-control-server/fan-control-server.cpp).

## Gas fireplace placeholders

An unspecified fireplace profile remains disabled. Profile fields retain manufacturer, appliance model, remote model, verified transport, explicit command names, and manufacturer-permitted remote operation. Carrier frequency belongs to each learned waveform rather than the profile. IR support cannot be inferred from the presence of a handheld remote. A raw learned command has no automatic native Matter device-type meaning. Fireplace aliases do not create a native Matter endpoint.

Build results and pending physical checks: [validation evidence](validation.md).
