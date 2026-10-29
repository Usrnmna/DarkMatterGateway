# Documentation index

The [project overview](../README.md) describes intended use, capabilities, compatibility, and prototype status. The guides below provide task-specific procedures and engineering detail.

## Setup and operation

| Guide | Contents |
|---|---|
| [Build and programming](build.md) | Pinned dependencies, SDK setup, profile selection, compilation, flashing, and recovery |
| [Local artifact export](artifact-export.md) | Recorded builds, allowlisted development packages, relative flash paths, manifests, and verification |
| [IR learning and operation](operation.md) | Console connection, capture/test/save workflow, commands, commissioning, storage, and capture limits |
| [Appliance configuration](appliance-profiles.md) | Light, fan, fireplace, and portable A/C profiles; verified command mappings and state limitations |
| [Hardware bring-up](bring-up.md) | Staged electrical, optical, appliance, and power-interruption checks with an acceptance-record template |
| [Validation evidence](validation.md) | Recorded software and target-build results, evidence boundaries, and pending physical qualification |

## Engineering references

| Reference | Contents |
|---|---|
| [Hardware package](../hardware/README.md) | Package status and navigation to circuit specifications and schedules |
| [Hardware design](hardware-design.md) | Power budget, receiver and emitter circuits, interfaces, and PCB/enclosure constraints |
| [Bill of materials](../hardware/bom.csv) | Components, quantities, ratings, population options, and source references |
| [Connection schedule](../hardware/connections.csv) | Circuit-level pin-to-net wiring |
| [GPIO map](../hardware/gpio-map.csv) | GPIO numbers, module pads, and electrical connections |
| [Component source register](../hardware/sources.md) | Primary component references and verification limits |
| [ESP-Matter SDK reference](sdk-reference.md) | Dependency baseline, build integration, console wrappers, and endpoint/callback contracts |
| [SIT Proflame 2 communications research](proflame2/README.md) | U.S. hardware identification, X0A/RF evidence, archived sources and isolated-validation plan |
| [Wired X0A software](proflame2/software.md) | Implemented codec/controller, disabled UART service, explicit console controls and verification limits |
| [Wired X0A implementation plan](proflame2/implementation-plan.md) | Project scope, handset-priority policy, hardware prerequisites and bench gates |
| [X0A hardware qualification](proflame2/hardware-qualification.md) | Missing identities, electrical design and measurements; all activation gates pending |

## Configuration and development

| Location | Purpose |
|---|---|
| [Appliance configuration](../config/devices.json) | Four disabled starting profiles with editable identities and aliases |
| [Firmware](../firmware/) | ESP-IDF/ESP-Matter application and portable IR core |
| [Tools](../tools/) | SDK setup, configuration validation, serial client, and host checks |
| [Tests](../tests/) | Portable waveform, direct IR service/storage, Matter/startup, console, serial-client, configuration, and hardware-data checks |
| [Development conventions](../CONTRIBUTING.md) | Source maintainability, documentation style, verification, and repository preparation |
| [Delivery requirements and decisions](delivery-requirements.md) | M1 classification, M2 scope and acceptance criteria, deferred M3, and unresolved future capabilities |
| [Host-check workflow](../.github/workflows/host-checks.yml) | GitHub Actions definition for portable checks |

Hardware schedules specify the proposed circuit; they are not a captured CAD schematic or a fabrication release. Validation claims are limited to the conditions and dates recorded in the validation report.
