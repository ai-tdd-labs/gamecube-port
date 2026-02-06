#!/usr/bin/env bash
set -euo pipefail

# Copy a whitelist of MP4 decomp files into src/game_workload/mp4/vendor/.
#
# This is optional and for local convenience only; the vendor folder is gitignored
# by default.
#
# Usage:
#   tools/sync_mp4_workload.sh

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
decomp_root="$repo_root/decomp_mario_party_4"
list="$repo_root/src/game_workload/mp4/filelist.txt"
out_root="$repo_root/src/game_workload/mp4/vendor"

if [[ ! -d "$decomp_root" ]]; then
  echo "fatal: decomp root not found: $decomp_root" >&2
  exit 2
fi
if [[ ! -f "$list" ]]; then
  echo "fatal: missing file list: $list" >&2
  exit 2
fi

mkdir -p "$out_root"

while IFS= read -r rel; do
  [[ -z "$rel" ]] && continue
  [[ "$rel" =~ ^# ]] && continue

  src="$decomp_root/$rel"
  dst="$out_root/$rel"
  mkdir -p "$(dirname "$dst")"

  if [[ ! -f "$src" ]]; then
    echo "warn: missing source: $src" >&2
    continue
  fi

  cp -f "$src" "$dst"
  echo "[sync] $rel"
done < "$list"

