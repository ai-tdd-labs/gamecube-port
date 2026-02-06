#!/usr/bin/env bash
set -euo pipefail

# Minimal runner wrapper.
# Today: builds/runs host binaries under tests/runtime via Makefile targets.
# Future: expand to iterate tests/sdk/ metadata.

if [[ $# -lt 1 ]]; then
  echo "Usage: tools/run_tests.sh <make-target> [more targets...]" >&2
  exit 2
fi

make -C tests/runtime "$@"
