# Codex Workflow

This repository is the memory.

## Core Loop (for a single SDK function)

1) Scoping
- Pick exactly one SDK function.
- Record subsystem and signature.

2) Callsite research
- List all callsites per game decomp (MP4/TP/WW/AC).
- Record call order, arguments, and frequency.

3) Behavior discovery
- Record side effects (memory, globals, registers).
- Record preconditions and edge cases.

4) Unit tests (deterministic)
- At least 3 tests:
  - minimal call
  - realistic game call
  - edge case
- Each test must produce an expected.bin from Dolphin.
  - Current policy: prioritize per-game callsite tests first (start with `mp4/`), and only add `generic/` when we have no realistic callsite yet or when we need an explicit edge-case that the game does not exercise.

5) Expected vs actual
- Run host/runtime port to produce actual.bin.
- Diff expected.bin vs actual.bin.

### Host actual.bin (SDK port)

After `expected/` exists, implement the minimal SDK port for the function under `src/sdk_port/`
and add a matching host testcase under `tests/sdk/<subsystem>/<function>/host/`.

Commands:
1) Dump expected: `tools/run_tests.sh expected tests/sdk/<subsystem>/<function>`
2) Produce actual: `tools/run_host_scenario.sh tests/sdk/<subsystem>/<function>/host/<case>_scenario.c`
3) Diff: `tools/diff_bins.sh tests/sdk/<subsystem>/<function>/expected/<case>.bin tests/sdk/<subsystem>/<function>/actual/<case>.bin`

Important:
- Do **not** run `tools/run_tests.sh expected` in parallel across multiple suites. It starts headless Dolphin instances that share the same GDB stub port and uses `pkill` to clear stale instances, so parallel runs can corrupt expected dumps.

Tip (per-game only):
- Dump/build only MP4 cases: `GAME=mp4 tools/run_tests.sh all tests/sdk/<subsystem>/<function>`

6) Implementation
- Minimal changes only.
- Iterate until bit-exact match.

7) Notes
- Write confirmed behaviors to docs/codex/NOTES.md (facts only, with evidence).

## Test Locations

- New tests go under tests/sdk/<subsystem>/<function>/
- Harness templates and shared boilerplate go under tests/harness/

### Canon test layout (per SDK function)

```
tests/sdk/<subsystem>/<function>/
  dol/
    generic/<case_id>/            # optional: synthetic minimal + edge cases (not tied to a specific game)
    mp4/<case_id>/                # realistic calls (mirrors MP4 callsites/args)
    tp/<case_id>/                 # realistic calls (mirrors TP callsites/args)
    ww/<case_id>/                 # realistic calls (mirrors WW callsites/args)
    ac/<case_id>/                 # realistic calls (mirrors AC callsites/args)
  expected/<test_name>.bin        # Dolphin oracle dumps (bit-exact)
  actual/<test_name>.bin          # Host/sdk_port dumps (bit-exact)
  README.md                       # optional: what the cases represent (keep short)
```

Naming:
- Prefer `<function>_<case_id>` for the built `.dol` base name so expected bins do not collide.
- Use per-game folders for realistic callsite tests. Add `generic/` only when needed.

## Host Workloads (MP4 Decomp Slices)

Besides per-function SDK tests, we also run **MP4 workload scenarios** on host to validate
that larger chunks of the MP4 init/mainloop code are reachable and deterministic when
linked against `src/sdk_port`.

Location:
- `tests/workload/mp4/*_scenario.c` (each scenario produces one `tests/actual/workload/*.bin`)

Runner:
- `bash tools/run_host_scenario.sh tests/workload/mp4/<scenario>_scenario.c`

Important (HuPrc cooperative scheduler on host):
- Use `GC_HOST_JMP_IMPL=asm` for HuPrc* workloads.
- Host `gcsetjmp/gclongjmp` must save the **callsite SP**; saving a wrapper-function SP corrupts
  optimized builds on resume. Therefore the asm implementation provides `gcsetjmp/gclongjmp`
  as leaf AArch64 assembly.
  Evidence: `tests/workload/mp4/slices/jmp_aarch64.S` (`_gcsetjmp`/`_gclongjmp`).

