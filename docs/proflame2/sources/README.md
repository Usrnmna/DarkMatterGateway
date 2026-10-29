# Retained source archive

[Research overview](../README.md) | [Bibliography and locators](../sources.md) | [Evidence matrix](../evidence-matrix.md)

This directory contains local research references, selected source snapshots, one published timing capture, and offline analysis. It is not a deployable controller package. Archived upstream programs were inspected as text and were not imported or executed. Their instructions, comments, and broad claims remain source material rather than project requirements.

## Inventory and provenance

| Material | Local content |
|---|---|
| Manufacturer/OEM originals | Seven PDFs: SIT system, IFC, wall transmitter, CE transmitter; Montigo Wi-Fi; Marquis PF2; TI CC1101 |
| RF research | SmartFire source/README; rtl_433 decoder; Aetf protocol notes, mapping, encoder/decoder source, 220-row CSV, and published pulse-timing JSON |
| Wired research | ESPHome Proflame 2 header/implementation/README; original Bonaparte pages cited in the bibliography |
| Wi-Fi lead | Community protocol/README snapshots |
| Web provenance | [Selected short excerpts and metadata](web-evidence.json) |
| Integrity | [Retrieval manifest with SHA-256 hashes](manifest.json) |
| Offline analysis | [Reproduction script](check_evidence.py) and [recorded result](offline-evidence-checks.json) |

PDFs are retained as downloaded, including original notices and metadata. Source snapshots were fetched through the GitHub connector and saved as UTF-8 with a normalized final newline. Upstream Git blob hashes identify the retrieved content; local SHA-256 hashes identify the saved bytes. README/protocol snapshots use `.upstream.txt` to keep their relative upstream links and narrative separate from the maintained guides.

An `unavailable` manifest record represents an attempted download, not a retained file. Access challenges, DNS failures, and certificate errors were not bypassed. GitHub archive failures were resolved by fetching selected files through the connector, not by disabling TLS verification.

## Rights and reuse

Manufacturer/OEM documents remain the property of their respective rights holders. Public download availability is not a general redistribution license. Originals are retained for local technical reference; any later publication of this directory requires checking the source notices and applicable reuse terms. In particular, the TI document limits use of its resources to application development involving the described products. No blanket permission to redistribute these PDFs is asserted.

The selected open-source material retains its applicable notices:

| Project | Retained license/notice |
|---|---|
| Aetf/proflame | [MIT option](community/aetf/LICENSE-MIT) and [dual-license notice](community/aetf/COPYRIGHT) |
| johnellinwood/smartfire | [GPLv3](community/smartfire/LICENSE.txt) |
| merbanan/rtl_433 | [GPLv2 or later](community/rtl_433/COPYING) and per-file header |
| kaechele/esphome-proflame2 | [GPLv3 or later](community/esphome-proflame2/LICENSE) and per-file header |
| psaab/proflame_wifi | [MIT](community/proflame_wifi/LICENSE) |

Reference archiving does not incorporate this code into Matter-IR-Terminal firmware or change its architecture or licensing. The independent `check_evidence.py` script only computes arithmetic over local published data and prints JSON. It requires Python 3.10 or later and has no serial, RF, network, subprocess, or upstream-code execution path.

## Evidence limitations

The public CSV and timing JSON are other investigators' captures. Their presence is not proof that a local receiver was tested, that every sample has independent provenance, or that the recorded operation applies to an unidentified IFC. The offline result establishes consistency with those inputs only. The patent and inaccessible FCC exhibits supply no locally inspected receiver schematic or complete factory protocol definition.
