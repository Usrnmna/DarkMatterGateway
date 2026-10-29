# Repeatable local artifact export

## Scope

[export_artifacts.py](../tools/export_artifacts.py) builds and packages the existing ESP32-S3 firmware for local development. It preserves the 16 MB flash layout and does not flash a device. The [requirements record](delivery-requirements.md) defines acceptance criteria and unresolved delivery decisions. Production provisioning (M3) is deferred.

The package is an initial/local installation set. Its OTA initialization image resets OTA selection to the packaged baseline; it is not an OTA update, retained-state recovery, or production deployment workflow. Hardware acceptance remains in the [bring-up guide](bring-up.md).

## Build and export

From the project root in the existing activated Linux/WSL SDK environment:

```bash
source tools/activate-sdk.sh
python tools/export_artifacts.py build \
  --build-dir firmware/build-export-default \
  --output build/exports/default
```

Both destination paths must be new. Build failure retains a local `artifact-build.log` for diagnosis, without a successful receipt or package. Use a new build directory after a failed attempt. The wrapper selects ESP32-S3, passes the default `config/devices.json`, and creates `sdkconfig` inside that build directory. The workspace's ordinary `firmware/sdkconfig` is not changed.

Select a validated appliance profile explicitly when needed:

```bash
python tools/export_artifacts.py build \
  --build-dir firmware/build-export-selected \
  --profile config/devices.local.json \
  --output build/exports/selected
```

The selected JSON and generated header must agree. The summary records only the profile hash, count, and enabled kinds. Raw JSON, appliance names, mappings, and SDK configuration text are not copied into the package. Compiled firmware can contain configuration values and SDK development identities; inspect inputs before sharing an artifact. Packaging is not a secret scanner or production credential process.

The existing default `config/proflame2.json` and its generator are included in source identity when present. The exporter does not alter that feature, enable it, or support an alternative X0A configuration path in this version. Concurrent source changes are preserved and identified by the build-input hashes.

## Re-export and verification

The successful local receipt is `artifact-build.json` in the build directory. Re-export requires the same source, selected profile, clean tracked SDK state, and unchanged recorded outputs:

```bash
python tools/export_artifacts.py package \
  --build-dir firmware/build-export-default \
  --output build/exports/default-copy
python tools/export_artifacts.py verify build/exports/default-copy
```

Supply the same `--profile` when re-exporting a nondefault build. `--source-root` can identify a staged source tree; the build metadata must refer to that tree. Moving an existing build tree invalidates its absolute internal SDK metadata; copying a completed package does not.

Repeated packaging of unchanged build outputs produces the same file bytes. Output directory names and creation times are not embedded. This is repeatable packaging, not a claim of bit-for-bit firmware reproducibility across separate compilations, machines, or SDK environments. The verifier requires only Python and the maintained tool; it performs no SDK, network, or device operation.

## Package contents

| File | Purpose |
|---|---|
| `bootloader/bootloader.bin` | ESP32-S3 bootloader at `0x0`, bounded below the partition table |
| `partition_table/partition-table.bin` | Existing six-partition layout at `0xc000`; entries and MD5 checked |
| `ota_data_initial.bin` | 8 KiB erased OTA initialization data at `0x21000` |
| `matter_ir_terminal.bin` | Application at `0x130000`; fits each 6 MiB application slot |
| `flash_args`, `flasher_args.json` | Four allowed writes, DIO/80 MHz/16 MB settings, package-relative paths |
| `build-config.json` | Target/flash summary, SDK configuration digest, selected-profile digest and activation summary |
| `manifest.json` | Schema, local-development scope, source/SDK and exporter identity, artifact sizes and SHA-256 values |
| `SHA256SUMS.txt` | SHA-256 checksums for every other package file, including the manifest |
| `README.txt` | Standalone verification and conditional installation instructions and limits |

Only these files are exported. ELF/map files, raw configurations, keys, captured-command data, caches, and logs are excluded. The manifest's source map contains hashes rather than source contents. SDK revision identifiers are read from the existing SDK checkouts; no project Git repository is required or initialized.

The verifier rejects missing/extra files or directories, links/junctions, invalid checksums, inconsistent manifest entries, unsupported targets/settings, unsafe flash paths, changed partitions, image corruption, and region overflow. The app and bootloader checks cover segment bounds, XOR checksum, and appended SHA-256; they do not execute firmware. Checksums and editable local receipts do not provide publisher authentication or protection against a malicious replacement of the whole package.

## Verification and physical limits

```bash
python3 -m unittest discover -s tests -p 'test_export_artifacts.py' -v
bash tools/test-host.sh
```

[Exporter tests](../tests/test_export_artifacts.py) use clearly synthetic SDK metadata, image payloads, and a fake external compiler boundary. Actual target build/export results are recorded separately in the [validation report](validation.md). No test fixture contains real device credentials. Package success does not establish electrical safety, flash retention, Matter commissioning, appliance operation, certification, or distribution rights.