## MP4 Chain Tracking

MP4 init chain status is tracked in:
- `docs/sdk/mp4/MP4_chain_all.csv`

Update the `expected_bins/actual_bins/pass_bins/covered` columns by scanning MP4-only cases (`dol/mp4/*`) with:
- `python3 tools/helpers/update_mp4_chain_csv.py docs/sdk/mp4/MP4_chain_all.csv`

Notes on naming:
- MP4 testcase identity comes from the MP4 dol-case folder name, or (if present) the `.dol` stem / Makefile `TARGET`.
- `expected/*.bin` and `actual/*.bin` are usually named with a suite prefix (e.g. `os_get_arena_lo_mp4_realistic_initmem_001.bin`).
  The helper resolves those automatically.

## Notes Format (Facts Only)

When adding facts to `docs/codex/NOTES.md`, prefer this shape so future Codex runs can grep quickly:

```
### <FunctionName>
- Contract: <one sentence>
  Evidence: <header or decomp path>
- Side effects: <bullets, memory/globals/regs>
  Evidence: <decomp path or expected/actual bins>
- Callsites: <MP4/TP/WW/AC pointers>
  Evidence: docs/sdk/<subsystem>/<FunctionName>.md
- Testcases:
  - <case_id>: <what it represents>
    Evidence: tests/sdk/<subsystem>/<function>/expected/<case>.bin (+ actual if available)
```

For repeated runs, log PASS/FAIL outcomes via:
- `tools/helpers/record_test_result.py` (appends an evidence line to `docs/codex/NOTES.md`)

## Checkpoint Dumps (Smoke Chains + Real RVZ)

There are two different "oracles" we use:

1) **Primary oracle (deterministic):** `sdk_port` running on PPC (Dolphin) vs `sdk_port` running on host.
   This is what the `tests/sdk/smoke/*` suites are for.

2) **Secondary oracle (best effort):** real game (`.rvz` / `.iso`) running in Dolphin, dump at a checkpoint.
   This is useful to sanity-check that the smoke-chain isn't a "toy", but it is more fragile.

Practical Dolphin note (MMU):
- Some smoke DOLs call `OSInit()` and will write exception vectors in low memory (e.g. `0x00000900`).
  Dolphin can warn/crash on these low-memory writes unless MMU is enabled.
  If you see errors like "Invalid write to 0x00000900" or "Unknown instruction at PC=00000900",
  rerun the dump with `tools/ram_dump.py --enable-mmu` (passes `-C Core.MMU=True`).
  If you're launching Dolphin manually in the GUI, enable it via: `Config -> Advanced -> Enable MMU`.

## 1-Button Loop (Automation)

Goal: reduce human steps so we can scale to dozens/hundreds of SDK functions without losing rigor.

Definition: a "1-button loop" is **one command** that:

1) builds the test artifact (DOL and/or host executable)
2) runs Dolphin and dumps `expected/*.bin`
3) runs the host scenario and dumps `actual/*.bin`
4) diffs and reports PASS/FAIL (first mismatch offset)
5) optionally appends facts/evidence links to `docs/codex/NOTES.md`
6) optionally commits (when a task is complete)

Examples in this repo:
- Per-function suites: `tools/run_tests.sh expected ...` + `tools/run_host_scenario.sh ...` + `tools/diff_bins.sh ...`
- Smoke suites: `tools/dump_expected_mem1.sh` + `tools/run_host_smoke.sh` + `tools/diff_bins_smoke.sh`

Rule: if you find yourself repeating a manual sequence more than 2-3 times, add a wrapper script in `tools/`
so the loop becomes a single command.

Smoke DOL build rule:
- Do not `#include` multiple `src/sdk_port/*/*.c` files into a *single* DOL translation unit. Some modules define overlapping enums/macros (example: `VI_NTSC` in `VI.c` vs `SI.c`).
- Instead, add `oracle_*.c` files next to the smoke DOL and include exactly one module per file (see `tests/sdk/smoke/mp4_init_chain_001/dol/mp4/mp4_init_chain_001/oracle_*.c`).

