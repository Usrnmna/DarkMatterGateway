# Wired X0A protocol reference

[Research overview](README.md) | [Integration decision](integration.md) | [Bench validation](validation-plan.md)

**Status: source-backed candidate specification, not a SIT-supported public API and not validated on the target IFC.** The principal evidence is an original serial-capture investigation of a Napoleon eFIRE/Proflame 2 installation, supported by the inspected ESPHome component. Preserve normal handset priority. [S14](sources.md#s14); [S15](sources.md#s15)

## Electrical and transport layer

| Property | Reported value | Boundary |
|---|---|---|
| Connector | Diagnostic Interface `X0A`, four-position JST XH; housing `XHP-4` | Confirm the actual board and keyed orientation |
| IFC-side order in the referenced orientation | `3V3 \| TX \| RX \| GND`, with GND at the printed dot | Not a universal left-to-right viewing instruction; mating-side views reverse the drawing |
| Signaling | 3.3 V logic UART, 9600 baud, 8 data bits, no parity, 1 stop bit | Not RS-232 voltage levels, RS-485, or USB |
| Message size | Six bytes | No separate delimiter documented in the observed exchange |
| Checksum | XOR of bytes 0 through 4, stored as byte 5 | Detects some corruption; no authentication established |
| Observed polling | eFIRE polls the state registers approximately once per second | Observed accessory behavior, not an IFC timing requirement |
| Unsolicited output | No output without a request in the reported installation | Passive observation needs existing accessory traffic to show exchanges |

[S14, Connectors and UART Communication](sources.md#s14); [S15, Hardware Setup and `calculate_checksum`](sources.md#s15).

Signal names are relative to the IFC: its TX is observed by an external RX, and an external TX would drive its RX. Passive capture uses receive-only/high-impedance channels on both directions; no second transmitter is connected to an active accessory bus. A connector that physically fits does not establish the correct pinout.

The 3V3 pin is reported as an accessory supply output. Its spare current capacity and isolation from mains were not established. Do not feed a USB adapter's 5 V or 3.3 V supply into that pin or assume it can power an ESP board. Ground-reference and isolation suitability must be established before attaching a grounded oscilloscope, logic analyzer, or computer. This is especially relevant because the IFC also contains mains and high-voltage ignition circuits. [S15, Hardware Setup](sources.md#s15); [S02, pp. 4 and 9](sources.md#s02)

## Message layout

All numbers below are hexadecimal unless a unit is shown.

| Byte offset | Meaning |
|---:|---|
| 0 | Type: `5D` set request, `5E` get request, `AD` set response, `AE` get response |
| 1 | Payload byte 0 |
| 2 | Payload byte 1 |
| 3 | Payload byte 2 |
| 4 | Register: `16` control ownership, `17` state 1, `18` state 2 |
| 5 | `byte0 XOR byte1 XOR byte2 XOR byte3 XOR byte4` |

GET requests use a zero payload. Ordinary state-register writes in the observed protocol use payload `00 <state-byte> 00`. The control register uses a different three-byte token. Request/response type values are listed explicitly; no portable C/C++ structure layout or host endianness is assumed. [S14, Message format](sources.md#s14); [S15, `send_state_`, packet declarations](sources.md#s15)

## Read-only request examples

These are documented byte strings for analysis, **not commands executed during this research**. Even reads require an electrically qualified connection and can collide with an existing serial master.

| Request | Purpose | Example response |
|---|---|---|
| `5E 00 00 00 16 48` | Read ownership | `AE 00 00 00 16 B8` = no UART ownership |
| `5E 00 00 00 17 49` | Read state 1 | `AE 00 61 00 17 D8` = reported state byte `61` |
| `5E 00 00 00 18 46` | Read state 2 | `AE 00 00 00 18 B6` = reported state byte `00` |

The source's `61`/`00` example has light level 6 and the power bit set, but flame level zero. It illustrates why a power bit is not evidence of a burning main flame. [S14, Communication between eFIRE and ProFlame 2](sources.md#s14)

## Ownership and ordinary writes

The observed controller requests ownership with `5D C1 A0 00 16 2A`. The documented grant response is `AD C1 A0 00 16 DA`; denial is `AD 00 00 00 16 BB`. A subsequent GET of register `16` returns the same ownership token with response type `AE` and the corresponding XOR checksum. The meanings of the individual token bits are unknown. This is reported arbitration, not permission to override a denial. [S14, register 0x16](sources.md#s14)

The normal exchange reported by the source is:

1. Request UART ownership and inspect the response.
2. Confirm ownership with a GET of `16`.
3. Read `17` and `18` to obtain a current baseline.
4. Change only the requested, hardware-supported field and write its register.
5. Check the response type, register, payload, and checksum; obtain a readback when ownership remains valid.

For example, the source records `5D 00 80 00 18 C5` with response `AD 00 80 00 18 35`: state 2 becomes `80`, representing split flow enabled with the other state-2 fields clear. It is a frame-format example, **not a generally appropriate state to apply**. A write replaces a register's full state byte; it must preserve unrelated fields from a valid baseline. [S14, captured exchange](sources.md#s14)

## State-byte fields

| Register | Bits | Meaning supported by the serial/Bluetooth investigation |
|---|---|---|
| `17` | 7 | Continuous-pilot selection |
| `17` | 6-4 | Light level, 0-6 |
| `17` | 3-2 | Reserved in the documented command layout |
| `17` | 1 | Thermostat flag |
| `17` | 0 | Power flag |
| `18` | 7 | Split flow |
| `18` | 6-4 | Comfort fan, 0-6 |
| `18` | 3 | Auxiliary relay |
| `18` | 2-0 | Flame level, 0-6 |

[S14, register mapping](sources.md#s14); [S16, commands 0x27 and 0x28](sources.md#s16). Feature availability depends on the appliance. Value 7 is not an ordinary documented level.

There is an important naming conflict: the ESPHome header represents the low nibble of state 1 as `OFF=0`, `ON=1`, `THERMOSTAT=2`, `SMART=3`, while the Bonaparte/RF documentation separates power and thermostat bits. Treat raw values as raw values; these enum labels do not prove that all IFCs implement two distinct autonomous temperature-regulation modes. The observed six-byte messages contain no temperature setpoint. A direct UART thermostat would need further specifications and is outside this research's proposed ordinary-control path. [S15, `Proflame2Mode` and `Proflame2State1`](sources.md#s15); [S16](sources.md#s16)

## State and ownership limitations

Handset override is acceptable and remains enabled. The candidate controller should use these explicit conditions:

| Condition | Required behavior for a future implementation |
|---|---|
| Ownership granted and fresh responses valid | Permit a deliberate supported request; distinguish requested state from readback |
| Ownership denied or handed to RF | Stop writes, show `handset/external control`, invalidate serial state as authoritative |
| Response missing, malformed, or mismatched | Mark state unknown; abandon the operation rather than continuously retrying |
| Ownership later becomes available | Read a fresh baseline; discard pending pre-override commands |
| Startup or reconnection | Observe first; no unsolicited ON, pilot change, or replay of remembered state |

These are proposed integration requirements. They preserve the observed priority rule without unpairing the handset, manipulating selectors, or repeatedly fighting for control. No lockout resets or safety-register writes are included.

The inspected ESPHome code uses a 50 ms command interval and 250 ms receive timeout, and requests ownership during setup/update. Those values and automatic requests are implementation choices, not manufacturer requirements. Its simulator is a model, not hardware evidence. The component is a reference to review, not a ready-to-flash recommendation. [S15, `setup`, `update`, `process_command_queue_`](sources.md#s15)

Serial readback describes registers. It does not independently establish flame presence, successful ignition, gas flow, airflow, or safety-system health. The source specifically reports invalid or stale state under RF ownership. [S14, Limitations](sources.md#s14)

## Questions before implementation

The target board's exact compatibility, connector orientation, electrical isolation, accessory coexistence, response latency, ownership-release behavior, and available registers are not yet verified. Unknown registers and payload bits must not be probed by brute force. See the [passive-first validation plan](validation-plan.md).
