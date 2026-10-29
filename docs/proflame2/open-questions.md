# Unresolved questions

[Research overview](README.md) | [Validation plan](validation-plan.md)

No unresolved item should be filled with a value from a visually similar remote or a different Proflame generation. The following observations would materially change implementation readiness.

| Priority | Question | Evidence or measurement required | Current consequence |
|---|---|---|---|
| Before wiring | Which appliance, handset, IFC, and firmware/revisions are installed? | Full labels, FCC IDs, exact OEM manual/parts list, Connect indicators, photographs with connector orientation | No exact compatibility declaration or cable specification |
| Before wiring | Is X0A present, and what are its voltage, reference, and isolation conditions? | OEM electrical information and qualified measurements on an isolated fixture | No direct USB/MCU connection or assumption that the 3V3 pin can power an external controller |
| Before writing | Is an OEM accessory already the serial master? | Harness inventory and passive two-direction capture | No second transmitting device on the same connection |
| Before writing | Does this IFC use the documented six-byte 9600 8N1 exchange? | Raw timed captures, checksum/response comparison, board identifier | Community protocol remains a candidate rather than a verified target specification |
| Before writing | How does handset priority affect ownership and state on this board? | Isolated observation of register 16 and validity of 17/18 before/during/after handset activity | Handset override is accepted; denied or stale state must block writes and invalidate readback |
| Before writing | What releases UART control denial, and what happens to queued writes? | Isolated handover sequences, including documented normal handset OFF; no forced unpairing | No universal release timeout or automatic resumption policy |
| Before feature exposure | Which ordinary fields actually exist? | OEM option list and controlled one-field observations | Unsupported controls remain unavailable even if their bits are known |
| Before thermostat support | Are UART low-nibble values 2/3 independent mode values or combinations of flags? Where is setpoint/regulation implemented? | Exact accessory/version behavior and documented or captured setpoint flow | No claim of autonomous UART thermostat/SMART control from enum names alone |
| Before status claims | Are diagnostic/fault/flame states exposed by a documented interface? | Exact OEM interface documentation or passive verified status mapping | Register state is not actual flame, valve, gas-flow, or safety status |
| Before disconnect handling | What happens after serial or RF communication stops? | OEM specification or safely isolated timed observations covering the actual operating mode | No assumed fail-off watchdog; 24-hour restart and seven-day PoD are different mechanisms |
| If RF is pursued | What carrier, symbol timing, repeat count, and gap does the owned handset use? | Calibrated passive IQ/pulse captures and measured distributions | 315 MHz / 417-450 microseconds are research starting points, not calibrated settings |
| If RF is pursued | Do captured identity/checksum constants remain valid across modes/restarts? | Separate calibration and held-out captures for the exact handset | No arbitrary-ID checksum generation or universal replay claim |
| If RF is pursued | Does any target receiver emit an RF acknowledgment or status? | Independent passive receiver, source separation, and hardware documentation | No expected RF ACK, automatic acknowledgment retries, or bidirectional telemetry claim |
| If Connect is pursued | Do the exact OEM dongle and firmware expose the reported local service? | Approved accessory identifiers and passive traffic/firmware documentation | Community WebSocket details are a lead, not confirmed compatibility or security evidence |
| Documentation gap | Can original FCC exhibits and revision-specific schematics be obtained normally? | Accessible official/public exhibits or manufacturer-supplied documents | No internal-chip, oscillator, or schematic claims from blocked files |

The source basis for these limits is recorded in the [evidence matrix](evidence-matrix.md), especially [S14](sources.md#s14), [S15](sources.md#s15), and the conflicts in [S10-S13](sources.md#s10).

## Boundary of the present conclusion

A matching wired prototype can be specified from the available evidence. Installed use, hardware approval, guaranteed shutdown on communication loss, and universal third-party RF compatibility cannot be concluded. Work on an actual appliance, firmware integration, architecture changes, and radiated experiments were not part of this research.
