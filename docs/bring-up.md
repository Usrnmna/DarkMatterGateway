# Hardware bring-up and acceptance procedure

Hardware acceptance status: **not performed**. The circuit and operating limits are specified in the [hardware design](hardware-design.md). Bench records must include the board identifier, BOM substitutions, firmware revision, instruments, supply and cable identifiers, measurements, and deviations. Host tests and successful firmware compilation provide no evidence of physical acceptance.

## 1. Unpowered inspection

1. Review the captured schematic against the [connection schedule](../hardware/connections.csv), manufacturer's pin tables, and footprints. Resolve ERC/DRC findings before assembly release. Verify U5 is TPS2553DBVR with 21.5 kΩ from ILIM to ground; its FAULT pin must not carry a timing capacitor.
2. Check U2/U4 five-pin and U5 six-pin SOT23 footprints individually; their pin functions differ. Verify U3 orientation under magnification.
3. Inspect diode polarity, MOSFET gate/source/drain orientation, regulator feedback connection, and electrolytic polarity. Confirm one 68 Ω resistor per onboard LED and one 220 Ω resistor per external port.
4. Measure resistance from each supply rail to ground. Investigate persistent near-shorts; capacitor charging can affect readings. Verify continuity of ground, USB pair, CC nets, and header pins. Confirm no external 5.1 kΩ CC resistors and no connection between CC1 and CC2.
5. Leave expansion modules and appliance emitters disconnected. Check external emitter harnesses separately for polarity, antiparallel diode installation, short circuits, and correct pin1 marking.

## 2. Power qualification

1. Apply a current-limited 5 V source through a Type-C test fixture capable of advertising default, 1.5 A, and 3 A current. Start with the main load disconnected if practical. Do not bypass current advertisement by bridging U5.
2. In default-current mode, confirm 3V3_AON is present, OUT1 is high, MAIN_ON is low, and +5V_MAIN stays off. The MCU must not boot. Measure always-on input current.
3. Advertise 1.5 A, then 3 A. In both modes, confirm OUT1 low, MAIN_ON high, and +5V_MAIN startup. Observe C7 charging, main buck startup current, and MAIN_FAULT_N on an oscilloscope. Current limiting can extend the ramp; confirm the rail reaches regulation without repeated attach/reset cycling, including minimum input voltage and the full electronics load.
4. Reverse Type-C plug orientation and repeat. Test a default-current data host; the absence of boot is expected. Test native USB through a data-capable source/hub that advertises at least 1.5 A.
5. Measure +5V_MAIN, 3V3_MAIN, receiver-local supplies, and ripple at idle and during Wi-Fi activity. Provisional acceptance: main 3.3 V within ±5%, no MCU brownout, and receiver supplies within their manufacturer operating range. Apply actual device limits if stricter.
6. Exercise the 0.8 A electronics allocation with a controlled load before relying on the power budget. Measure converter efficiency, F1 drop/temperature, U5 loss, and transient rail minima. Keep total input within the advertised source capability including startup.
7. With an electronic load and instrumented current-limited source, verify U5's approximately 1.106–1.297 A current-limit range and fault response. Check recovery after overload removal and monitor thermal cycling during a bounded short test. Brief transient peaks are not bounded by the steady-state limit; record source and input-capacitor response.
8. Verify rail discharge through R42 on source removal and on return to default-current advertisement. Measure reverse current during U5's protection response delay. Confirm no UART or expansion lead back-powers the board. GPIO1 divider, if fitted, must read the switched rail.

## 3. Reset and programming

1. Observe `TX_OE_N` and all MOSFET gates during power insertion, reset, BOOT download entry, watchdog reset, and firmware crashes. TX_OE_N must remain high until intentional transmit initialization; gates must remain low with outputs disabled.
2. Program through native USB on a qualifying Type-C data source. If unavailable, use J4 with a 3.3 V UART adapter and independently supplied board power; leave the adapter power lead disconnected.
3. Confirm RESET and BOOT actions, readable console, status LED polarity, commissioning-button input, and firmware GPIO mapping. Confirm PSRAM initialization uses the N16R8-compatible configuration.
4. Read the flashed application identifier. A successful flash verifies programming only; it does not establish Matter operation or optical compatibility.

## 4. Driver electrical tests

