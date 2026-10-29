#!/usr/bin/env bash
# Sources the workspace-local Linux SDK environment without changing shell profiles.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    printf '%s\n' 'Source this file: source tools/activate-sdk.sh' >&2
    exit 1
fi

export IR_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export IDF_PATH="$IR_PROJECT_ROOT/.cache/sdk/esp-idf"
export ESP_MATTER_PATH="$IR_PROJECT_ROOT/.cache/sdk/esp-matter"
export IDF_TOOLS_PATH="$IR_PROJECT_ROOT/.cache/idf-tools"
export IDF_PYTHON_ENV_PATH="$IR_PROJECT_ROOT/.cache/python-build"
export PIP_CACHE_DIR="$IR_PROJECT_ROOT/.cache/pip"
export IDF_COMPONENT_CACHE_PATH="$IR_PROJECT_ROOT/.cache/component-cache"
export ESP_ROM_ELF_DIR="$IDF_TOOLS_PATH/tools/esp-rom-elfs/20241011"

if [[ ! -f "$IDF_PYTHON_ENV_PATH/bin/activate" ]]; then
    printf '%s\n' 'SDK environment is missing. Run bash tools/bootstrap-sdk.sh first.' >&2
    return 1
fi
source "$IDF_PYTHON_ENV_PATH/bin/activate"
export PATH="$IR_PROJECT_ROOT/.cache/build-tools:$IDF_PATH/tools:$IR_PROJECT_ROOT/.cache/gn:$IDF_TOOLS_PATH/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin:$IDF_TOOLS_PATH/tools/esp32ulp-elf/2.38_20240113/esp32ulp-elf/bin:$PATH"
