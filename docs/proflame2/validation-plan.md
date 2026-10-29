# Passive capture and isolated validation

[Research overview](README.md) | [Open questions](open-questions.md)

## Recorded work and limits

Only public-source research, document inspection, and offline arithmetic were performed. No target hardware was identified, connected, paired, powered, reset, or commanded. No radio or serial device was opened. No upstream controller, accessory application, or firmware was executed.

| Offline check | Result | What the result establishes |
|---|---|---|
| Published six-byte UART examples | 13 examples; all XOR checks match | Internal consistency of the selected documented byte strings |
| Published RF packet table | 220 rows, five identities; one calibration row per identity | Available sample scope, not five independently tested local devices |
| RF checksum prediction on held-out rows | 215 rows / 430 check bytes; zero mismatches | The command-mixing relation fits this published dataset beyond its calibration rows |
| Proposed arbitrary-serial matrix | Computes `14E7` for `4B52020106`; reported frame has `1DE5` | The published generalization fails the cited counterexample |
| RF frame-length arithmetic | 182 symbols; 81.90 ms at 450 microseconds, or 81.45 ms without final low symbol | Nominal arithmetic, not measured transmitter duration |
| SmartFire example bit interpretation | `51`: power bit 1, thermostat bit 0 | Detects the reversed prose labels in that example |

Inputs and detailed results are retained in the [source archive](sources/README.md), including [offline-evidence-checks.json](sources/offline-evidence-checks.json). The [reproduction script](sources/check_evidence.py) uses only local files and Python's standard library; it neither imports archived controller code nor accesses a device or network. Run `python docs/proflame2/sources/check_evidence.py` from the workspace to print a fresh result. [S10-S13](sources.md#s10); [S14](sources.md#s14)

The IFC technical table and connector drawing, plus the Montigo accessory/label page, were visually checked against rendered PDF pages. Other relevant PDF passages were extracted and reviewed with page boundaries. PDF extraction alone can lose drawing orientation; actual harness construction requires the original OEM drawing.

Documentation verification covered 12 Markdown files, including the existing documentation index: 283 local links/anchors resolved, code fences were balanced, and all 29 retained-source/analysis hashes matched the manifest. Ten unavailable download attempts remained explicitly recorded. The independently reproduced arithmetic matched the saved JSON. These checks do not validate external-link availability, upstream claims, electrical interfaces, RF behavior, firmware, or an appliance.

## Stage 1: establish a bounded fixture

