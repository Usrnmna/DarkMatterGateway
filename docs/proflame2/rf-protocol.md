# U.S. 315 MHz RF protocol reference

[Research overview](README.md) | [Primary wired interface](wired-protocol.md)

**Status: independently reported protocol with offline-checked arithmetic.** This is a secondary integration path. It does not establish the exact target hardware or permission to radiate a custom transmitter.

## Carrier, modulation, and timing

| Parameter | Evidence | Interpretation |
|---|---|---|
| Nominal U.S. carrier | 315 MHz in SIT documents | Start identification here; not a measured carrier for the target |
| Community tuning | 314.973 MHz in SmartFire | Measurement/configuration value, not a second allocated channel |
| Modulation | ASK/OOK: carrier present or absent | Supported by independent implementations; manufacturer manuals do not specify the baseband format |
| Timing, earlier sources | 2400 symbols/s; about 416.7 microseconds per half-bit; rtl_433 uses 417 | Symbol rate is not payload-bit rate |
| Timing, Aetf captures | 450 microseconds per half-bit | Reported hardware/measurement difference; measure the actual handset |
| Repetition | Commonly five copies per state | Aetf's encoder-side constant is ten; not an OEM requirement |
| Inter-frame silence | About 5 ms in early notes; 4.15 ms in Aetf observations | Source code and final-space accounting differ; retain the actual measured waveform |
| Channel hopping | Not established; reviewed implementations use a fixed carrier | No claim about every revision |

