# Hardware design package

The ESP32-S3 IR terminal has three transmit zones, 38 and 56 kHz demodulating receivers, and a 30–60 kHz carrier-learning sensor. The prototype remains unbuilt. CAD schematic capture, PCB routing, electrical-rule checks, fabrication files, and bench measurements remain pending.

- [Hardware design](../docs/hardware-design.md): circuits, calculations, interfaces, and layout constraints.
- [Bring-up procedure](../docs/bring-up.md): staged electrical and optical validation.
- [BOM](bom.csv): quantities for one main board and two emitter harnesses; optional components are identified explicitly.
- [GPIO map](gpio-map.csv): firmware-facing pin allocation.
- [Connection schedule](connections.csv): circuit-level wiring specification.
- [Source register](sources.md): primary component references and verification boundaries.

The component population uses a custom PCB and the ESP32-S3-WROOM-1-N16R8 module. Development-board implementations require a separate pin and power audit; two powered USB supplies must not be connected together. GPIO7 is reserved for an additional receiver input. Wideband reception beyond 30–60 kHz remains unimplemented.