The following stages are **proposed**, not completed. The starting point is passive observation or an isolated receiver/test fixture with **fuel and ignition outputs safely disconnected under the applicable manufacturer procedure**. Isolation and instrument suitability must be established by qualified personnel. This research is not a procedure for making an installed gas appliance safe to service. The IFC warnings cover mains, ignition voltage, grounding, and manufacturer wiring requirements. [S02, p. 9](sources.md#s02)

Record the [full hardware identity](hardware-variants.md#required-local-identity-record), OEM manual revision, fixture configuration, and approved disconnection procedure. Confirm that existing selector positions cannot create an unintended request when power returns. The IFC manual explicitly describes restart behavior after supply restoration. [S02, p. 13](sources.md#s02)

A fixture without a real flame can enter lockout or refuse some operations. Record that outcome. Do not emulate flame, bridge air-pressure/thermal inputs, short safeguards, modify firmware, or repeatedly reset the IFC to force completion. If the manufacturer provides no suitable procedure, validation stops at passive/offline evidence.

## Stage 2: passive wired observation

1. Establish the actual X0A keying, ground reference, logic voltage, and isolation before instrument connection. Record the viewing direction and pin labels photographically; do not infer orientation from wire colors.
2. With any existing approved accessory still acting as the sole serial master, observe IFC TX and accessory TX using high-impedance, receive-only channels. Do not attach another UART transmitter or supply pin.
3. Preserve the raw time-stamped waveform and decoded bytes in both directions. Begin by testing the reported 9600 8N1 hypothesis; record measured bit period, idle polarity, voltage levels, framing errors, and response latency.
4. On an isolated fixture, observe available ordinary accessory requests one field at a time. Record raw bytes before assigning meanings. Compare response types, XOR, payload ordering, registers, and ownership exchanges with the source reference.
5. Observe permitted handset intervention and ownership denial on the same safe fixture. Record whether responses become stale, how ownership later becomes available, and whether pending accessory commands are replayed.

The reported IFC sends no unsolicited serial output. Therefore, silence on a board without an accessory is inconclusive: it cannot disprove the interface or confirm a pinout. [S14, UART Communication](sources.md#s14)

## Stage 3: offline parser and policy review

Replay captured bytes into an offline parser, never back into the appliance. Check truncated frames, checksum failures, unexpected registers, duplicate responses, delayed replies, and loss of ownership. Use recorded timings to choose conservative request scheduling; do not promote the ESPHome component's 50/250 ms values to protocol requirements. [S15](sources.md#s15)

The [implemented software](software.md) starts without traffic, rejects stale baselines, preserves unrelated fields and has no actuation backlog. Host tests exercise denial/loss handling, uncertain writes and explicit recovery. Requested state and timestamped register readback remain distinct from actual flame state. These software results do not establish physical handset arbitration or measured timing bounds; recorded-capture validation remains pending in the [qualification record](hardware-qualification.md).

## Stage 4: isolated request/response validation

Only after identity, electrical conditions, and passive framing are established, a future authorized bench investigation may substitute one controller for the existing serial master. Begin with documented GETs of `16`, `17`, and `18`. Ownership requests and ordinary state writes are subsequent, separate checks with bounded attempts and explicit logging. Unknown registers, brute-force payloads, lockout resets, and safety-related outputs are excluded.

Compare requests, responses, and readback against a current baseline. Do not use a command's apparent acceptance as proof of load operation. If the isolated IFC refuses operation or locks out, preserve the failure record and stop the affected test; never defeat supervision to make a protocol test pass.

Acceptance for a prototype requires repeatable framing, valid ownership handling, supported-field mapping, bounded failure handling, and no automatic replay after handset intervention. Acceptance of an installed appliance is a separate OEM/service matter and is not an output of this plan.

## Optional passive RF investigation

Use a receiver covering 315 MHz and an owned handset, with the appliance safely isolated as above. Retain raw IQ or pulse timings as well as decoder output. A suggested acquisition uses at least 1 MS/s IQ for resolving approximately 417-450 microsecond symbols; this is an analysis choice, not an OEM parameter. Record receiver settings, sample rate, antenna, distance, clock calibration, and software version.

Capture individual ordinary controls and repeated identical requests. Separate pairing, manual, thermostat, and idle observations. Measure carrier offset, mark/space distributions, block lengths, repetition count, inter-frame gaps, identity stability, and changes across batteries or normal handset restarts. Derive constants from calibration frames and test them on held-out states. Keep parity validation separate from final-check-byte validation. [S11-S13](sources.md#s11)

To investigate a claimed RF reply, use an independent passive receiver while distinguishing handset bursts from any receiver emission. A half-duplex radio cannot establish what occurred while it was transmitting. No absent burst in a short capture proves the entire family is receive-only. Replay testing remains a later, separately authorized isolated experiment; it was not conducted.

## Evidence record template

| Field | Required content |
|---|---|
| Identity | Appliance, IFC, handset/accessory parts and revisions; label photographs |
| Safety boundary | Applicable OEM procedure and isolated fixture configuration; permitted scope |
| Capture | Unique ID, timestamp/time zone, file SHA-256, format, instrumentation and software versions |
| Stimulus | Ordinary documented input, previous state, changed field; none for passive idle capture |
| Observation | Raw bytes/waveforms, decoded values, measured timing, actual response source |
| Conclusion | Confirmed on this fixture, conflicting, inferred, or inconclusive; source of each interpretation |
| Limit | Missing loads, lockout, absent feature, unknown state, RF collision, or measurement uncertainty |

No completion of this table should imply combustion verification unless a separately authorized, manufacturer-compliant test actually establishes it.