1. Use a dummy load or shielded test target before attaching emitters to appliances. Command each zone separately with a short 38 kHz burst at one-third carrier duty.
2. Measure each gate, drain, and resistor voltage. Derive current from the measured resistor drop. Expect approximately 54 mA per onboard LED and 17 mA per external emitter at nominal 5 V; measured forward voltage determines actual values.
3. Check 30, 38, 40, 56, and 60 kHz waveforms with the selected cable lengths. Verify carrier period, duty ratio, mark/space timing, ringing, and absence of unintended activity on other zones.
4. Test open and short external-port conditions using a current-limited setup. Current must be bounded by the 220 Ω series resistor. Verify resistor and MOSFET temperature. Do not use an energized appliance connection as a test load.
5. Confirm buffer disable terminates drive and a normal reset does not emit an unintended burst. Separately test the software duration limit; do not interpret the enable buffer as an independent watchdog.
6. Perform a controlled static-drive thermal test with the emitters optically enclosed and monitored. Confirm all LED, resistor, MOSFET, fuse, and enclosure temperatures satisfy component limits at 40 °C ambient. Stop on unexpected heating.
7. Evaluate external-port transient suppression with appropriate instruments and methods. The proposed TVS parts do not establish system-level ESD immunity by inspection.

## 5. Receiver and learning tests

1. Observe U7/U8 idle-high envelope outputs and U9 carrier pulses under a known source. Confirm supply filtering, logic levels, and pin polarity without relying on a camera image.
2. Measure known 30, 38, 40, 56, and 60 kHz sources at several distances. Record carrier estimate error and missing/extra cycles against an oscilloscope reference. Reject captures outside the validated operating range.
3. Capture short commands, long AC state frames, repeats, and held keys. Validate frame boundaries and memory-overflow handling while Wi-Fi and Matter processing are active. Include multi-frame commands with gaps longer than the 15 ms receive-idle cutoff; a syntactically accepted first frame does not establish a complete command capture.
4. Compare learned replay to the original remote optically, including initial mark, all frame gaps, repeat counts, and total duration. A matching digital transmit buffer alone is insufficient.
5. Test incandescent/LED lighting, sunlight through windows, TV illumination, and transmitter self-interference. Evaluate the final enclosure window and opaque internal divider.
6. Document successful captures with exact remote model, transport, carrier, command identity, firmware version, source distance, and validation instrument. Do not infer 455 kHz or RF support from the GPIO7 expansion provision.

## 6. Appliance and Matter validation

| Category | Required evidence before enabling a profile |
|---|---|
| IR light | Exact model and remote; discrete versus toggle power; supported brightness/color commands; observed response |
| IR fan | Exact model and remote; discrete power/speed versus cycle commands; stop behavior; observed response |
| Gas fireplace | Exact appliance and remote; verified IR transport; manufacturer-permitted remote behavior; power/flame command semantics; retained original safety controls; supervised functional check |
| Portable rolling AC | Exact appliance and remote; verified IR transport; complete capture and observed preset behavior; temperature units; mode/fan values; compressor restart constraints from manufacturer documentation. Frame encoding and checksum analysis are required before generating commands for uncaptured settings. |
| Raw device | Command identity, selected emitter zone, carrier/timing validation, and explicit distinction between transmitted and observed state |

Placeholder profiles remain disabled. Do not substitute sample commands for a fireplace or AC whose model is unknown. Record commanded state as an estimate until an independent and suitable feedback mechanism has been validated. A successful Matter write or queued IR transmission does not prove that an appliance received or acted on the command.

Commission with the intended Matter controller, exercise on/off and supported mapped attributes, reboot the node, lose/recover Wi-Fi, and simulate missed IR commands. Check that persisted mappings survive restarts and that startup does not replay an appliance activation command. Test the original remote alongside the node and record state-estimation limitations.

## 7. Persistent storage and interruption tests

1. Learn a command, test it against an appropriate target, and save it with `--confirmed`. Record `show NAME` output and the successful commit/read-back message.
2. Reboot, then disconnect and reconnect power. Confirm the same waveform loads and replays only after an explicit command. Repeat with several saved names and all three emitter zones.
3. Leave a new capture unsaved and remove power. Confirm the pending capture is lost while previously committed names remain readable.
4. On a test unit, interrupt power at varied points during saves and overwrites. A save without its success message is unconfirmed. Check existing records, CRC rejection, and NVS initialization after each restart. Retain console and waveform evidence for any missing or changed committed record.
5. Run `matter-reset --confirmed`, then verify saved commands remain available before recommissioning. Flash an updated application with the same partition layout and repeat the checks. Do not use full-chip erasure during retention testing.
6. Test controlled brownouts and repeated supply removal with emitters directed at a test target. Confirm no automatic replay and no unintended output during recovery. Record the minimum supply conditions under which persistence and normal operation remain valid.

## Acceptance record template

```text
Board identifier:
Assembly/BOM deviations:
Firmware revision and build command:
Date and ambient temperature:
Supply / Type-C advertisement / cable:
Instrument identifiers:
Rail measurements and startup captures:
TX currents / carrier accuracy / driver temperatures:
Receiver and long-frame capture results:
Save / reboot / power-interruption / Matter-reset retention results:
Appliance and remote model identifiers:
Matter controller and tested controls:
Observed behavior and unresolved failures:
Evidence file paths:
Acceptance status: NOT TESTED / PASS FOR STATED CONDITIONS / FAIL
```
