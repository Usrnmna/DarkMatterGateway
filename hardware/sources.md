# Hardware source register

Reference review date: 2026-10-16. Component specifications and pin functions are source-derived; assembled-circuit operation remains unverified. Ordering suffixes, package drawings, substitutions, and stock require review before procurement. Passive components marked `specification-selected` in the BOM have electrical and package requirements but no fixed supplier.

| Component or subject | Primary reference | Design use |
|---|---|---|
| ESP32-S3 module | [Espressif module datasheet v1.8](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf) | Module pad mapping, N16R8 memory restrictions, supply and antenna integration |
| TSOP38438 / TSOP38456 | [Vishay 82491](https://www.vishay.com/docs/82491/tsop382.pdf) | Receiver pinout, nominal carrier variants, AGC limitations |
| TSMP98000 | [Vishay 82908](https://www.vishay.com/docs/82908/tsmp98000.pdf) | 30–60 kHz carrier output, recommended supply filter and pull-up |
| TSAL6200 | [Vishay 81010](https://www.vishay.com/docs/81010/tsal6200.pdf) | 940 nm emitter, optical angle, current and thermal limits |
| AO3400A | [AOS datasheet](https://www.aosmd.com/res/data_sheets/AO3400A.pdf) | Gate drive rating and MOSFET package |
| Buffer | [TI SN74LVC125A](https://www.ti.com/lit/ds/symlink/sn74lvc125a.pdf) | Tri-state gate isolation and pin mapping |
| Inverter | [TI SN74LVC1G14](https://www.ti.com/lit/ds/symlink/sn74lvc1g14.pdf) | Current-advertisement inversion and DBV pin mapping |
| Type-C logic | [TI TUSB320LAI Rev D](https://www.ti.com/lit/ds/symlink/tusb320lai.pdf) | UFP GPIO mode, integrated Rd, current decode, VBUS detection resistor |
| Always-on LDO | [TI TLV755P Rev D](https://www.ti.com/lit/ds/symlink/tlv755p.pdf) | Fixed 3.3 V variant, decoupling and DBV pin mapping |
| Main power switch | [TI TPS2553 Rev F](https://www.ti.com/lit/ds/symlink/tps2553.pdf) | Current-limit bounds including resistor tolerance, soft start, fault behavior, DBV pin mapping |
| Main buck | [Diodes AP63200/1/3/5, November 2024](https://www.diodes.com/datasheet/download/AP63200-AP63201-AP63203-AP63205.pdf) | Fixed output topology and reference passives |
| L1 | [Coilcraft XAL5030-472MEC](https://www.coilcraft.com/en-us/products/power/shielded-inductors/molded-inductor/xal/xal50xx/xal5030-472/) | Inductance, current capability, DCR and footprint |
| Signal ESD | [TI TPD1E05U06](https://www.ti.com/lit/ds/symlink/tpd1e05u06.pdf) | USB/CC shunt pinout and placement |
| Port TVS | [Littelfuse SMAJ series](https://www.littelfuse.com/~/media/electronics/datasheets/tvs_diodes/littelfuse_tvs_diode_smaj_datasheet.pdf.pdf) | 6 V standoff TVS selection |
| Drain clamp | [Vishay SS12–SS16](https://www.vishay.com/docs/88746/ss12.pdf) | SS14 polarity and electrical ratings |
| Harness diode | [Vishay 1N4148](https://www.vishay.com/docs/81857/1n4148.pdf) | Antiparallel diode selection and cathode marking |
| Input fuse | [Littelfuse 1206L](https://www.littelfuse.com/assetdocs/littelfuse-ptc-1206l-datasheet?assetguid=2b6a1515-d4ee-4c83-8bd4-152b4901b8f5) | Ordering code and temperature-dependent hold current |
| Type-C connector | [GCT USB4105 drawing](https://gct.co/files/drawings/usb4105.pdf) | Pin mapping and PCB footprint |
| Emitter connectors | [JST XH series](https://www.jst-mfg.com/product/index.php?lang=2&series=277) | B2B-XH-A, B6B-XH-A and housings |
| Crimp terminals | [JST XH terminals](https://www.jst-mfg.com/product/index.php?lang=2&series=277) | SXH-001T-P0.6 terminal selection |

## Verification limits

Available evidence is limited to component documentation and circuit calculations. Schematic ERC, PCB DRC, assembly inspection, regulator load tests, USB compliance tests, ESD tests, RF tests, IR range tests, and appliance tests remain pending. The module's existing certifications do not establish certification of the final enclosure, board, or Matter product.

Gas-fireplace and portable-AC model identifiers remain undefined. Manufacturer-manual review, command semantics, IR transport, supported operating modes, installation restrictions, and state feedback remain unresolved.
