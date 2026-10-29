# SIT Proflame 2 communications research

Research date: **2026-10-28 (America/Los_Angeles)**. Intended region: **United States**. Primary integration subject: **the wired X0A diagnostic interface**, with normal handset priority retained.

Public evidence is sufficient to specify a **candidate 9600-baud wired controller** for the IFC variants investigated by the Bonaparte and ESPHome projects. It is not sufficient to certify compatibility with an unidentified fireplace. The available record includes message framing, ordinary control registers, request/response examples, checksums, and control-ownership behavior. These are independent reverse-engineering results, not a published SIT serial API. [S14, UART Communication and Limitations](sources.md#s14); [S15, README and source](sources.md#s15)

The U.S. handheld link is nominally **315 MHz RF**, not infrared. Community sources describe a usable RF encoder, but handset-specific checksum constants, timing variation, and unconfirmed acknowledgment behavior make RF a secondary path here. [S01, p. 5](sources.md#s01); [S11](sources.md#s11); [S12](sources.md#s12)

## Reading paths

| Task | Guide |
|---|---|
| Establish the exact handset, IFC, region, and accessory combination | [Hardware and variant identification](hardware-variants.md) |
| Understand controls, startup, shutdown, power loss, and safeguards | [System operation](operation.md) |
| Inspect the primary candidate interface | [Wired X0A protocol reference](wired-protocol.md) |
| Compare supported accessories and third-party methods | [Integration options and command handling](integration.md) |
| Plan a project-specific wired extension with handset priority | [X0A implementation plan and hardware gates](implementation-plan.md) |
| Inspect implemented software and blocked activation | [X0A software, commands and transaction policy](software.md) |
| Review missing target evidence | [Hardware qualification record — all gates pending](hardware-qualification.md) |
| Inspect RF timing, framing, addressing, and checksums | [315 MHz RF reference](rf-protocol.md) |
| Plan passive observation and an isolated bench investigation | [Validation plan and recorded offline checks](validation-plan.md) |
| Trace a finding to a source | [Source bibliography](sources.md) and [claim/evidence matrix](evidence-matrix.md) |
| Identify the measurements still required | [Open questions](open-questions.md) |
| Inspect retained originals and source code | [Source archive](sources/README.md) |

## Scope and evidence status

**Specified** means stated in a manufacturer or appliance-OEM document. **Reported** means described by an independent investigator. **Source-derived** means visible in inspected code. **Offline-checked** means arithmetic was reproduced using published data. **Proposed** means a future validation procedure. None of these labels establishes physical verification on the intended appliance.

No target handset label, IFC label, fireplace model, hardware revision, or capture was supplied. Model examples in these guides are identification aids, not a determination of installed hardware. Handset override is an accepted operating constraint: a candidate wired integration must yield when control is denied and expose uncertain state while RF control is active. Unpairing or suppressing the handset is not the selected approach.

This package contains research, archived references, offline evidence checks and documentation of the [disabled X0A software backend](software.md). No signals were transmitted, serial devices opened, appliances operated, or upstream controllers executed. No Git commit or push was performed.

[Matter-IR-Terminal](../../README.md) retains optical IR transport and Matter light/fan behavior. Its optional X0A codec/controller, UART service and console interface are implemented, but production activation is blocked by missing qualification. No RF control or native Matter fireplace endpoint is added.

Experimental work must start with passive capture or a qualified, isolated bench arrangement with fuel and ignition outputs safely disconnected under the applicable manufacturer procedure. Protocol acceptance cannot validate ignition or appliance safety. Safeguards, lockouts, and flame supervision remain responsibilities of the original IFC. [S02, pp. 9-12](sources.md#s02)

Coverage is bounded to the sources recorded here. Inaccessible exhibits, proprietary internals, and untested hardware remain explicit gaps; the collection is not an exhaustive product-family specification.
