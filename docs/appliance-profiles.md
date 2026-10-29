# Appliance configuration

`config/devices.json` contains four disabled profiles. Manufacturer, appliance model, and remote model fields accept `null` until identified. Command names are aliases for individually learned waveforms; they do not contain built-in appliance codes.

Copy the file to `config/devices.local.json` for installation-specific settings. The local file is ignored by Git. Validate before building:

```text
python tools/profile_config.py config/devices.local.json
```

The CMake `PROFILE_CONFIG` option selects the configuration compiled into firmware; see [configuration and compilation](build.md#configuration-and-compilation). Changing profile topology requires rebuilding and [recommissioning](operation.md#matter-commissioning); profile changes are not applied over the serial console. Learned command records occupy a separate partition and need not be relearned when the application is reflashed without erasure and the partition layout remains compatible.

## Shared fields

| Field | Meaning |
|---|---|
| `id` | Stable local name, 1–15 lowercase letters, digits, or underscores |
| `kind` | `light`, `fan`, `gas_fireplace`, `portable_ac`, or `raw` |
| `enabled` | Enables the profile's named aliases; native Matter endpoints are available only for lights and fans |
| `zone` | 0 onboard array, 1 external port J2, 2 external port J3 |
| `manufacturer`, `model`, `remote_model` | Appliance identification, initially `null` |
| `transport_verified_ir` | Confirmation that the required functions use IR |
| `discrete_commands_verified` | Confirmation that mapped controls set explicit states rather than cycling or toggling |
| `commands` | Action-to-saved-command mapping |

An enabled profile requires a model and verified IR transport. Native light and fan mappings also require distinct command names and explicit-state verification. These configuration assertions require physical evidence; the validator cannot establish appliance behavior from a JSON file.

## Lights

Native Matter light control requires separate ON and OFF waveforms. Learn and confirm each command. Assign their names to `commands.on` and `commands.off`, set verified fields, and enable the profile.

A remote with only a power-toggle button remains usable through a directly called saved command. Assigning the same toggle capture to ON and OFF does not implement reliable absolute power control and is rejected for native profiles. Dimming and color controls are not implemented.

## Fans

Native Matter fan control requires separate OFF, LOW, MEDIUM, and HIGH commands. Percentage controls map as follows:

| Requested percentage | Learned action | Reported discrete percentage |
|---:|---|---:|
| 0 | OFF | 0 |
| 1–33 | LOW | 33 |
| 34–66 | MEDIUM | 66 |
| 67–100 | HIGH | 100 |

The percentages represent selected speed positions, not measured airflow or RPM. Auto mode, oscillation, direction, timers, and speed-cycle commands are not native controls. Additional actions can be learned and replayed directly by name.

## Matter state and feedback

Matter state represents accepted control intent for both light and fan profiles. An IR transmission has no appliance acknowledgement. Operation through the original remote, local buttons, mains interruption, or missed IR can make reported state differ from physical state. No IR command is emitted merely to synchronize startup attributes.

Matter can suppress a write when its cached attribute already has the requested value. Repeated ON/OFF requests therefore do not guarantee repeated optical transmission, including an OFF request when the startup attribute is already OFF. Use an explicit `send` or `call` command for a deliberate retransmission. A state-confirming feedback path would be required to remove this limitation.

## Gas fireplaces

The separate [wired Proflame 2 X0A backend](proflame2/software.md) implements explicit console operations behind blocked qualification gates. Its configuration does not weaken the IR requirements below. The supplied X0A profile is disabled, and no hardware identity, UART/pin allocation or bench result is qualified. An enabled IR profile with the same device ID is rejected by X0A configuration validation. There is no native Matter fireplace endpoint.

The fireplace profile begins disabled. Fill manufacturer, appliance model, remote model, and IR verification after checking the appliance documentation. Verify `remote_operation_permitted` against the manufacturer's operating instructions before enabling profile aliases.

Reserved aliases are `on`, `off`, `flame_up`, and `flame_down`. Remove or rename unsupported aliases; only verified captures should be assigned. Some remotes use RF rather than IR, and combined remotes may use different transports for different functions.

The terminal operates the existing remote receiver optically. Ignition sequencing, flame supervision, lockout handling, and shutoff remain under the original appliance controller. There are no valve connections, ignition outputs, thermostat schedules, automatic relight actions, or startup replays.

Native Matter fireplace controls are not implemented. Enabled fireplace aliases remain manually callable through `call fireplace ACTION`; no fireplace endpoint is mislabeled as a light, fan, or thermostat. A model with toggle-only power can retain a clearly named manual toggle command rather than separate ON/OFF claims. Confirm the OFF behavior separately under the appliance's prescribed test conditions.

## Portable rolling air conditioners

Portable A/C model and remote identity fields begin `null`. The following additional variables remain unset until documented:

| Field | Initial value | Required evidence |
|---|---|---|
| `temperature_unit` | `null` | Remote display and appliance instructions; use `C` or `F` |
| `temperature_min`, `temperature_max` | `null` | Supported setpoint limits, supplied together |
| `state_frame_verified` | `false` | Captures showing whether each button transmits a complete state |

The initial aliases are `off`, `cool_preset`, `fan_preset`, and `dry_preset`. A preset denotes the exact state represented by its confirmed capture. Record the mode, temperature, fan setting, swing position, and timer state alongside each capture. Avoid naming a captured complete-state frame `temperature_up` unless that relative behavior has been established.

Example commissioning sequence:

1. Record appliance and remote models and verify the transport.
2. Configure the original remote to a supported cooling state with unintended timers disabled.
3. Capture the command, test the intended result, and save it under `ac_cool` after confirmation.
4. Repeat for OFF and any required fan-only or dry presets.
5. Compare captures at different setpoints and modes before claiming protocol-level control.
6. Check whether the complete transmission fits the single-frame capture limits. A long gap or multi-frame protocol can exceed those limits even when the carrier is supported.

A/C remotes often encode the complete operating state, and general protocol implementations require model-specific encoding and checksum knowledge. Named presets avoid assuming that one learned command can generate every setting. [IRremoteESP8266 protocol notes](https://github.com/crankyoldgit/IRremoteESP8266/wiki/Frequently-Asked-Questions)

No native Matter thermostat, temperature regulation loop, compressor cycling schedule, or temperature sensor is implemented. Manufacturer compressor restart delays and condensate handling remain unchanged. Unexpected response requires a new capture or protocol analysis, not repeated automatic transmission.
