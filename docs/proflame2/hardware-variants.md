# Hardware and variant identification

[Research overview](README.md)

## Identification result

The target is assumed to be used in the United States. **315 MHz is the documented U.S./FCC baseline**, but country alone does not identify an imported or replacement handset. The exact installed transmitter and IFC remain unknown. Read both labels and the appliance parts list before choosing a protocol or cable. [S01, p. 5](sources.md#s01)

| Family or documented example | Identification evidence | Frequency and interface | Compatibility boundary |
|---|---|---|---|
| Earlier GT / GTM / GTMF family | FCC ID `T99058402300`, product code `058402300`, grant dated 2006-06-26; original test-report listing names GT/GTM/GTMF | 315 MHz grant | Earlier filing exhibits do not establish every later IFC or handset revision |
| Proflame 2 handheld `0.584.040` | SIT IFC drawings identify this transmitter | U.S. IFC documents specify 315 MHz | Match enabled functions and exact appliance approval; the drawing does not identify all transmitter circuitry |
| Proflame 2 handheld `0.584.042` | Kingsman/Marquis IPI-MAN2 lists PF2 transmitter `584.042`, IFC `584.325`, and a GTMFL configuration | Same documented PF2 family; regional label still required | OEM-specific parts combination, not a universal replacement rule |
| Wall transmitter `058404300` / `0.584.04300` filing designation | FCC ID `T99058404300`, grant dated 2014-01-28; wall-transmitter manual | 315 MHz FCC version; manufacturer also lists 433.92 MHz CE version | Distinct wall product; do not map retail suffixes to this filing without a label |
| Connect handheld `0.584.080` and wall handset `0.584.090` | Montigo XG0681 p. 2; Connect symbol on label | Works with the documented Connect accessory combination | Different feature/configuration generation from a generic older handset |
| Connect IFC `0.584.665` / Montigo `GC3104` | Montigo XG0681 p. 2, green printed schematic | X0A connection to dongle `0.584.526` / `SIT-WIFI` | Green print is an OEM identification cue, not proof that every green board shares a third-party UART implementation |
| CE handheld described by `9.957.120 00` | SIT transmitter manual | 433.92 MHz | Reference only; not the selected U.S. RF target |

Sources: [S08](sources.md#s08), [S09](sources.md#s09), [S02, p. 6, Figs. 3-4](sources.md#s02), [S06, pp. 3-4](sources.md#s06), [S03, pp. 3 and 10](sources.md#s03), [S05, p. 2](sources.md#s05), [S04, p. 3](sources.md#s04).

The enlarged handset-label photograph in Montigo XG0681 p. 2 identifies `0.584.080` and shows `FCC ID: T99058402300`. Thus the older FCC ID is also visible on a later Connect handset; it must not be used alone to classify the device as Proflame 1 or to prove packet compatibility. The grant date and brand generation are different identifiers. [S05, p. 2, label photograph](sources.md#s05)

An FCC ID identifies the authorized radio equipment, not the fireplace model, complete feature set, or PCB firmware revision. No exact FCC or other radio-certification identifier for the target IFC or an installed accessory can be assigned without its label. A receiver-only IFC should not be assumed to have a transmitter's FCC ID. No absence-of-transmission claim is based solely on a missing grant.

## System configurations

SIT distinguishes Basic, Standard, and Complete configurations, and atmospheric (ATMO) versus fan-assisted (FAN) operation. Basic primarily provides ignition and pilot control; Standard adds remote flame modulation and optional split flow; Complete adds accessory controls. Valve families 880, 886, and 885 appear in the documentation, with different on/off, manual, and motorized modulation arrangements. A shared system name does not imply identical loads or wiring. [S01, pp. 2-5](sources.md#s01)

Feature abbreviations describe enabled functions: T = thermostat, M = flame modulation, F = fan, S = split flow, L = light modulation, A = auxiliary output. A remote menu may expose fewer functions than the generic packet layout. No additional appliance feature is established merely by setting a bit. [S01, p. 4](sources.md#s01)

Some IFCs have an optional external RF/IR adapter or local interface. The label in a diagram does not establish an optical protocol, carrier, pinout, or compatibility with Matter-IR-Terminal. [S01, pp. 7-8](sources.md#s01)

## Required local identity record

Record the following before any future electrical or RF experiment:

| Item | Required record | Current status |
|---|---|---|
| Appliance | Manufacturer, full model, serial range, installation-manual revision, approved accessories | Unknown |
| Transmitter | Full SIT and OEM part numbers, FCC ID, revision, label photograph, Connect symbol, enabled menu items | Unknown |
| IFC | Full part number and revision; label/schematic photograph; ATMO/FAN, FFRT, PoD and Connect indications | Unknown |
| Wiring | X0A availability, keyed connector orientation, ground marking, existing dongle/controller, OEM harness number | Unknown |
| Function set | Flame, fan, lights, AUX, split flow, IPI/CPI, thermostat; absent features explicitly marked | Unknown |
| Radio, if investigated | Label frequency and measured carrier/timing of the owned handset | U.S. nominal 315 MHz only |

Label inspection inside an appliance requires the manufacturer's isolation and service procedure. Several drawings in the public 2018 IFC manual carry a **NOT FOR PRODUCTION** watermark; they are research context, not a substitute for the exact appliance wiring diagram. [S02, pp. 5-7](sources.md#s02)
