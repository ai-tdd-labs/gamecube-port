#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
scenario="$repo_root/tests/sdk/card/memcard_backend/host/memcard_backend_unit_001_scenario.c"
out_bin="$repo_root/tests/sdk/card/memcard_backend/actual/memcard_backend_unit_001.bin"

bash "$repo_root/tools/run_host_scenario.sh" "$scenario" >/dev/null

python3 - <<'PY' "$out_bin"
from pathlib import Path
import sys

p = Path(sys.argv[1])
b = p.read_bytes()
def u32be(off): return int.from_bytes(b[off:off+4], "big")

marker = u32be(0x00)
rc_ins = u32be(0x04)
ins0   = u32be(0x08)
sz0    = u32be(0x0C)
rc_wr  = u32be(0x10)
rc_fl  = u32be(0x14)
rc_rd  = u32be(0x18)
w0     = u32be(0x1C)
w1     = u32be(0x20)

assert marker == 0x4D434231, hex(marker)
assert rc_ins == 0, rc_ins
assert ins0 == 1, ins0
assert sz0 == 0x2000, hex(sz0)
assert rc_wr == 0, rc_wr
assert rc_fl == 0, rc_fl
assert rc_rd == 0, rc_rd
assert w0 == 0xDEADBEEF, hex(w0)
assert w1 == 0xCAFEBABE, hex(w1)
print("PASS: memcard_backend_unit_001")
PY