[S01, p. 5](sources.md#s01); [S10, README and radio initialization](sources.md#s10); [S11, decoder timing](sources.md#s11); [S12, Physical layer and implementation constants](sources.md#s12).

SmartFire's README says 12 low symbols separate repetitions, but its code appends nine `Z` symbols, each encoded as `00` (18 low symbols), before radio-level repetition. The radio may add further framing effects. This inconsistency prevents adopting either spacing as a universal requirement. Aetf removes the final low symbol from its returned timing list and accounts for the tail through the repeat gap. [S10, `build_packet` and `send_packet`](sources.md#s10); [S12, `encode_timings`](sources.md#s12)

## Framing

The common decoded payload contains seven bytes:

```text
byte:    0         1          2           3       4       5       6
      identity0 identity1 identity2    command1 command2 check1  check2
```

Some sources call byte 2 `version` and observe `02`; older sources call all three bytes the serial. Preserve the entire observed three-byte identity. `02` is not a manufacturer-confirmed universal generation discriminator. [S11](sources.md#s11); [S12, Frame fields](sources.md#s12); [S13, issue discussion](sources.md#s13)

Each byte is carried in a 26-symbol block:

```text
1110 | Manchester(data[7:0]) | Manchester(flag) | Manchester(parity) | Manchester(1)
 4   +          16         +         2         +         2         +       2
```

`1 -> 10`, `0 -> 01`; data is most-significant bit first. The flag is one only in the first block. Parity makes the eight data bits plus flag plus parity even. The final logical bit is one. The raw `1110` prefix includes the sync violation and start guard; do not Manchester-encode it again. [S11, `proflame2_mc`](sources.md#s11)

Seven blocks contain **182 raw symbols**, not 182 payload bits. Full nominal duration is 75.83 ms at 2400 symbols/s or 81.90 ms at 450 microseconds/symbol. Omitting the last low symbol leaves 81.45 ms at 450 microseconds/symbol. Aetf's prose says about 81.1 ms; exact arithmetic and implementation tail handling are recorded separately in the [offline results](sources/offline-evidence-checks.json).

## Command layout

| Byte | Bits | Field |
|---|---|---|
| command 1 | 7 | Pilot: 1 CPI, 0 IPI request |
| command 1 | 6-4 | Light level 0-6 |
| command 1 | 3-2 | Reserved/zero in reviewed ordinary captures |
| command 1 | 1 | Thermostat flag; not a transmitted temperature setpoint |
| command 1 | 0 | Main power request |
| command 2 | 7 | Split flow (`front` in community code) |
| command 2 | 6-4 | Comfort-fan level 0-6 |
| command 2 | 3 | AUX relay |
| command 2 | 2-0 | Flame level 0-6 |

[S10](sources.md#s10), [S11](sources.md#s11), [S12, Command semantics and MAPPING](sources.md#s12). Aetf reports controlled sweeps for power, flame, fan, light, thermostat, and pilot on one installation; its split-flow and separate AUX fields rely on earlier work because those controls were absent locally. These are source-reported physical results, not tests performed here.

A frame carries a complete requested state, not a relative button action. Replaying a captured fan adjustment also restores the captured power, pilot, flame, and light fields. OFF frames can retain nonzero level fields; do not equate zero flame level with every aspect of power-off behavior. Aetf's broad statement that the appliance is stateless is not adopted: the IFC has internal safety state and the wired interface exposes registers. [S12](sources.md#s12); [S14](sources.md#s14)

## Checksum relation

For each command byte `c` and its check byte `e`, inspected sources support this relation for the sampled handsets:

```text
nibble(n) = (n XOR (n << 5)) AND FF
M(c)      = (c AND F0) XOR nibble(c AND 0F) XOR nibble(c >> 4)
e         = M(c) XOR K
K         = M(c) XOR e_observed
```

Each half has its own handset-specific constant, `K1` and `K2`. A single clean frame supplies candidate constants, but deriving them and validating them against the same frame is tautological. Confirm them on additional, differently valued frames before constructing anything. [S12, Checksums and `derive_key`](sources.md#s12)

The local offline check derives constants from one row for each of five identities, then checks **215 held-out rows / 430 checksum bytes**, with no mismatches. These rows come from a public 220-packet table; they are not new RF captures or independent physical replication. [S12, `cmd.csv`](sources.md#s12); [offline result](sources/offline-evidence-checks.json)

Do not reuse SmartFire's `D0`/`07` constants for another handset. A later 16-mask proposal claimed to generate checksums from arbitrary serials, but a reported frame `4B 52 02 01 06 1D E5` contradicts it: the proposed matrix computes `14 E7`, a difference of `09 02`. That mismatch was reproduced arithmetically here. Arbitrary-identity pairing is therefore not an established general solution. [S13, comments 2944061060 and 3421707915](sources.md#s13)

The inspected rtl_433 decoder validates block framing and parity but does not compare the two final check bytes with a handset-specific expected value, despite emitting `Integrity: CHECKSUM`. Aetf's decoder derives constants afresh from a frame; without comparison against a previously learned identity it likewise does not independently validate the final checksums. Their output labels must not be treated as proof of authentication or full packet integrity. [S11, `proflame2_decode`](sources.md#s11); [S12, `decode_frame`](sources.md#s12)

SmartFire's example `command1=51` is also mislabeled in its prose: bit 0 is one and bit 1 is zero, so the published layout means power ON and thermostat flag clear, not the reverse. The local reference follows the inspected bit layout. [S10, Packet Examples](sources.md#s10)

## Replay, pairing, acknowledgments, and security

Aetf reports successful verbatim replay on its own installation. Repeated identical frames and stable per-handset constants support replay feasibility for the sampled devices. No changing counter, nonce, or cryptographic challenge appears in this format. This is not proof that every revision lacks replay protection, and a checksum is not a secret authentication key. [S12, Transmission is accepted](sources.md#s12)

Reuse of an owned handset's captured identity avoids relying on an unverified arbitrary-serial mapping. It also creates two controllers sending complete states; the next handset command can overwrite a third-party state. That override is acceptable in the selected operating model, but cached state must reflect it.

SmartFire claims an RF echo. Aetf leaves it unresolved; a later contributor argues the IFC is receiver-only. No target-board RF transmitter evidence or independently confirmed RF acknowledgment was obtained. An overlapping burst, handset repetition, or an audible beep is insufficient proof. Do not infer a retry policy based on an expected radio acknowledgment. [S10](sources.md#s10); [S12, echo discussion](sources.md#s12); [S13, comment 5363895460](sources.md#s13)

The inspected sources do not establish a complete factory pairing exchange, identity-to-checksum derivation, universal periodic-refresh interval, lost-link shutdown deadline, or receiver error-correction algorithm. These remain [open questions](open-questions.md).
