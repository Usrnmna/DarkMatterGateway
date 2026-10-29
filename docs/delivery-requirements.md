# Local delivery requirements and scope decisions

## Status and authority

The current scope requires a maintained local artifact exporter (M2), a requirements record (M1), meaningful tooling tests, and verification against actual build output. Production provisioning work (M3) is deferred and stopped. No provisioning implementation or production-readiness claim is included.

The established constraints preserve firmware targets, architecture, hardware roles, GPIO assignments, partition layout, learned-command compatibility, completed S1–S6 work, and unrelated workspace changes. Repository initialization, publication, hosted CI, commits, pushes, releases, device flashing, and irreversible device operations are excluded. S7/S8 remain unchanged.

Existing source, documentation, imported references, and historical implementation choices establish implementation evidence only. They do not establish approval for a product capability or architectural change. Concurrent workspace features are retained as inputs when present; their product acceptance is outside this record.

## Classification of M2–M7

| Item | Classification for this work | Required behavior or remaining decision |
|---|---|---|
| M2 — local artifact export | **Required** | Build and export the existing ESP32-S3 application locally, with validated metadata, safe relative flash paths, source/SDK identity, sizes, checksums, and usage instructions. Acceptance criteria appear below. |
| M3 — production provisioning | **Deferred; work stopped** | Deployment stage, identity ownership, certificate source, key custody, and production acceptance remain unresolved. No further investigation or implementation is included. |
| M4 — waveform import/restoration | **Out of scope for this task; product requirement unresolved** | A future requirement must define restoration inputs, compatibility, overwrite semantics, and recovery acceptance. Existing export/display capability does not settle this decision. |
| M5 — OTA installation/recovery | **Out of scope for this task; product requirement unresolved** | A future requirement must select update delivery, verification, rollback/recovery, and retention behavior. Reserved slots do not imply an approved OTA workflow. |
| M6 — additional controls/protocols | **Out of scope for this task; further product requirements unresolved** | Per-appliance evidence and explicit control/transport scope are needed for additional work. Separate existing or concurrent implementations are preserved without extending their authorization here. |
| M7 — observation/feedback/expansion | **Out of scope for this task; product requirement unresolved** | Required observations, hardware ownership, and state semantics must be selected before implementation. Reserved pins do not establish intended behavior. |

M1 acceptance is a complete classification and a concrete interface and acceptance criterion for each currently required item. Recording an unresolved future product decision satisfies classification; it does not approve or implement that capability. M3–M7 remain unchecked in the [remaining-work checklist](remaining-work.md#missing-programming).

## M2 deployment and distribution scope

The output is a **local development installation package** for the existing ESP32-S3, 16 MB flash layout. The four flash images include OTA initialization data and the `ota_0` application. Applying the complete set would reinitialize OTA selection; the package is not an OTA update mechanism or a validated field-recovery procedure.

Public distribution, production deployment, signing, and credential provisioning are outside the confirmed scope. A concurrently added `LICENSE` contains GPLv3 text; distribution scope and license-coverage review are not resolved by this local packaging task. SDK development identities are not production-ready. General build packages must exclude private source configuration, captured commands, credentials, caches, and unrelated output. Firmware still contains its compiled settings; packaging does not certify arbitrary application contents as secret-free.

## M2 inputs, outputs, interfaces, and dependencies

- **Inputs:** maintained firmware sources, partition CSV, SDK defaults, dependency lock, selected appliance profile, profile generators, activated pinned SDKs, and a new build directory. The existing default X0A configuration is hashed when present; alternative X0A configuration selection is outside this initial exporter interface.
- **Build interface:** `tools/export_artifacts.py build` runs the existing `idf.py build` with explicit ESP32-S3 and appliance-profile selection and a build-local `sdkconfig`. A successful receipt is written only after source stability, build metadata, generated profile, image, partition, and size checks pass.
- **Re-export interface:** `package` requires the receipt and unchanged source/profile/SDK/build inputs. Older one-off exports and unrelated preexisting build directories cannot be accepted without a fresh recorded build.
- **Verification interface:** `verify` checks a package without access to its original build tree or SDK. Checksums establish consistency, not authenticity.
- **Outputs:** four BIN files, package-relative `flash_args` and `flasher_args.json`, a filtered configuration summary, a machine-readable manifest, SHA-256 checksum list, and concise installation/verification instructions. Source and configuration contents, ELF/map files, and build logs are excluded.
- **Dependencies:** Python 3.10 or later for the exporter and verifier; the existing pinned ESP-IDF/ESP-Matter environment and Git for build/export provenance. Linux/WSL remains the reference target-build environment. No network service, hosted workflow, or additional firmware target is introduced.

The source identity hashes the enumerated maintained build-input hashes; the selected profile has a separate digest. SDK identity records actual Git revisions and requires clean tracked SDK state. The manifest also identifies the exporting tool by SHA-256. This is a local provenance record, not a signed attestation or proof that every compiler/environment input is reproducible.

## M2 acceptance criteria

1. A fresh actual target build completes and produces all four required images. Build metadata, generated configuration, source partition CSV, binary partition table, and both flash argument formats agree.
2. Images pass structural and integrity checks, fit their allowed flash regions, and retain the existing six-partition layout. The application fits both 6 MiB slots; no image writes NVS or `ir_store`.
3. Packaging rejects missing files, stale source/SDK/build data, duplicate JSON keys, linked or unsafe paths, conflicting offsets/flags, additional flash targets, corrupt images/partitions, oversized images, and existing output directories. Failures leave no accepted package.
4. Packages contain only the documented allowlist. Raw private configuration and unrelated files placed beside build outputs are excluded. Manifests contain sizes, SHA-256 values, selected-configuration identity, source hashes, and SDK revisions, without absolute host paths.
5. Re-exporting the same recorded build yields identical package file bytes. Standalone verification checks file inventory, checksums, manifest consistency, flash paths, image integrity, and slot limits. Independent builds are not claimed to produce identical firmware bytes.
6. Meaningful success/failure tests and the complete current host suite pass. Actual target/export evidence and physical-validation limits are recorded in the [validation report](validation.md).

## Outstanding decisions and limits

No additional product decision is required for the explicitly scoped local exporter. Public distribution scope, production deployment, and future M4–M7 requirements remain unresolved here; distribution license coverage is outside this verification. Production provisioning (M3) stays deferred; no assumptions substitute for its stopped decisions. Physical hardware, installation, retention, commissioning, and appliance acceptance remain separate checklist items.

Operational commands and package details are maintained in the [local artifact export guide](artifact-export.md).
