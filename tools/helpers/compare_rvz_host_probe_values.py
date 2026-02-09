#!/usr/bin/env python3
"""
Compare a real-game RVZ probe values.txt to a host-probe values.txt.

This is a *semantic* check:
- RVZ prints retail symbol-backed globals (__OSArenaLo, SamplingRate, etc).
- Host prints sdk_state-backed mirrors (os_arena_lo, si_sampling_rate, etc).

We compare the values that should match across both worlds.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


def parse_values(path: Path) -> dict[str, int]:
    d: dict[str, int] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        m = re.match(r"^([A-Za-z0-9_]+)\s*=\s*0x([0-9A-Fa-f]+)$", line)
        if m:
            d[m.group(1)] = int(m.group(2), 16)
    return d


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print("usage: compare_rvz_host_probe_values.py <rvz_values.txt> <host_values.txt>", file=sys.stderr)
        return 2

    rvz_path = Path(argv[1])
    host_path = Path(argv[2])
    rvz = parse_values(rvz_path)
    host = parse_values(host_path)

    # RVZ keys (from dump_expected_rvz_probe_at_pc.sh)
    # Host keys (from dump_actual_host_probe_at_scenario.sh)
    checks = [
        ("arenaLo", "arenaLo"),
        ("arenaHi", "arenaHi"),
        ("__OSCurrHeap", "os_curr_heap"),
        ("__OSArenaLo", "os_arena_lo"),
        ("__OSArenaHi", "os_arena_hi"),
        ("SamplingRate", "si_sampling_rate"),
        ("__PADSpec", "pad_spec"),
    ]

    failed = False
    for rvz_k, host_k in checks:
        if rvz_k not in rvz:
            print(f"SKIP missing RVZ key: {rvz_k}")
            continue
        if host_k not in host:
            print(f"SKIP missing host key: {host_k}")
            continue
        a = rvz[rvz_k]
        b = host[host_k]
        if a != b:
            print(f"FAIL {rvz_k} (rvz)=0x{a:08X} != {host_k} (host)=0x{b:08X}")
            failed = True
        else:
            print(f"OK   {rvz_k} == {host_k} == 0x{a:08X}")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

