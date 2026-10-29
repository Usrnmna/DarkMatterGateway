# X0A hardware qualification record

**Disposition: activation blocked.** No target hardware identification, electrical measurements, protocol captures, or bench results exist for this integration. Every gate below is pending. This record is an inventory of missing evidence, not a qualified profile or permission to connect equipment.

| Contract item | Current evidence | Required evidence before qualification |
|---|---|---|
| Appliance | Unknown | Manufacturer/model/serial range; applicable installation/service manual revision; permitted accessory/integration basis |
| IFC | Unknown | Full SIT/OEM part number, PCB/firmware revision and variant; label and schematic photographs |
| Paired handset | Unknown | Part number, FCC ID, enabled modes and association; handset remains paired |
| Existing X0A accessory/master | Unknown | Dongle/harness identity, connector occupancy and single-master arrangement |
| Terminal board and interface | Hardware unbuilt; no X0A design | Board revision, pin/peripheral audit, keyed connector drawing, TX inhibit circuit and polarity |
| Connector/reference | Unknown | Orientation, keying, mating parts, continuity and identified GND/TX/RX/supply; approved unpowered procedure |
| Electrical/isolation | Unknown | Idle/high/low voltage, tolerance, grounds/reference and isolation assessment, instrument protection |
| Power | Unknown | Separate supplies, accessory current limits if used, startup/shutdown sequencing, unpowered leakage and no backfeed |
| Ordinary controls | Empty allowlist | Captured and repeated evidence for each permitted field and installed output; unsupported mode rejection |
| UART/framing | Source-reported 9600 8N1 only | Both-direction timestamped passive captures confirming levels, baud, frame format, payloads and XOR |
| Timing | All five production values null | Response/inter-byte maxima, safe request gap, late/duplicate-response bound, quiet interval, snapshot age under load |
| Handset arbitration | Unverified | Denial/loss/release behavior and precedence during acquisition, baseline reads, check/write race and readback |

## Gates

| Gate | Status | Evidence and acceptance needed |
|---|---|---|
| G0 — identity and permitted fixture | Pending | Exact appliance/IFC/handset/board/interface contract, OEM procedure, serial-master inventory, and physical identity cross-check against IR profiles |
| G1 — electrical qualification | Pending | Reviewed interface/connector/pin assignment and isolation design; voltage/current measurements; safe default inhibit through both-side reset, power loss and partial power |
| G2 — passive protocol | Pending | Raw bidirectional captures with timestamps, direction labels, instrument configuration and SHA-256 hashes; framing and timing bounds; treatment of unknown/duplicate/late responses |
| G3 — isolated priority and controls | Pending | Handset priority throughout the check/write race, every allowlisted control, mode rejection, ownership denial/release and readback validity on the exact IFC |
| G4 — integrated failures | Pending | Isolated adapter/service tests for unplug, reboot, brownout, driver errors, late/partial replies, TX inhibit and host disconnect; no unsolicited commands or replay |
| G5 — coexistence and installation | Pending | IR capture/TX and Matter/Wi-Fi load, commissioning/reset, measured timing margins, mechanical/thermal routing and applicable OEM installation review |

Physical validation begins with passive observation or an isolated fixture with fuel and ignition outputs safely disconnected under the applicable manufacturer procedure. No safeguard emulation, lockout bypass, valve control or live-fuel experiment is prescribed. An IFC lockout is recorded and the affected test stops. The [validation plan](validation-plan.md) and [hardware prerequisites](implementation-plan.md#hardware-prerequisites) provide the detailed sequence.

The implementation task performs offline work only. No gate has been passed or waived by compilation, host assertions, synthetic fixtures, or this document. A future activation review must add a reviewed profile matching real evidence and its board constants; changing `enabled` or `qualified_for_control` alone is insufficient. G0–G3 precede target-control enablement, with a separately reviewed isolated-test scope for G4; G4/G5 precede installed-use claims.

For each eventual result, retain the date/time zone, exact identities, OEM procedure/revision, fixture diagram, instrument and software versions, raw file hashes, stimulus, observed bytes and physical outputs, uncertainty, pass/fail disposition and review reference. No physical output may be inferred solely from a UART register value.
