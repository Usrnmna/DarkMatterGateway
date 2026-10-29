#!/usr/bin/env bash
# Preserve the full host-check entry point; options select phases or suites.
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
exec "${PYTHON:-python3}" tools/test_host.py "$@"
