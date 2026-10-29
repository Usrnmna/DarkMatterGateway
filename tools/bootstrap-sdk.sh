#!/usr/bin/env bash
# Installs the pinned Linux x86-64 build environment under the project cache.
# Existing SDK checkouts with unexpected revisions are rejected without reset.
set -euo pipefail

if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
    printf '%s\n' 'This bootstrap supports Linux x86-64, including WSL2.' >&2
    exit 1
fi
for task_tool in git python3; do
    command -v "$task_tool" >/dev/null || {
        printf 'Required host tool is unavailable: %s\n' "$task_tool" >&2
        exit 1
    }
done

task_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export IDF_PATH="$task_root/.cache/sdk/esp-idf"
export ESP_MATTER_PATH="$task_root/.cache/sdk/esp-matter"
export IDF_TOOLS_PATH="$task_root/.cache/idf-tools"
export IDF_PYTHON_ENV_PATH="$task_root/.cache/python-build"
export PIP_CACHE_DIR="$task_root/.cache/pip"
export IDF_COMPONENT_CACHE_PATH="$task_root/.cache/component-cache"
task_jobs="${SDK_DOWNLOAD_JOBS:-4}"
mkdir -p "$task_root/.cache/sdk"

# Retrieves an immutable commit through a shallow release-branch clone.
# An existing different revision requires explicit maintenance of that cache.
clone_pinned_sdk() {
    local url="$1" branch="$2" revision="$3" destination="$4"
    if [[ ! -d "$destination" ]]; then
        git clone --no-checkout --depth 1 --filter=blob:none --branch "$branch" \
            "$url" "$destination"
        git -C "$destination" fetch --depth 1 --filter=blob:none origin "$revision"
        git -C "$destination" checkout --detach "$revision"
    fi
    if [[ "$(git -C "$destination" rev-parse HEAD)" != "$revision" ]]; then
        printf 'Unexpected SDK revision in %s; expected %s.\n' "$destination" "$revision" >&2
        return 1
    fi
}

clone_pinned_sdk https://github.com/espressif/esp-idf.git v5.4.1 \
    4c2820d377d1375e787bcef612f0c32c1427d183 "$IDF_PATH"
clone_pinned_sdk https://github.com/espressif/esp-matter.git release/v1.4.2 \
    4cedd4ad40821933d6cc237e80ab6604f2953fb0 "$ESP_MATTER_PATH"
git -C "$IDF_PATH" submodule update --init --depth 1 --recursive --jobs "$task_jobs"
git -C "$ESP_MATTER_PATH" submodule update --init --depth 1 --filter=blob:none
python3 "$ESP_MATTER_PATH/connectedhomeip/connectedhomeip/scripts/checkout_submodules.py" \
    --platform esp32 --shallow --jobs "$task_jobs"

if [[ ! -x "$IDF_PYTHON_ENV_PATH/bin/python" ]]; then
    python3 -m venv "$IDF_PYTHON_ENV_PATH"
fi
"$IDF_PYTHON_ENV_PATH/bin/python" -m pip install cmake==3.30.2 ninja==1.11.1.4

# Both ESP-IDF and the nested Matter build invoke Ninja. A local launcher limits
# their default concurrency; IR_BUILD_JOBS adjusts the per-build limit.
mkdir -p "$task_root/.cache/build-tools"
cat > "$task_root/.cache/build-tools/ninja" <<'NINJA'
#!/usr/bin/env bash
exec "$(dirname "$0")/../python-build/bin/ninja" -j "${IR_BUILD_JOBS:-8}" "$@"
NINJA
chmod +x "$task_root/.cache/build-tools/ninja"
python3 "$IDF_PATH/tools/idf_tools.py" install xtensa-esp-elf esp32ulp-elf esp-rom-elfs
python3 "$IDF_PATH/tools/idf_tools.py" install-python-env

# Only GN is needed from the Pigweed tool bundle for this target build.
# The revision is selected by the pinned CHIP/Pigweed dependency manifest.
python3 - "$task_root/.cache/gn" <<'PYTHON'
import hashlib
from pathlib import Path
import sys
import urllib.request
import zipfile

destination = Path(sys.argv[1])
destination.mkdir(parents=True, exist_ok=True)
archive = destination / "gn.zip"
expected = "12077a82c7bf40b472fa74473905f41dcea52dab28370471b84e0f4bab382abd"
url = ("https://chrome-infra-packages.appspot.com/dl/gn/gn/linux-amd64/+/"
       "git_revision:487f8353f15456474437df32bb186187b0940b45")
if not archive.exists():
    urllib.request.urlretrieve(url, archive)
if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
    raise SystemExit("GN archive checksum mismatch; cached archive was retained for inspection.")
with zipfile.ZipFile(archive) as package:
    (destination / "gn").write_bytes(package.read("gn"))
PYTHON
chmod +x "$task_root/.cache/gn/gn"

"$IDF_PYTHON_ENV_PATH/bin/python" -m pip install \
    lark==1.1.2 stringcase==1.2.0 jinja2==3.1.6 click-option-group
"$IDF_PYTHON_ENV_PATH/bin/python" -m pip install -e \
    "$ESP_MATTER_PATH/connectedhomeip/connectedhomeip/scripts/py_matter_idl"

# Uses Pigweed's own serializer to describe the local Python environment to GN.
# This generated SDK-cache file is normally produced by the full bootstrap.
"$IDF_PYTHON_ENV_PATH/bin/python" <<'PYTHON'
import os
from pathlib import Path
import sys

chip_root = Path(os.environ["ESP_MATTER_PATH"]) / "connectedhomeip/connectedhomeip"
sys.path.insert(0, str(chip_root / "third_party/pigweed/repo/pw_env_setup/py"))
from pw_env_setup.environment import Environment
from pw_env_setup.gni_visitor import GNIVisitor

environment = Environment()
environment.set("VIRTUAL_ENV", os.environ["IDF_PYTHON_ENV_PATH"])
relative_path = "build_overrides/pigweed_environment.gni"
with (chip_root / relative_path).open("w", encoding="utf-8") as output:
    GNIVisitor(str(chip_root), relative_path).serialize(environment, output)
PYTHON
printf '\nSDK setup completed. Activate this shell with:\n  source "%s/tools/activate-sdk.sh"\n' "$task_root"