### Real-game checkpoint dumping

On this machine, Dolphin's GDB stub **does** accept `Z0/Z1` packets (GDB remote breakpoints),
so `tools/ram_dump.py --breakpoint <addr>` is the preferred way to reach an exact checkpoint.

If you need "stop on the Nth frame" behavior (example: the same function is called every frame),
use hit-counts:

- `--bp-hit-count N` with `--breakpoint`
- `--pc-hit-count N` with `--pc-breakpoint`

If you cannot rely on breakpoints for a particular run (e.g. address not hit / timing issues), use **PC/NIP polling**:

- Run for a small step, halt, read stop packet, decode PC (reg `0x40`), repeat until PC==target.
- Then dump RAM.

### Host MEM1 dumping (for RVZ comparisons)

For comparing a host workload scenario against an RVZ MEM1 oracle, dump host MEM1 from the
virtual RAM buffer:

- Expected (RVZ at PC): `tools/dump_expected_rvz_mem1_at_pc.sh <rvz_path> <pc_hex> <out_mem1_bin>`
- Actual (host scenario): `tools/dump_actual_mem1.sh <scenario_c> <out_mem1_bin>`
- Diff: `tools/diff_bins.sh <expected_mem1_bin> <actual_mem1_bin>`

Implementation detail: the host runner supports an optional second dump via env vars
(`GC_HOST_DUMP_ADDR`, `GC_HOST_DUMP_SIZE`, `GC_HOST_DUMP_PATH`). See `tests/harness/gc_host_runner.c`.

### RVZ symbol probes (recommended without loader)

Until we implement a real DOL loader on host, do **not** compare full MEM1 images retail-vs-host.
Instead, dump a few RVZ memory windows at a stable PC checkpoint and compare *derived invariants*
to what `sdk_port` reports.

Tool:
- `tools/dump_expected_rvz_probe_at_pc.sh <rvz> <pc_hex> <out_dir> [timeout] [hit_count]`
- `tools/dump_actual_host_probe_at_scenario.sh <scenario_c> <out_dir>`

Important:
- Do not run multiple RVZ probe scripts concurrently. They use `pkill -f "Dolphin -b -d"` to keep the GDB stub deterministic; parallel runs will race and cause `connection refused`.

`dump_actual_host_probe_at_scenario.sh` runs the host scenario once, dumps the full 32 MiB
host RAM buffer, then slices out:
- a BootInfo window (0x80000020..0x80000060)
- the `sdk_state` window (RAM-backed SDK globals page)

It also writes `values.txt` with parsed semantic values (arenas, sampling rate, pad spec).

Default probe windows are currently hardcoded to cover MP4 SDK globals around:
- `__OSCurrHeap`, `__OSArenaLo`, `__OSArenaHi`
- `__PADSpec`, `__PADFixBits`, `SamplingRate`

Addresses come from: `decomp_mario_party_4/config/GMPE01_00/symbols.txt`.

Command pattern:

```bash
python3 tools/ram_dump.py \
  --exec "/path/to/game.rvz" --delay 3 \
  --breakpoint 0x80001234 --bp-timeout 20 \
  --addr 0x80000000 --size 0x01800000 --chunk 0x1000 \
  --out /tmp/game_checkpoint_mem1.bin
```

Notes:
- Use `--chunk 0x1000` for large MEM1 dumps; it's slower but more reliable on Dolphin's stub.
- If you don't know the target PC yet: do a short `--run 0.2 --halt` and log the observed stop packet in `docs/codex/NOTES.md`.

PC polling fallback:

```bash
python3 tools/ram_dump.py \
  --exec "/path/to/game.rvz" --delay 3 \
  --pc-breakpoint 0x80001234 --pc-timeout 20 --pc-step 0.02 \
  --addr 0x80000000 --size 0x01800000 --chunk 0x1000 \
  --out /tmp/game_checkpoint_mem1.bin
```
