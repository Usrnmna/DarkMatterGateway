# Source bibliography

[Research overview](README.md) | [Evidence matrix](evidence-matrix.md) | [Archive and rights notes](sources/README.md)

All sources were accessed **2026-10-28, America/Los_Angeles**; download timestamps may read 2026-10-29 UTC. PDF page references are one-based file pages, including covers. Document revisions and printed dates take precedence over URL upload folders and search-engine dates. A manufacturer-authored PDF hosted by a distributor is identified as such. Independent projects provide original investigations, not SIT specifications.

## S01

**SIT Group, Proflame 2: Basic, Standard, Complete.** Document `9.955.116 02`; source filename/title identifies the 2016-03-03 revision; PDF created 2016-03-14. Manufacturer catalogue and operation reference, 24 pages. [Original PDF](https://www.sitgroup.it/wp-content/uploads/2021/03/584-IFC-9955116_02_Proflame_2_Basic_Standard_Complete_03-03-2016.locked.pdf) · [retained PDF](sources/sit-system-9955116-02.pdf).

Used: pp. 2-5 for configurations, feature abbreviations, handset supply and regional frequencies; pp. 7-8 for system connections; pp. 14-20 for pairing and ordinary controls. Manufacturer evidence does not include an RF bit-level protocol or X0A serial API.

## S02

**SIT Group, Proflame IFC Use and Installation Instructions, FFRT 10.** Document `9.957.201 01`, printed date **2018-10-01**; PDF metadata created 2018-10-16. Manufacturer reference, 14 pages. [Original PDF](https://www.sitgroup.it/wp-content/uploads/2021/04/584-IFC-FFRT-10-Use-and-installation-instructions.pdf) · [retained PDF](sources/sit-ifc-ffrt10-2018.pdf).

Used: p. 2 feature/PoD limits; p. 4 timing and electrical table; pp. 5-7 figures and transmitter identification; p. 8 learning/low-battery operation; p. 9 warnings; pp. 10-11 operating requests and lockout; p. 12 diagnostics and safety restart; p. 13 flame-loss and supply-restoration behavior. The p. 4 table and p. 6 drawings were visually inspected. Several connection drawings are marked **NOT FOR PRODUCTION**. Exact appliance instructions remain necessary.

## S03

**SIT Group, Proflame Wall Mount Transmitter.** Document `9.955.126 A00`, page-production date **2014-02-24**; PDF modified in 2021. Manufacturer manual, 12 pages. [Original PDF](https://www.sitgroup.it/wp-content/uploads/2021/07/Proflame_Wall_Mount_Transmitter-Catalogue.pdf) · [retained PDF](sources/sit-wall-transmitter.pdf).

Used: p. 3 technical data and FCC/CE frequencies; p. 10 FCC statement identifying `T99058404300`. Wall-model identity is not interchangeable with the handheld's FCC ID.

## S04

**SIT Group, Proflame 2 Transmitter, CE version.** Printed document code `9.957.120 00`; filename includes `99570120_00`; PDF created **2012-12-12**, with no separate publication date established. Manufacturer reference, 12 pages. [Original PDF](https://www.sitgroup.it/wp-content/uploads/2021/04/584-Remote-Control-for-IFC-EU-99570120_00.pdf) · [retained PDF](sources/sit-transmitter-ce-9957120-00.pdf).

Used: p. 3 technical data, 433.92 MHz; pp. 5-9 ordinary controls. Regional comparison only; not evidence of U.S. RF timing.

## S05

**Montigo, Proflame Connect WiFi Dongle installation/operation manual.** Revision/footer **`XG0681 - 090222`**; PDF created 2023-06-15. OEM-authored manual hosted by Empire's distributor portal, 8 pages. The numeric footer is retained without assuming a date convention. [Public PDF](https://www.empireinsider.com/wp-content/uploads/2024/06/xg0681-montigo-manual-sit-wifi-full.pdf) · [retained PDF](sources/montigo-wifi-xg0681.pdf).

Used: p. 2 approved component identifiers and label photographs; p. 3 X0A installation; pp. 4-6 app/setup distinction; p. 7 app/cloud state sharing. Page 2 was visually inspected: the `0.584.080` label also shows `T99058402300`. No assertion of universal brand or IFC compatibility follows from this parts list.

## S06

**Kingsman Industries / Marquis, Proflame 2 Remote Control and IPI parts reference.** Footer **`IPI-MAN2`, 2017-10-13**; 7 pages. OEM-authored document hosted by Urban Fireplaces. [Public PDF](https://urbanfp.ca/wp-content/uploads/2016/05/Marquis-ProFlame2-Manual.pdf) · [retained PDF](sources/marquis-proflame2.pdf).

Used: pp. 3-4 parts/configuration lists linking PF2 handset `584.042` and IFC `584.325`, and identifying a separate Basic configuration. The 2016 URL folder is not the document's revision date. No manual-mode reset sequence is adopted into the integration procedure.

## S07

**Montigo, Gas Fireplace Wall Switch FAQ / Switch It Up!** OEM web guidance; publication/revision date not established. [Original article](https://www.montigo.com/blog/gas-fireplace-wall-switch-faq/).

Used: sections on compatible SPST/dry contacts, Wi-Fi switches, X4 connections, and simultaneous remote/wall switching. Establishes no externally applied contact voltage and cautions about parallel controls. A short excerpt and retrieval record are retained in [web evidence](sources/web-evidence.json); the article and images are not mirrored wholesale.

## S08

**FCC equipment authorization for SIT LA PRECISA `T99058402300`, GT-Fire Remote Control System.** Original grant **2006-06-26**, model/product code `058402300`, 315 MHz, rule part 15.231. [SIT-hosted grant URL](https://apps01.sitgroup.it/tds/certif.nsf/referenceID/26A8572A3CB223E4C12574B000311F3C/%24file/GrantT99058402300.pdf) · [filing index and reproduced grant](https://fccid.io/T99058402300) · [original test-report exhibit URL](https://fcc.report/FCC-ID/T99058402300/673118.pdf).

The SIT-hosted grant's text was available through the search index; direct download failed DNS resolution. Filing indexes listed the original manual, schematic, block diagram, operating description, photos, and test report. The test-report listing identifies GT/GTM/GTMF; the full report was not obtained. FCC-mirror requests returned access challenges/403. No inaccessible schematic or internal-photo content is treated as inspected evidence. S05 supplies independent OEM label evidence that this ID also appears on a later Connect handset.

## S09

**FCC equipment authorization for SIT LA PRECISA `T99058404300`, Security/Remote Control Transmitter.** Original grant/application **2014-01-28**, model/product code `058404300`, 315 MHz. [Filing index and reproduced grant](https://fccid.io/T99058404300) · [test-report exhibit](https://fcc.report/FCC-ID/T99058404300/2178867.pdf) · [label exhibit](https://fcc.report/FCC-ID/T99058404300/2178866.pdf) · [internal-photo exhibit](https://fcc.report/FCC-ID/T99058404300/2178864.pdf).

Index/grant text was accessible through web retrieval; PDF exhibit downloads were denied. The wall identity and frequency are corroborated by manufacturer source S03. Internal parts, oscillator implementation, and detailed emissions measurements were not established from these inaccessible exhibits.

## S10

**John Ellinwood, SmartFire.** Original independent RF investigation and Python transmitter. [Pinned repository](https://github.com/johnellinwood/smartfire/tree/aa669ae633c2f1e567bd9db94683119f599e081a), commit **`aa669ae633c2f1e567bd9db94683119f599e081a`**, 2021-11-04; investigation described as 2020 work. [Retained README](sources/community/smartfire/README.upstream.txt), [controller source](sources/community/smartfire/fireplace.py), and [GPLv3 license](sources/community/smartfire/LICENSE.txt).

Used: README Protocol / Packet Examples, plus `build_packet`, `send_packet`, and radio setup. Important conflicts: hard-coded handset constants are not universal; example `51` has reversed prose labels; the README's gap and code's padding differ; the claimed RF echo is unresolved. Code was inspected, not executed.

## S11

**rtl_433 contributors, `src/devices/proflame2.c`.** Original decoder implementation. [Upstream file](https://github.com/merbanan/rtl_433/blob/master/src/devices/proflame2.c) · [retained source](sources/community/rtl_433/proflame2.c) · [GPLv2-or-later license](sources/community/rtl_433/COPYING).

Snapshot: default branch accessed on the research date, Git blob **`2efb605ab53a5f05a889ab238b124931a46f09d2`**. The exact containing commit/publication date was not established; the archive hash identifies the inspected bytes. Used: `proflame2_mc`, `proflame2_decode`, and decoder timing configuration. Its framing/parity checks do not validate the final handset-specific check bytes, despite the output integrity label.

## S12

**Aetf, proflame.** Original RF capture/implementation notes and public packet dataset. [Pinned repository](https://github.com/Aetf/proflame/tree/ac49e3b709de2f17895c3d7f626ff04f6198198b), commit **`ac49e3b709de2f17895c3d7f626ff04f6198198b`**, 2026-10-28; physical-investigation notes dated 2026-08-16/17. Retained: [PROTOCOL](sources/community/aetf/PROTOCOL.upstream.txt), [MAPPING](sources/community/aetf/MAPPING.upstream.txt), [protocol.py](sources/community/aetf/protocol.py), [cmd.csv](sources/community/aetf/cmd.csv), and [power on/off pulse-timing capture](sources/community/aetf/power_on_off.timings.json).

Used: physical layer, frame fields, checksum relation, replay report, controlled control sweeps, and unresolved echo discussion. The 220-row CSV supplies the offline held-out check. Raw timing JSON is retained as published evidence; it was not replayed or independently recaptured. MIT/Apache-2.0 dual-license notice is preserved; this archive includes the [MIT option](sources/community/aetf/LICENSE-MIT) and [COPYRIGHT](sources/community/aetf/COPYRIGHT). Broad upstream claims such as appliance statelessness are not adopted.

## S13

**rtl_433 issue 1905, Proflame 2 support.** Original multi-contributor investigation, opened in 2021 with later additions. [Issue and public sample attachment link](https://github.com/merbanan/rtl_433/issues/1905).

Specific evidence: [symbol timing / decoding](https://github.com/merbanan/rtl_433/issues/1905#issuecomment-992107757); [arbitrary-serial checksum matrix proposal](https://github.com/merbanan/rtl_433/issues/1905#issuecomment-2944061060); [counterexample](https://github.com/merbanan/rtl_433/issues/1905#issuecomment-3421707915); [later receiver-only argument](https://github.com/merbanan/rtl_433/issues/1905#issuecomment-5363895460). These are claims and measurements by their authors, not universal specifications. Comment timestamps were not exposed by the connector and are not invented. The issue's ZIP attachment was identified but not downloaded; S12 supplies retained timing/data samples.

## S14

**Bonaparte, System Information.** Original Napoleon eFIRE/IFC serial investigation, rendered documentation version **1.0.2**, `latest` page; publication date not established. [Original page](https://bonaparte.readthedocs.io/en/latest/system_information.html).

Used: Napoleon W190-0090 hardware, Connectors, UART Communication, Message format, register 0x16, captured exchanges, and Limitations. The page expressly describes serial sniffing, making it original measurement evidence. Exact target IFC part/revision equivalence is not established. UART pinout, ownership, and invalid-state behavior remain reported observations. A short excerpt is retained in [web evidence](sources/web-evidence.json).

## S15

**Felix Kaechele, ESPHome Proflame 2 component.** [Pinned repository](https://github.com/kaechele/esphome-proflame2/tree/bb67453c8782385e2b895274d482e60eb4808934), commit **`bb67453c8782385e2b895274d482e60eb4808934`**, 2026-02-04. Retained [README](sources/community/esphome-proflame2/README.upstream.txt), [implementation](sources/community/esphome-proflame2/proflame2.cpp), [header](sources/community/esphome-proflame2/proflame2.h), and [GPLv3-or-later license](sources/community/esphome-proflame2/LICENSE).

Code files were fetched at the commit; README was retrieved from the then-current default branch and is identified separately by blob hash in the manifest. Used: Hardware Setup, Handheld Remote Priority, `setup`, `update`, `process_command_queue_`, checksum calculation, and packet/state declarations. Its low-nibble mode enum conflicts in naming with separate power/thermostat flags in S14/S16. Automatic ownership requests and source timing choices must not be mistaken for a passive sniffer or OEM requirements.

## S16

**Bonaparte, eFIRE Bluetooth Protocol.** Rendered documentation version **1.0.2**, `latest` page; publication date not established. [Original page](https://bonaparte.readthedocs.io/en/latest/bluetooth_protocol.html).

Used: GATT characteristics, packet framing/checksum, commands `27`/`28` for ordinary IFC state, power/timer and ordinary authentication. Applies to the investigated accessory, not directly to every IFC or handset. Password reset/retrieval is outside the documented integration method.

## S17

**psaab, Proflame WiFi integration and protocol notes.** [Repository](https://github.com/psaab/proflame_wifi) · [upstream protocol](https://github.com/psaab/proflame_wifi/blob/main/PROTOCOL.md). Publication/commit date not established. Default-branch snapshots accessed on the research date are retained as [README](sources/community/proflame_wifi/README.upstream.txt), [PROTOCOL](sources/community/proflame_wifi/PROTOCOL.upstream.txt), and [MIT license](sources/community/proflame_wifi/LICENSE).

Protocol blob **`677626da17744371d3767024d4424b4f7177fc93`** identifies the inspected text. Used: Connection, Initial Connection Sequence, JSON format, Commands, and Status. A single community implementation with template placeholders and unestablished firmware coverage provides a lead, not a verified universal API. No live service was contacted.

## S18

**Texas Instruments, CC1101 Low-Power Sub-1 GHz RF Transceiver.** Datasheet **SWRS061I**, revision I dated **2013-11-05** in the p. 97 revision history; retrieved file includes later packaging appendices (112 PDF pages). [Manufacturer PDF](https://www.ti.com/lit/ds/symlink/cc1101.pdf) · [retained PDF](sources/ti-cc1101.pdf).

Used: pp. 1-2 product description/features, operating-frequency/modulation tables, and serial/asynchronous data-interface sections. Establishes chip capabilities only, not the internal silicon of the SIT handset or suitability of an unspecified finished module. The manufacturer's symlink can change; the manifest hash identifies this download.

## S19

**Great Scott Gadgets, YARD Stick One.** Manufacturer product/technical page, revision date not established. [Original page](https://greatscottgadgets.com/yardstickone/).

Used: half-duplex operation, supported bands/modulations, CC1111 platform, and rfcat firmware. Corrects the loose CC1101 terminology in some community descriptions; no equipment was purchased or operated.

## S20

**Antonio Vendramini / SIT LA PRECISA, USD548703S1, Remote control.** U.S. design patent, filed **2006-06-29**, granted **2007-08-14**. [Patent text and figures](https://patents.google.com/patent/USD548703S1/en) · [USPTO application reference](https://patentcenter.uspto.gov/applications/29247616).

The patent's claimed subject is ornamental remote-control design. It supplies no command framing, timing, serial API, or cryptographic mechanism. It was inspected as an original patent document reproduced by Google Patents; no current ownership/validity conclusion is drawn from the aggregator's legal-status fields. Bounded searches did not produce a patent establishing the target protocol. That is a search result, not proof none exists.

## S21

**SIT Group, 584 Remote Control series.** Manufacturer product page; exposed modification metadata **2021-07-01**, not a guarantee of current hardware revision. [Original page](https://www.sitgroup.it/en/product/584-proflame-system/).

Used to locate manufacturer system/transmitter/IFC documents and establish family context. Detailed technical claims cite the individual PDFs, not a generic product landing page.

## Retrieval limits and coverage

Searches covered SIT/Proflame manufacturer material, appliance-OEM manuals, FCC identifiers and exhibits, original GitHub investigations/captures, component documentation, and patents. Public pages/PDFs were searched and scraped with Firecrawl and web retrieval; GitHub files/comments were fetched through the connected repository tool; downloadable PDFs were parsed locally and selected figures rendered for inspection.

The older [SIT IFC manual URL](https://proflame.sitgroup.it/media/99/t-file/Proflame_IFC-control-board-Use_and_Installation_Instructions.pdf) was readable through the document scraper but failed local certificate verification. The [SIT Wi-Fi manual URL](https://proflame.sitgroup.it/media/96/t-file/Proflame-WIFI-Dongle-Use-and-Installation-Instructions.pdf) also failed certificate verification. These failures were not bypassed; S02 and S05 provide retained references for the claims used here. FCC access challenges were recorded without attempts to defeat them. GitHub archive downloads failed TLS negotiation; individual public files were obtained through the authorized connector instead.

The [manifest](sources/manifest.json) records successful byte counts/SHA-256 hashes and failed attempts. An `unavailable` entry names an attempted destination, not an existing file. No restricted, confidential, paywalled, or unpublished material is claimed. Coverage does not establish every Proflame revision, a complete receiver schematic, a factory protocol specification, or physical compatibility with the intended appliance.
