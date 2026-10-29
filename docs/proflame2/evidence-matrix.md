# Claim and evidence matrix

[Research overview](README.md) | [Bibliography](sources.md)

**Specified** = manufacturer/OEM statement; **reported** = original independent investigation; **source-derived** = inspected code; **offline-checked** = arithmetic on published data; **inferred** = reasoned interpretation; **unknown/conflicting** = unresolved. None implies local physical verification.

| ID | Important claim | Evidence and locator | Classification | Applicability or limitation |
|---|---|---|---|---|
| H01 | U.S./FCC baseline 315 MHz; CE reference 433.92 MHz | [S01 p. 5](sources.md#s01), [S03 p. 3](sources.md#s03), [S04 p. 3](sources.md#s04) | Specified | Actual target label remains unknown |
| H02 | Earlier GT filing uses T99058402300; wall product uses T99058404300 | [S08 grant/index](sources.md#s08), [S09 grant/index](sources.md#s09), [S03 p. 10](sources.md#s03) | Regulatory record; partly index-only | Full exhibits unavailable |
| H03 | Later Connect .080 label also shows T99058402300 | [S05 p. 2 label photo](sources.md#s05) | Specified; visually inspected | FCC ID alone cannot distinguish Proflame generations |
| H04 | .042 handset/.325 IFC and .080/.665 combinations occur in different OEM references | [S06 pp. 3-4](sources.md#s06), [S05 p. 2](sources.md#s05) | Specified | Examples, not installed inventory or interchangeability |
| H05 | Optional RF/IR adapter label exists | [S01 pp. 7-8](sources.md#s01) | Specified | Does not specify any optical IR protocol |
| O01 | IFC owns ignition, flame supervision, interlocks, and lockout | [S02 pp. 10-13](sources.md#s02) | Specified | No third-party bypass/reset method |
| O02 | Controls include power, levels, thermostat/SMART, pilot and optional loads | [S01 pp. 14-20](sources.md#s01) | Specified | Only fitted/enabled functions apply |
| O03 | SW1 learning interval approximately 10 s with documented beep sequence | [S02 p. 8](sources.md#s02) | Specified | No complete factory RF handshake derived |
| O04 | FFRT10 timing table differs from optional FFRT5 description | [S02 p. 4](sources.md#s02), [S01 p. 2](sources.md#s01) | Specified | Values must not be combined across variants |
| O05 | Low receiver batteries produce double beep then command refusal | [S02 p. 8](sources.md#s02) | Specified | Not an observed local test |
| O06 | Supply restoration may restore an ignition request | [S02 p. 13, section 3.4.2](sources.md#s02) | Specified | Power cycling is not assumed neutral |
| O07 | 24-hour safety restart and seven-day PoD are separate functions | [S02 pp. 2, 10, 12](sources.md#s02) | Specified | Neither proves loss-of-radio shutdown |
| O08 | Universal lost-link shutdown deadline | Reviewed [S01](sources.md#s01), [S02](sources.md#s02), [S10-S16](sources.md#s10) | Unknown | No fail-off guarantee |
| W01 | X0A is used by an approved Connect dongle | [S05 p. 3](sources.md#s05) | Specified | Does not make the reverse-engineered UART API supported |
| W02 | X0A carries 3.3 V UART at 9600 8N1 on investigated hardware | [S14 Connectors / UART Communication](sources.md#s14), [S15 Hardware Setup](sources.md#s15) | Reported | Electrical reference, orientation, isolation/current still need target checks |
| W03 | Six bytes, types 5D/5E/AD/AE, XOR, registers 16/17/18 | [S14 Message format and examples](sources.md#s14), [S15 packet/checksum implementation](sources.md#s15) | Reported and source-derived | Not a manufacturer serial standard |
| W04 | Ownership token C1 A0 00; RF control can deny ownership | [S14 register 0x16 / Limitations](sources.md#s14) | Reported | Token semantics and target behavior unresolved |
| W05 | State 17/18 can be stale or invalid during RF control | [S14 Limitations](sources.md#s14), [S15 README](sources.md#s15) | Reported | No authoritative status under handset override |
| W06 | No unsolicited UART output; approximately 1 s accessory polling | [S14 UART Communication](sources.md#s14) | Reported | Silence without requests is inconclusive |
| W07 | Ordinary state-field layout maps to commands 27/28 | [S14 register mapping](sources.md#s14), [S16 IFC CMD1/CMD2](sources.md#s16) | Reported | Mode naming in S15 conflicts with independent flag layout |
| W08 | 50 ms spacing/250 ms timeout and startup ownership requests | [S15 source constants, setup/update](sources.md#s15) | Source-derived | Implementation choices; not required timings or passive behavior |
| W09 | 13 selected UART checksums match | [Offline result](sources/offline-evidence-checks.json), [S14 byte strings](sources.md#s14) | Offline-checked | Arithmetic only |
| I01 | Dry-contact input has no external voltage; parallel remote/switch use discouraged | [S07 compatibility and remote questions](sources.md#s07) | Specified by OEM | Limited to approved appliance arrangement; no universal GPIO wiring |
| I02 | Connect accessory combination supports shared app/cloud state | [S05 pp. 2 and 7](sources.md#s05) | Specified | Handset-to-UART synchronization is not established by this passage; S14's proposed improvement is an assumption |
| I03 | BLE GATT and TLV framing of W190-0090 accessory | [S14 hardware](sources.md#s14), [S16 Basic Communication / packet format](sources.md#s16) | Reported | Accessory-specific, normal authorized authentication |
| I04 | Port 88 WebSocket and named control fields | [S17 protocol snapshot](sources.md#s17) | Reported; unverified here | Firmware coverage and no-auth claim not independently tested |
| R01 | OOK/ASK, roughly 417 or 450 microseconds per symbol | [S10 RF setup](sources.md#s10), [S11 timing](sources.md#s11), [S12 Physical layer](sources.md#s12) | Reported/source-derived; conflicting timing | Actual handset measurement required |
| R02 | Seven bytes, Manchester blocks, first-block flag and even parity | [S11 proflame2_mc](sources.md#s11), [S12 Frame fields](sources.md#s12) | Source-derived; reported captures | 182 raw symbols, not payload bits |
| R03 | Two state bytes encode ordinary command fields | [S10 packet description](sources.md#s10), [S11 decoder](sources.md#s11), [S12 MAPPING](sources.md#s12) | Reported/source-derived | Some optional fields not exercised in S12's installation |
| R04 | Per-handset constants plus command mixing fit public dataset | [S12 cmd.csv and protocol.py](sources.md#s12), [offline result](sources/offline-evidence-checks.json) | Offline-checked | Five identities, 215 held-out rows; no new physical captures |
| R05 | General serial-to-checksum matrix fails an additional identity | [S13 proposal/counterexample comments](sources.md#s13), [offline result](sources/offline-evidence-checks.json) | Conflicting claim; failure reproduced | Arbitrary-ID construction not established |
| R06 | Decoder integrity label is stronger than its implemented validation | [S11 proflame2_decode](sources.md#s11), [S12 decode_frame](sources.md#s12) | Source-derived | Parity/derived constants do not independently prove final checksum integrity |
| R07 | Verbatim replay accepted on one reported installation | [S12 Transmission is accepted](sources.md#s12) | Reported | Supports feasibility, not universal compatibility or local verification |
| R08 | No counter/challenge visible in reviewed seven-byte format | [S10-S12](sources.md#s10) | Inferred from inspected format | No universal absence-of-authentication/replay-protection claim |
| R09 | RF acknowledgment/echo | [S10 claim](sources.md#s10), [S12 unresolved notes](sources.md#s12), [S13 later argument](sources.md#s13) | Conflicting/unknown | Beep, repetition, or missing FCC ID is insufficient proof |
| R10 | Exact repetition spacing and counts | [S10 README/code](sources.md#s10), [S12 notes/constants](sources.md#s12) | Conflicting | Preserve target waveform; no universal retry schedule |
| C01 | CC1101 supports candidate band/modulation; YARD Stick One is CC1111-based | [S18](sources.md#s18), [S19](sources.md#s19) | Specified by component/tool manufacturers | Not identification of target silicon or a qualified transmitter design |
| P01 | Located remote-control patent is ornamental design | [S20 claim and figures](sources.md#s20) | Original patent evidence | No protocol specification or current legal-status conclusion |

Proposed controller policies and test stages are explicitly marked in the [integration guide](integration.md) and [validation plan](validation-plan.md). They are requirements for future review, not experimental results.
