#!/usr/bin/env bash
set -euo pipefail

# Simple cross-process lock using an atomic mkdir.
# We avoid flock(1) because paths differ across environments; mkdir is universal.
#
# Usage:
#   source tools/helpers/lock.sh
#   acquire_lock "gc-tests" 300
#   ... do work ...
#   release_lock

LOCK_DIR=""

acquire_lock() {
  local name=${1:?lock name required}
  local timeout_s=${2:-300}

  local repo_root
  repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

  local locks_root="$repo_root/tools/.locks"
  local dir="$locks_root/${name}.lock"

  mkdir -p "$locks_root"

  local start
  start="$(date +%s)"

  while ! mkdir "$dir" >/dev/null 2>&1; do
    local now
    now="$(date +%s)"
    if [[ $((now - start)) -ge $timeout_s ]]; then
      echo "fatal: lock busy: $name" >&2
      echo "hint: another replay/mutation run is active." >&2
      echo "hint: if you're sure it's stale, remove: $dir" >&2
      exit 2
    fi
    sleep 1
  done

  LOCK_DIR="$dir"
}

release_lock() {
  if [[ -n "${LOCK_DIR:-}" ]]; then
    rmdir "$LOCK_DIR" >/dev/null 2>&1 || true
    LOCK_DIR=""
  fi
}

