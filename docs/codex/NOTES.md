# Codex Notes (Facts Only)

Codex has no memory. The repository is the memory.

Rules:
- Facts only. No guesses.
- Every bullet must cite evidence (decomp path + symbol, or a test id + expected.bin).
- Separate: API contract vs internal implementation vs side effects.

## Confirmed Behaviors

### Integration sync: pbt/remaining-sdk-coverage -> codex/integration-all (2026-02-11)
- Integrated remote commits:
  - `d245941` ("todo: add SDK port coverage overview (69% of game-needed functions)")
  - `169d526` ("todo: add AR module, fix ARQ coverage in SDK overview")
  Evidence: `todo/SDK_PORT_COVERAGE.md`.
- Replay gate robustness: optional harvest scripts that exit with code `2` are treated as skip (missing RVZ/trace corpus), not hard failure.
  Evidence: `tools/run_replay_gate.sh`.
- Validation after integration:
  - `tools/run_pbt_chain_gate.sh` => PASS (including mutation checks).
  - `tools/run_replay_gate.sh` => PASS with explicit SKIPs when assets are unavailable.
  - `tools/run_tests.sh all tests/sdk` => PASS (`rc:0`) in integration-all worktree run log.
  Evidence logs: `/tmp/gc-intall-pbt.log`, `/tmp/gc-intall-replay2.log`, `/tmp/gc-intall-sdk2.log`.

### Pre-main test sweep stabilization (2026-02-11)
- `tests/sdk` full expected sweep now completes end-to-end (`tools/run_tests.sh all tests/sdk`) after resolving PAD test helper symbol collisions.
  Evidence: renamed local dump helpers in:
  - `tests/sdk/pad/pad_reset/dol/mp4/realistic_padreadvsync_001/pad_reset_realistic_padreadvsync_001.c`
  - `tests/sdk/pad/pad_read/dol/mp4/realistic_padreadvsync_001/pad_read_realistic_padreadvsync_001.c`
  - `tests/sdk/pad/pad_clamp/dol/mp4/realistic_padreadvsync_001/pad_clamp_realistic_padreadvsync_001.c`
- Replay gate now treats exit code `2` from optional harvest/replay scripts as a non-fatal "missing external asset/corpus" skip, while preserving hard-fail behavior for real test failures.
  Evidence: `tools/run_replay_gate.sh` (`run_if_exists` rc handling).

### PBT strict dualcheck (MTX)
- Added strict decomp-derived leaf oracle for MTX core math:
  - `strict_C_MTXIdentity`
  - `strict_C_MTXOrtho`
  Evidence: `tests/pbt/mtx/mtx_strict_oracle.h`.
- MTX property test now compares sdk_port output against both invariant formulas and strict decomp leaf oracle output on each iteration.
  Evidence: `tests/pbt/mtx/mtx_core_pbt.c`.
- One-button dualcheck now includes MTX strict run in addition to CARD checksum strict run.
  Evidence: `tools/run_oracle_dualcheck.sh`.

### PBT strict dualcheck (OS round leafs used in OSAlloc flows)
- Added strict decomp-derived leaf oracle for:
  - `OSRoundUp32B`
  - `OSRoundDown32B`
  Evidence: `tests/pbt/os/os_round_32b/os_round_32b_strict_oracle.h`.
- OS round property test now checks sdk_port output against strict oracle output every iteration.
  Evidence: `tests/pbt/os/os_round_32b/os_round_32b_pbt.c`.
- One-button dualcheck now includes the strict OS round suite.
  Evidence: `tools/run_oracle_dualcheck.sh`.

### PBT strict dualcheck (DVDFS read-window leaf arithmetic)
- Added strict leaf oracle for deterministic DVD read-window semantics:
  - invalid offset/len -> fail
  - clamp to file end when `off + len` exceeds file length
  - wrapper return mapping (`ok`, `sync_ret`, `prio_ret`)
  Evidence: `tests/pbt/dvd/dvd_core_strict_oracle.h`.
- `dvd_core_pbt` now derives expected results from the strict leaf oracle instead of ad-hoc inline formulas.
  Evidence: `tests/pbt/dvd/dvd_core_pbt.c`.

### PBT strict dualcheck (ARQ callback normalization leaf)
- Added strict leaf oracle for ARQ callback normalization from decomp semantics:
  callback field is always normalized to non-null (`__ARQCallbackHack` fallback).
  Evidence: `tests/sdk/ar/property/arq_strict_leaf_oracle.h`.
- ARQ property suite now randomizes `has_callback` input and enforces strict normalization parity in addition to existing adapted-oracle parity checks.
  Evidence: `tests/sdk/ar/property/arq_property_test.c`.

### Dolphin GDB Stub (macOS build on this machine)
- Stop packets include PC/NIP in reg `0x40` (usable for PC-polling checkpoints).
  Evidence: `tools/ram_dump.py` `parse_stop_pc()`; observed stop example `T0540:800ba2f0;01:8019d798;` from real MP4 RVZ.
- GDB remote breakpoint packets (`Z0`/`Z1`) are accepted by the stub; `--breakpoint` works.
  Evidence: `tools/ram_dump.py --breakpoint 0x800057c0` hit and returned stop `T0540:800057c0;...` during real MP4 RVZ run.
- Previous "Z0/Z1 not supported" conclusion was a bug in our client: it ignored the `OK` reply because it starts with `O`.
  Evidence: `tools/ram_dump.py` fix after commit `c19d1f5`.
- Trace harvesting (entry/exit snapshots) is possible without game instrumentation:
  break at entry PC, read LR, break at LR, dump small RAM windows, dedupe by input hash.
  Evidence: `tools/trace_pc_entry_exit.py` (uses `Z0/Z1` breakpoints + LR return breakpoint).
- OSDisableInterrupts retail note: the real SDK implementation is MSR-based (register side effects),
  so for RVZ traces we primarily consume **MSR/return-value invariants**, not raw RAM diffs at `0x817FE000`.
  Evidence: `tools/replay_trace_case_os_disable_interrupts.sh` derives expected return from MSR[EE].
- Real MP4 RVZ breakpoint test (USA): breakpoint at `OSDisableInterrupts` address `0x800B723C` hits and stops (`T0540:800b723c;...`).
- RVZ checkpoints can be captured on the Nth call without instrumenting the game by using hit-counts:
  - `tools/ram_dump.py --breakpoint <pc> --bp-hit-count N` (preferred)
  - fallback `--pc-breakpoint <pc> --pc-hit-count N`
  Evidence: `tools/dump_expected_rvz_probe_at_pc.sh ... 0x800CF628 ... 60 2` stops at the 2nd `GXLoadTexMtxImm` call and dumps under `tests/oracles/mp4_rvz/probes/gxloadtexmtximm_pc_800CF628_hit2_v1/`.
  Sanity (semantic compare): `python3 tools/helpers/compare_rvz_host_probe_values.py ... --ignore __PADSpec` vs host scenario `tests/workload/mp4/mp4_mainloop_two_iter_tick_001_scenario.c` dumped under `tests/oracles/mp4_rvz/probes/host_gxloadtexmtximm_mp4_mainloop_two_iter_tick_001_v1/`.
- MP4 post-`HuPadInit` init continues with `HuPerfInit` (perf.c). New SDK calls covered with deterministic PPC-vs-host tests:
  - `OSInitStopwatch` (from decomp `src/dolphin/os/OSStopwatch.c`: sets name/total/hits/min/max only; does not touch running/last).
  - `GXSetDrawSyncCallback` / `GXSetDrawSync` (from decomp `src/dolphin/gx/GXMisc.c`).
  - Suites: `tests/sdk/os/os_init_stopwatch`, `tests/sdk/gx/gx_set_draw_sync_callback`, `tests/sdk/gx/gx_set_draw_sync`.
  Evidence: `<TMP>/mp4_rvz_osdisable_bp_0x80300000.bin` produced by `tools/ram_dump.py --breakpoint 0x800B723C` on `<MP4_RVZ_PATH>`
- MP4 RVZ breakpoint attempt at SDK `GXCopyTex` (`GXCopyTex = 0x800CBA0C`) did not hit within 60 seconds of boot (headless run).
  Evidence: `tools/dump_expected_rvz_mem1_at_pc.sh` output "Breakpoint not hit before timeout."
- Real MP4 RVZ probe checkpoints (small window dumps, not full MEM1):
  - `VISetPostRetraceCallback` entry PC `0x800C0DD8`: `tests/oracles/mp4_rvz/probes/visetpostretracecallback_800C0DD8/values.txt`
  - `SISetSamplingRate` entry PC `0x800DA3C8`: `tests/oracles/mp4_rvz/probes/sisetsamplingrate_800DA3C8/values.txt`
  Evidence: `tools/dump_expected_rvz_probe_at_pc.sh` runs with `--breakpoint` and `Core.MMU=True`.

### MP4 checkpoint smoke chain: mp4_pad_init_chain_001
- Purpose: validate combined SDK effects for the HuPadInit-related subset (SI/VI/PAD + OS interrupt gating) as a checkpoint dump.
- OSDisableInterrupts MP4-realistic test suite: `tests/sdk/os/os_disable_interrupts` now PASSes expected vs actual.
  - Fix applied: DOL Makefile must not include decomp headers, because some decomps ship a nonstandard `<stdint.h>` that breaks `uint32_t` etc.
  Evidence: `tests/sdk/smoke/mp4_pad_init_chain_001/README.md`
- Additional MP4 pad callsite-style suites (used by the MP4 main loop / PadReadVSync path) now have deterministic PPC-vs-host coverage:
  - `tests/sdk/pad/pad_read`
  - `tests/sdk/pad/pad_clamp`
  - `tests/sdk/pad/pad_reset`
  - `tests/sdk/pad/pad_recalibrate`
  Evidence: `tests/sdk/pad/*/expected/*padreadvsync_001.bin` and matching `actual/*`.
- Retail MP4 RVZ input-harvest for `PADRead` (entry PC `0x800C4B88`) requires dumping the pointer argument buffer (stack) via register-relative dumps:
  - `tools/trace_pc_entry_exit.py --dump status:@r3:0x30` (dynamic addr uses entry regs).
  - Observed idle-case invariant in retail: return `0x80000000` and `PADStatus.err` = `[0, -1, -1, -1]` (chan0 present, others absent) with all other fields zero.
  - Replay harness: `tools/replay_trace_case_pad_read.sh <hit_dir>` compares retail `out_status.bin` against host `sdk_port` output buffer.
  Evidence: `tests/trace-harvest/pad_read/mp4_rvz/hit_000001_pc_800C4B88_lr_8005A7F0/` and `tools/replay_trace_case_pad_read.sh`.
- Determinism scope:
  - Primary oracle: `sdk_port` on PPC (Dolphin DOL) vs `sdk_port` on host (virtual RAM) should be bit-exact for the dumped ranges.
  - This does NOT prove retail MP4 correctness; retail correctness needs the RVZ breakpoint dump oracle.
  Evidence: `docs/codex/WORKFLOW.md` ("Checkpoint Dumps")
- Dolphin warning seen sometimes when running tiny DOLs: "Invalid write to 0x00000900" / "Unknown instruction at PC=00000900".
  Interpretation: PPC exception vector `0x00000900` (decrementer/interrupt path) firing before handlers are installed.
  Mitigation in smoke DOLs: use `gc_safepoint()` (disable MSR[EE] + push DEC far future) very early and during loops.
  Evidence: `tests/sdk/smoke/mp4_pad_init_chain_001/dol/mp4/mp4_pad_init_chain_001/mp4_pad_init_chain_001.c`

### MP4 checkpoint smoke chain: mp4_hudata_read_chain_001
- Purpose: validate combined/deterministic behavior of the dvd read subset used by MP4 HuData* helpers (DVDFastOpen -> OSRoundUp32B -> (intended) DCInvalidateRange -> DVDReadAsync -> status -> DVDClose -> OSRoundDown32B).
  Evidence: `docs/sdk/mp4/MP4_chain_all.csv` rows under `HuDvdDataReadWait` + `HuDataReadNumHeapShortForce`.
- Oracle: run the same sdk_port chain on PPC (DOL in Dolphin) and on host (virtual RAM), then compare deterministic dump windows:
  - `0x80300000..` marker/snapshot
  - `0x817FE000..0x81800000` sdk_state page
  Evidence: `tests/sdk/smoke/mp4_hudata_read_chain_001/README.md`; `tools/diff_bins_smoke.sh`.
- Status: PASS (bit-exact for the included dump ranges).
  Evidence: `tools/diff_bins_smoke.sh tests/sdk/smoke/mp4_hudata_read_chain_001/expected/mp4_hudata_read_chain_001_mem1.bin tests/sdk/smoke/mp4_hudata_read_chain_001/actual/mp4_hudata_read_chain_001_mem1.bin`.
- Note: this smoke chain does not call the real `DCInvalidateRange` symbol on PPC because libogc defines it and linking a replacement causes multiple-definition errors. We instead record the intended invalidate parameters into the snapshot window so PPC-vs-host dumps can remain comparable.
  Evidence: `tests/sdk/smoke/mp4_hudata_read_chain_001/dol/mp4/mp4_hudata_read_chain_001/mp4_hudata_read_chain_001.c` (`gc_cache_record_invalidate`).

### MP4 host workload: GWInit reachability
- Workload scenario runs MP4 init chain up to `GWInit()` on host:
  `HuSysInit -> HuPrcInit -> HuPadInit -> GWInit`.
  Evidence: `tests/workload/mp4/mp4_gwinit_001_scenario.c`; `tests/actual/workload/mp4_gwinit_001.bin` marker `MP43 DEADBEEF`.
- `GWInit` is compiled from a deliberately small slice of decomp `gamework.c` (reachability-only).

### MP4 HuPrcInit blocker inventory
- `HuPrcInit` itself is not an SDK blocker: decomp implementation only resets process scheduler globals (`processcnt`, `processtop`) and does not call Nintendo SDK functions.
  Evidence:
  - `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/game/main.c` (call order)
  - `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/game/process.c` (`HuPrcInit` body)
- Practical chain implication: next forward-progress checkpoint remains `GWInit`/post-`GWInit` path, not additional SDK port work for `HuPrcInit`.
  Evidence: `/Users/chrislamark/projects/gamecube/docs/sdk/mp4/HuPrcInit_blockers.md`

### MP4 GWInit checkpoint: RVZ vs host probe
- Real RVZ breakpoint on `GWInit` hits at symbol PC `0x800308B8`.
  Evidence: `tests/oracles/mp4_rvz/probes/gwinit_pc_800308B8_hit1/`.
- Host workload checkpoint `mp4_gwinit_001_scenario` runs and emits probe windows.
  Evidence: `tests/oracles/mp4_rvz/probes/host_gwinit_mp4_gwinit_001/`.
- Semantic probe comparison is aligned for core SDK state fields:
  - `arenaLo`, `arenaHi`
  - `__OSCurrHeap`
  - `__OSArenaLo`
  - `__OSArenaHi`
  Evidence: `tests/oracles/mp4_rvz/probes/host_gwinit_mp4_gwinit_001/compare.txt`.

### MP4 pfInit inventory
- `pfInit`/`pfClsScr` path introduces no uncovered SDK blockers.
- `printfunc.c` contains 40 unique SDK calls (mostly GX draw setup/vertex ops + `VIGetNextField`), and all are already covered in `MP4_chain_all.csv`.
  Evidence:
  - `docs/sdk/mp4/pfInit_blockers.md`
  - `decomp_mario_party_4/src/game/printfunc.c`

### MP4 HuSprInit checkpoint: RVZ vs host probe
- Real RVZ breakpoint on `HuSprInit` hits at symbol PC `0x8000D348`.
  Evidence: `tests/oracles/mp4_rvz/probes/husprinit_pc_8000D348_hit1/`.
- Host workload checkpoint `mp4_husprinit_001_scenario` runs and emits probe windows.
  Evidence: `tests/oracles/mp4_rvz/probes/host_husprinit_mp4_husprinit_001/`.
- Semantic probe comparison aligned for core SDK state fields:
  - `arenaLo`, `arenaHi`
  - `__OSCurrHeap`
  - `__OSArenaLo`
  - `__OSArenaHi`
  Evidence: `tests/oracles/mp4_rvz/probes/host_husprinit_mp4_husprinit_001/compare_husprinit.txt`.

### MP4 Hu3DInit inventory
- In the `Hu3DInit` + immediate `Hu3DPreProc`/`Hu3DExec` window, no new uncovered SDK blockers were found.
- Calls in this window (`GXSetCopyClear`, `GXSetCurrentMtx`, `GXSetDrawDone`, `GXWaitDrawDone`, `GXSetFog`, `GXInvalidateVtxCache`) are already covered.
  Evidence: `docs/sdk/mp4/Hu3DInit_blockers.md`.

### MP4 HuPerfCreate inventory
- `HuPerfCreate` has a single direct SDK dependency: `OSInitStopwatch`.
- That dependency is already covered in the SDK suite set.
  Evidence: `docs/sdk/mp4/HuPerfCreate_blockers.md`.

### MP4 omMasterInit inventory
- `omMasterInit` direct body has no direct SDK calls, but immediate transitive object-DLL path (`objdll.c`) introduces two uncovered SDK calls:
  - `OSLink`
  - `OSUnlink`
- These are currently the first clear unresolved SDK blockers for forward progression after `WipeInit`.
  Evidence: `docs/sdk/mp4/omMasterInit_blockers.md`.
  Evidence: `tests/workload/mp4/slices/gwinit_only.c` (mirrors decomp `decomp_mario_party_4/src/game/gamework.c` `GWInit` body and helpers).
- Retail MP4 RVZ MEM1 oracle at `GWInit` PC `0x800308B8`:
  - file: `tests/oracles/mp4_rvz/mem1_at_pc_800308B8_gwinit.bin`
  - sha256: `9130214e62b4c4abade670307d903bb9e8329145ca631e73057ea4e7eead0c0e`
  Evidence: `tools/dump_expected_rvz_mem1_at_pc.sh` run with breakpoint `0x800308B8` and MMU enabled.

### MP4 host workload: HuPrc scheduler semantics (deterministic scenarios)
- Host-only context switch implementation for MP4 HuPrc* scenarios uses `GC_HOST_JMP_IMPL=asm`.
  Implementation detail: host `gcsetjmp/gclongjmp` are leaf AArch64 assembly to ensure the saved SP is the *callsite* SP (saving the SP of a C wrapper corrupts optimized builds on resume).
  Evidence: `tests/workload/mp4/slices/jmp_aarch64.S` (`_gcsetjmp`/`_gclongjmp`); `tests/workload/include/game/jmp.h` (host jmp_buf layout comment).
- Scenario `mp4_process_scheduler_001` validates basic priority order (B then A) and process end cleanup.
  Evidence: `tests/workload/mp4/mp4_process_scheduler_001_scenario.c`; output `tests/actual/workload/mp4_process_scheduler_001.bin` (marker `PRCS`, seq `[0xB0B0B0B0, 0xA0A0A0A0]`, A=1, B=1).
- Scenario `mp4_process_sleep_001` validates `HuPrcSleep(2)` yields and resumes after 2 ticks.
  Evidence: `tests/workload/mp4/mp4_process_sleep_001_scenario.c`; output `tests/actual/workload/mp4_process_sleep_001.bin` (marker `PRCS`, seq `[0xB0000001, 0xA0000001, 0xA0000002]`, A=2, B=1).
- Scenario `mp4_process_vsleep_001` validates `HuPrcVSleep()` yields until the next scheduler call and resumes.
  Evidence: `tests/workload/mp4/mp4_process_vsleep_001_scenario.c`; output `tests/actual/workload/mp4_process_vsleep_001.bin` (marker `PRCS`, seq `[0xB0000001, 0xA0000001, 0xA0000002]`, A=2, B=1).

### MP4 vendor import: next translation units staged (not yet linked in workloads)
- Imported additional MP4 decomp translation units into the vendor slice for future incremental replacement of stubs:
  - `src/game_workload/mp4/vendor/src/game/printfunc.c`
  - `src/game_workload/mp4/vendor/src/game/wipe.c`
  - `src/game_workload/mp4/vendor/src/game/objmain.c`
  Evidence: file copies from `<DECOMP_MP4_ROOT>/src/game/`.
- These files are NOT compiled by the host workload runner yet (workloads still compile only `init.c`, `pad.c`, `process.c` + slices).
  Evidence: `tools/run_host_scenario.sh` workload `extra_srcs` list.

### MP4 SDK call inventory (vendor slice)
- Refreshed MP4 vendor SDK-call inventory after staging `printfunc.c`, `wipe.c`, `objmain.c`.
  - Inventory file: `docs/sdk/mp4/MP4_sdk_calls_inventory.csv`
  - Total discovered SDK-like call tokens: 115
  Evidence: `tools/helpers/mp4_sdk_calls_inventory.py` run on the vendor slice.
- Newly discovered SDK-like calls that required adding new callsite-style test suites:
  - `GXPosition2s16` (used in `printfunc.c`) -> `tests/sdk/gx/gx_position_2s16` PASS
  - `GXPosition2u16` (used in `wipe.c`) -> `tests/sdk/gx/gx_position_2u16` PASS
  Evidence: `docs/sdk/mp4/MP4_sdk_calls_inventory.csv` rows for those functions now show `has_test_suite=y`.

### MP4 host workload: pfDrawFonts reachability stub
- `pfDrawFonts()` is MP4 game-specific and GX-heavy; we keep it as a host-safe workload slice that only leaves deterministic breadcrumbs.
  Evidence: `tests/workload/mp4/slices/pfdrawfonts_stub.c`.
- Breadcrumb writes (big-endian):
  - `0x80300020`: call count
  - `0x80300024`: marker `0x464F4E54` (`'FONT'`)
  Evidence: `tests/actual/workload/mp4_mainloop_one_iter_tick_001.bin` (count=1) and `tests/actual/workload/mp4_mainloop_two_iter_tick_001.bin` (count=2).

### MP4 smoke chain: mp4_pfdrawfonts_chain_001 (sdk_port PPC vs host)
- Purpose: validate combined/deterministic behavior of the sdk_port GX immediate-mode mirrors used by MP4 `pfDrawFonts()` call patterns.
  Evidence: `decomp_mario_party_4/src/game/printfunc.c` (pfDrawFonts callsite patterns).
- Oracle: run the same scenario against sdk_port on PPC (DOL in Dolphin) and on host (virtual RAM), then compare full MEM1 dump bit-exact.
  Evidence: `tests/sdk/smoke/mp4_pfdrawfonts_chain_001/expected/mp4_pfdrawfonts_chain_001_mem1.bin` and `tests/sdk/smoke/mp4_pfdrawfonts_chain_001/actual/mp4_pfdrawfonts_chain_001_mem1.bin`.
- Status: PASS (bit-exact MEM1).
  Evidence: `tools/diff_bins_smoke.sh` output for the two bins above.

### RVZ vs host MEM1 dump (GWInit checkpoint)
- RVZ oracle: MEM1 dump at retail MP4 PC `0x800308B8` (`GWInit`) exists as `tests/oracles/mp4_rvz/mem1_at_pc_800308B8_gwinit.bin` (24 MiB).
  Evidence: `tools/dump_expected_rvz_mem1_at_pc.sh` run (recorded earlier in this file under "GWInit reachability").
- Host workload MEM1 dump for `tests/workload/mp4/mp4_gwinit_001_scenario.c` produced `tests/actual/workload/mp4_gwinit_001_mem1.bin` (24 MiB) via `tools/dump_actual_mem1.sh`.
- Full-image diff FAILs immediately after the ignored low-memory prefix:
  - first mismatch at offset `0x00004134` (expected `0x7c`, actual `0x00`).
  Interpretation: expected includes the loaded retail MP4 DOL image; host virtual RAM does not.
  Evidence: `tools/diff_bins.sh tests/oracles/mp4_rvz/mem1_at_pc_800308B8_gwinit.bin tests/actual/workload/mp4_gwinit_001_mem1.bin` output.

### RVZ probe (GWInit checkpoint) for SDK globals
- Probe tool: `tools/dump_expected_rvz_probe_at_pc.sh`.
- RVZ: `<MP4_RVZ_PATH>` at PC `0x800308B8` (`GWInit`).
- Output dir: `tests/oracles/mp4_rvz/probes/gwinit_pc_800308B8/` (see `manifest.sha256`):
  - `os_heap_arena_window.bin` (`0x801D38A0` size `0x80`) sha256 `a51024657eb2224f6c342f65797e7fd8535493bf7e9e49aa676dba306c51ba0d`
  - `os_arena_hi_window.bin` (`0x801D42E0` size `0x40`) sha256 `91947db67dece35f297f38d994fa8b1770567aa6b89c7a5a7f94923cc421f8d2`
  - `pad_si_window.bin` (`0x801D44F0` size `0x180`) sha256 `0246922752041d29fb2034aa1fe5e00472caab49119bfceeb2a66b85df9e998b`
- Parsed values (from `decomp_mario_party_4/config/GMPE01_00/symbols.txt`):
  - `__OSCurrHeap` @ `0x801D38B8` = `0x00000000`
  - `__OSArenaLo`  @ `0x801D38C0` = `0x817FDEA0`
  - `__OSArenaHi`  @ `0x801D42F8` = `0x817FDEA0`
  - `SamplingRate` @ `0x801D4628` = `0x00000000`
  - `__PADSpec`    @ `0x801D450C` = `0x00000000`

#### RVZ vs host semantic check (GWInit checkpoint)
- Fact: Host workload state right before `GWInit` matches the RVZ probe semantics at PC `0x800308B8` for BootInfo + OS arena + SamplingRate.
  Note: `__PADSpec` is intentionally ignored here because retail uses a different `Spec` storage pattern; host uses `sdk_state.pad_spec` for determinism.
  Evidence (RVZ): `tests/oracles/mp4_rvz/probes/gwinit_pc_800308B8_v2/values.txt`
  Evidence (host): `tests/oracles/mp4_rvz/probes/host_gwinit_mp4_hupadinit_001_v2/values.txt` (from `tests/workload/mp4/mp4_hupadinit_001_scenario.c`)
  Evidence (compare): `python3 tools/helpers/compare_rvz_host_probe_values.py <rvz_values> <host_values> --ignore __PADSpec`

### RVZ probe (HuPadInit entry) for SDK globals
- RVZ: same image at PC `0x80005A5C` (`HuPadInit` entry).
- Output dir: `tests/oracles/mp4_rvz/probes/hupadinit_pc_80005A5C/` (see `manifest.sha256`).
- Parsed values (same symbols as above):
  - `__OSArenaLo` / `__OSArenaHi` already `0x817FDEA0` (BootInfo-driven high arena).
  - `SamplingRate` is `0x00000000` because MP4 calls `SISetSamplingRate(0)`.
  - `__PADSpec` is `0x00000000` by design: `PADSetSpec` always clears `__PADSpec` and stores the current spec in `Spec` (not exposed via symbols here).
  Evidence: `decomp_mario_party_4/src/dolphin/pad/Pad.c` (`PADSetSpec`).

#### RVZ vs host semantic check (HuPadInit entry)
- Fact: Host workload state right before `HuPadInit` matches the RVZ probe semantics at PC `0x80005A5C`.
  Evidence (RVZ): `tests/oracles/mp4_rvz/probes/hupadinit_pc_80005A5C_v2/values.txt`
  Evidence (host): `tests/oracles/mp4_rvz/probes/host_hupadinit_mp4_huprcinit_001_v2/values.txt` (from `tests/workload/mp4/mp4_huprcinit_001_scenario.c`)
  Evidence (compare): `python3 tools/helpers/compare_rvz_host_probe_values.py <rvz_values> <host_values>` prints all `OK`.

### RVZ probes (post-GWInit init steps) + matching host checkpoints
- Purpose: confirm the same *semantic* invariants hold deeper into MP4 init without doing raw RVZ-vs-host full-MEM1 diffs.
- RVZ image: `<MP4_RVZ_PATH>`
- Tool: `tools/dump_expected_rvz_probe_at_pc.sh`
- Checkpoints (PC -> function -> RVZ dir -> host checkpoint scenario):
  - `0x8000D348` -> `HuSprInit` entry -> `tests/oracles/mp4_rvz/probes/husprinit_pc_8000D348/` -> host uses `tests/workload/mp4/mp4_gwinit_001_scenario.c` (state right before `HuSprInit`).
  - `0x8001F9AC` -> `Hu3DInit` entry -> `tests/oracles/mp4_rvz/probes/hu3dinit_pc_8001F9AC/` -> host uses `tests/workload/mp4/mp4_husprinit_001_scenario.c` (state right before `Hu3DInit`).
  - `0x80006E38` -> `HuDataInit` entry -> `tests/oracles/mp4_rvz/probes/hudatainit_pc_80006E38/` -> host uses `tests/workload/mp4/mp4_hu3dinit_001_scenario.c` (state right before `HuDataInit`).
  - `0x8002E74C` -> `HuPerfInit` entry -> `tests/oracles/mp4_rvz/probes/huperfinit_pc_8002E74C/` -> host uses `tests/workload/mp4/mp4_hudatainit_001_scenario.c` (state right before `HuPerfInit`).
- Verified invariant at these checkpoints:
  - BootInfo: `arenaLo` (0x80000030) = `0x00000000`, `arenaHi` (0x80000034) = `0x817FDEA0` (RVZ matches host workloads).
  Evidence (RVZ): corresponding `*/bootinfo_window.bin` in each probe dir.
  Evidence (host): `tools/dump_actual_host_probe_at_scenario.sh <scenario> <out_dir>` writes `values.txt` with the same BootInfo values.

#### RVZ vs host semantic check (GWInit checkpoint)
- We do **not** compare retail `.bss` addresses to host directly (host stores SDK state in the RAM-backed `sdk_state` page).
- RVZ at GWInit:
  - BootInfo `arenaHi` (0x80000034) = `0x817FDEA0`.
  - `__OSArenaLo == __OSArenaHi == 0x817FDEA0` (from symbols).
- Host workload at GWInit (`tests/workload/mp4/mp4_gwinit_001_scenario.c`):
  - BootInfo `arenaHi` (0x80000034) = `0x817FDEA0` (seeded via env for workloads).
  - `sdk_state.os_arena_lo == sdk_state.os_arena_hi == 0x817FDEA0` (MATCH).
  - `sdk_state.pad_spec == 0x00000005` (MP4 calls `PADSetSpec(PAD_SPEC_5)`).
  Evidence: `tools/dump_actual_host_probe_at_scenario.sh tests/workload/mp4/mp4_gwinit_001_scenario.c tests/oracles/mp4_rvz/probes/host_gwinit_mp4_gwinit_001_v2/` and `tests/oracles/mp4_rvz/probes/host_gwinit_mp4_gwinit_001_v2/values.txt`.

### VISetNextFrameBuffer
- Callsites enumerated across MP4/TP/WW/AC.
  Evidence: docs/sdk/vi/VISetNextFrameBuffer.md
- Current DOL tests in this repo are scaffolding only (marker/control-flow).
  They do not yet validate real VI side effects (shadow regs / HorVer / HW regs).
  Evidence: tests/sdk/vi/vi_set_next_frame_buffer/dol/*; tests/sdk/vi/vi_set_next_frame_buffer/expected/vi_set_next_frame_buffer_{min,realistic_mp4,edge_unaligned}_001.bin

### VIConfigure
- The deterministic oracle for VIConfigure in this repo records: `changeMode`, `dispPosX`, `dispPosY`, `dispSizeX`, `fbSizeX`, `fbSizeY`, `xfbMode`, and `helper_calls`.
  Evidence: tests/sdk/vi/vi_configure/expected/vi_configure_001.bin; tests/sdk/vi/vi_configure/dol/generic/legacy_001/vi_configure_001.c
- Host sdk_port implementation mirrors the legacy testcase's observable behavior (not a full SDK port yet).
  Evidence: src/sdk_port/vi/VI.c; tests/sdk/vi/vi_configure/actual/vi_configure_001.bin

### VIConfigurePan
- The deterministic oracle for VIConfigurePan records: `panPosX`, `panPosY`, `panSizeX`, `panSizeY`, `dispSizeY`, and `helper_calls` for a VI_NON_INTERLACE + VI_XFBMODE_SF configuration.
  Evidence: tests/sdk/vi/vi_configure_pan/expected/vi_configure_pan_001.bin; tests/sdk/vi/vi_configure_pan/dol/generic/legacy_001/vi_configure_pan_001.c
- Host sdk_port implementation mirrors the legacy testcase's observable behavior (not a full SDK port yet).
  Evidence: src/sdk_port/vi/VI.c; tests/sdk/vi/vi_configure_pan/actual/vi_configure_pan_001.bin

### OSSetArenaLo
- MP4 decomp implementation is a single store: `__OSArenaLo = addr;`.
  Evidence: decomp_mario_party_4/src/dolphin/os/OSArena.c
- Minimal test shows `OSGetArenaLo()` returns the last value passed to `OSSetArenaLo()`.
  Evidence: tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_min_001.bin
- Double-call edge case: last write wins.
  Evidence: tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_edge_double_call_001.bin
- Realistic fixed-value callsite style (TP OSExec/OSReboot uses 0x81280000) is preserved by `OSSetArenaLo`.
  Evidence: decomp_twilight_princess/src/dolphin/os/OSExec.c; tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_realistic_tp_fixed_001.bin
- Additional per-game representative testcases exist for MP4/WW/AC (callsite-style inputs, still testing OSSetArenaLo store behavior).
  Evidence: docs/sdk/os/OSSetArenaLo.md; tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_realistic_{mp4_mem_alloc_hi,ww_rounddown_hi,ac_stack_round}_001.bin

### OSSetCurrentHeap
- Contract: sets `__OSCurrHeap` and returns the previous value.
  Evidence: decomp_mario_party_4/src/dolphin/os/OSAlloc.c (`OSSetCurrentHeap`); docs/sdk/os/OSSetCurrentHeap.md
- Minimal deterministic test (heap created then set current heap).
  Evidence: tests/sdk/os/os_set_current_heap/expected/os_set_current_heap_generic_min_001.bin
- MP4 InitMem-style sequence is preserved (after `OSInitAlloc` + `OSCreateHeap`).
  Evidence: src/game_workload/mp4/vendor/src/game/init.c (`InitMem`); tests/sdk/os/os_set_current_heap/expected/os_set_current_heap_mp4_realistic_initmem_001.bin

### OSAllocFromHeap
- Contract: allocates `size` bytes from heap handle, returns pointer to user region (header is 0x20 bytes before the returned pointer). Alignment 32 bytes.
  Evidence: decomp_mario_party_4/src/dolphin/os/OSAlloc.c (`OSAllocFromHeap`); docs/sdk/os/OSAllocFromHeap.md
- Minimal deterministic test: two small allocations (0x20, 0x40).
  Evidence: tests/sdk/os/os_alloc_from_heap/expected/os_alloc_from_heap_generic_min_001.bin
- MP4 HuSysInit-style allocation: `OSAlloc(0x100000)` for GX FIFO.
  Evidence: src/game_workload/mp4/vendor/src/game/init.c (`HuSysInit`); tests/sdk/os/os_alloc_from_heap/expected/os_alloc_from_heap_mp4_realistic_fifo_001.bin

### OSSetArenaHi
- Contract: store `__OSArenaHi`; `OSGetArenaHi()` returns last value.
  Evidence: decomp_mario_party_4/src/dolphin/os/OSArena.c; docs/sdk/os/OSSetArenaHi.md; docs/sdk/os/OSGetArenaHi.md
- Minimal deterministic test: set then get.
  Evidence: tests/sdk/os/os_set_arena_hi/expected/os_set_arena_hi_generic_min_001.bin
- Double-call edge case: last write wins.
  Evidence: tests/sdk/os/os_set_arena_hi/expected/os_set_arena_hi_generic_edge_double_call_001.bin

### GXSetChanCtrl
- Contract: packs a channel control bitfield and writes it to XF regs (`idx+14`, and also 16/17 for `GX_COLOR0A0`/`GX_COLOR1A1`).
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXLight.c (`GXSetChanCtrl`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_chan_ctrl/expected/gx_set_chan_ctrl_mp4_init_gx_001.bin

### GXSetChanAmbColor
- Contract: packs RGBA into a 32-bit XF reg (`colIdx+10`) and updates `ambColor[colIdx]`.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXLight.c (`GXSetChanAmbColor`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_chan_amb_color/expected/gx_set_chan_amb_color_mp4_init_gx_001.bin

### GXSetChanMatColor
- Contract: packs RGBA into a 32-bit XF reg (`colIdx+12`) and updates `matColor[colIdx]`.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXLight.c (`GXSetChanMatColor`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_chan_mat_color/expected/gx_set_chan_mat_color_mp4_init_gx_001.bin

### GXSetNumTevStages
- Contract: sets `genMode` field (shift 10, size 4) to `nStages-1` and marks `dirtyState |= 4`.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXTev.c (`GXSetNumTevStages`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_num_tev_stages/expected/gx_set_num_tev_stages_mp4_init_gx_001.bin

### GXLight (GXInitLight* + GXLoadLightObjImm)
- SDK-private `GXLightObj` layout is 64 bytes and contains: Color (u32), `a[3]`, `k[3]`, `lpos[3]`, `ldir[3]`.
  Evidence: `decomp_mario_party_4/src/dolphin/gx/GXLight.c` (`__GXLightObjInt_struct`).
- Deterministic PPC-vs-host coverage exists for:
  - `GXInitLightSpot` (MP4 hsfdraw callsite: cutoff=20.0, GX_SP_COS)
    Evidence: `tests/sdk/gx/gx_init_light_spot/expected/gx_init_light_spot_mp4_hsfdraw_001.bin` and matching `actual/`.
  - `GXInitLightDistAttn` (generic coeff case)
    Evidence: `tests/sdk/gx/gx_init_light_dist_attn/expected/gx_init_light_dist_attn_generic_001.bin` and matching `actual/`.
  - `GXInitSpecularDir` (generic vector case)
    Evidence: `tests/sdk/gx/gx_init_specular_dir/expected/gx_init_specular_dir_generic_001.bin` and matching `actual/`.
  - `GXLoadLightObjImm` mirror (loads into `gc_gx_light_loaded[]`)
    Evidence: `tests/sdk/gx/gx_load_light_obj_imm/expected/gx_load_light_obj_imm_generic_001.bin` and matching `actual/`.

### GXSetTevOp
- Contract: expands a "mode" into calls to `GXSetTevColorIn/AlphaIn` and then sets `GXSetTevColorOp/AlphaOp` (stage 0: `GX_REPLACE` uses TEX).
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXTev.c (`GXSetTevOp`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_tev_op/expected/gx_set_tev_op_mp4_init_gx_001.bin

### GXSetAlphaCompare
- Contract: packs refs/comps/op into a RAS reg with high byte `0xF3` and writes it.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXTev.c (`GXSetAlphaCompare`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_alpha_compare/expected/gx_set_alpha_compare_mp4_init_gx_001.bin

### GXSetBlendMode
- Contract: updates `cmode0` fields for blend/logical/subtract, sets opcode and factors, sets high byte `0x41`, writes RAS reg.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXPixel.c (`GXSetBlendMode`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_blend_mode/expected/gx_set_blend_mode_mp4_init_gx_001.bin

### GXSetAlphaUpdate
- Contract: updates `cmode0` bit 4, writes RAS reg.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXPixel.c (`GXSetAlphaUpdate`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_alpha_update/expected/gx_set_alpha_update_mp4_init_gx_001.bin

### GXSetZCompLoc
- Contract: updates `peCtrl` bit 6, writes RAS reg.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXPixel.c (`GXSetZCompLoc`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_z_comp_loc/expected/gx_set_z_comp_loc_mp4_init_gx_001.bin

### GXSetDither
- Contract: updates `cmode0` bit 2, writes RAS reg.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXPixel.c (`GXSetDither`).
- MP4 callsite-style testcase (expected.bin only for now; host port pending).
  Evidence: tests/sdk/gx/gx_set_dither/expected/gx_set_dither_mp4_init_gx_001.bin

### GXSetCopyClear (MP4 Hu3DPreProc)
- Contract: emits 3 BP/RAS writes (0x4F/0x50/0x51) encoding clear color (R,A then B,G) and 24-bit Z.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXFrameBuf.c (`GXSetCopyClear`).
- MP4 callsite test: `GXSetCopyClear(BGColor={0,0,0,0xFF}, 0xFFFFFF)`.
  Evidence: decomp_mario_party_4/src/game/hsfman.c (`Hu3DPreProc`); tests/sdk/gx/gx_set_copy_clear/expected/gx_set_copy_clear_mp4_hu3d_preproc_001.bin

### GXSetFog (MP4 Hu3DFogClear / shadow path)
- MP4 callsite style (fog clear): `GXSetFog(GX_FOG_NONE, 0,0,0,0, BGColor)` updates fog BP regs 0xEE..0xF2 (fog0..fog3 + fogclr) and writes them as last RAS reg sequence.
  Evidence: decomp_mario_party_4/src/game/hsfman.c (`Hu3DShadowExec` fog clear); decomp_mario_party_4/src/dolphin/gx/GXPixel.c (`GXSetFog`);
  tests/sdk/gx/gx_set_fog/expected/gx_set_fog_mp4_hu3d_fog_clear_001.bin

### GXSetProjection (MP4 Hu3DShadowExec ortho)
- MP4 shadow-copy path sets an orthographic projection after copying the shadow texture:
  `C_MTXOrtho(sp18, 0,1,0,1,0,1); GXSetProjection(sp18, GX_ORTHOGRAPHIC);`
  Evidence: decomp_mario_party_4/src/game/hsfman.c (`Hu3DShadowExec`); tests/sdk/gx/gx_set_projection/expected/gx_set_projection_mp4_shadow_ortho_001.bin
- Observable side effects (deterministic oracle):
  - projType stored
  - projMtx[0..5] packed from the input matrix
  - XF regs 32..38 written (32..37 = projMtx floats, 38 = projType)
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXTransform.c (`GXSetProjection`/`__GXSetProjection`); tests/sdk/gx/gx_set_projection/expected/gx_set_projection_mp4_shadow_ortho_001.bin

### GXSetTexCopySrc (MP4 Hu3DShadowExec small branch)
- MP4 callsite (shadow copy setup, unk_02 <= 0xF0):
  `GXSetTexCopySrc(0, 0, unk_02 * 2, unk_02 * 2)` packs:
  - `cpTexSrc` high byte 0x49 with left/top fields
  - `cpTexSize` high byte 0x4A with (wd-1)/(ht-1)
  Evidence: decomp_mario_party_4/src/game/hsfman.c (`Hu3DShadowExec`); decomp_mario_party_4/src/dolphin/gx/GXFrameBuf.c (`GXSetTexCopySrc`);
  tests/sdk/gx/gx_set_tex_copy_src/expected/gx_set_tex_copy_src_mp4_shadow_small_001.bin

### GXSetTexCopyDst (MP4 Hu3DShadowExec small branch)
- MP4 callsite (shadow copy setup, unk_02 <= 0xF0):
  `GXSetTexCopyDst(unk_02, unk_02, GX_CTF_R8, 1)` packs:
  - `cpTexStride` high byte 0x4D, field0 = `rowTiles * cmpTiles` for format
  - `cpTex` mipmap bit + format fields (used later by `GXCopyTex`)
  - `cpTexZ` flag (0 for GX_CTF_R8)
  Evidence: decomp_mario_party_4/src/game/hsfman.c (`Hu3DShadowExec`); decomp_mario_party_4/src/dolphin/gx/GXFrameBuf.c (`GXSetTexCopyDst`);
  tests/sdk/gx/gx_set_tex_copy_dst/expected/gx_set_tex_copy_dst_mp4_shadow_small_001.bin

### GXCopyTex (MP4 Hu3DShadowExec small branch)
- MP4 callsite (shadow copy):
  `GXCopyTex(dest, GX_TRUE)` writes:
  - BP 0x4B (addr): low 21 bits = `(dest & 0x3FFFFFFF) >> 5`
  - BP 0x52 (cpTex): sets clear bit (bit11) and tex-copy bit14=0
  - On `clear=1`, also emits zmode (0x40...) then a temporary cmode reg (0x42...) with bits0/1 cleared.
  Evidence: decomp_mario_party_4/src/game/hsfman.c (`Hu3DShadowExec`); decomp_mario_party_4/src/dolphin/gx/GXFrameBuf.c (`GXCopyTex`);
  tests/sdk/gx/gx_copy_tex/expected/gx_copy_tex_mp4_shadow_small_001.bin

### GXGetTexBufferSize (MP4 WipeCrossFade)
- MP4 callsite:
  `GXGetTexBufferSize(w, h, GX_TF_RGB565, GX_FALSE, 0)` where `w=fbWidth=640`, `h=efbHeight=480`.
  Returns `0x00096000` bytes for RGB565 (tile shift 2x2, tileBytes=32).
  Evidence: decomp_mario_party_4/src/game/wipe.c (`WipeInit`, `WipeCrossFade`);
  decomp_mario_party_4/src/dolphin/gx/GXTexture.c (`GXGetTexBufferSize`);
  tests/sdk/gx/gx_get_tex_buffer_size/expected/gx_get_tex_buffer_size_mp4_wipe_cross_fade_001.bin

### GXLoadPosMtxImm (MP4 wipe)
- MP4 callsite (wipe):
  `GXLoadPosMtxImm(identity, GX_PNMTX0)` writes FIFO cmd `0x10`, reg `0x000B0000`, then 12 float words (row-major 3x4).
  Evidence: decomp_mario_party_4/src/game/wipe.c (`WipeFrameStill`); decomp_mario_party_4/src/dolphin/gx/GXTransform.c (`GXLoadPosMtxImm`);
  tests/sdk/gx/gx_load_pos_mtx_imm/expected/gx_load_pos_mtx_imm_mp4_wipe_identity_001.bin

### GXBegin / GXEnd (MP4 wipe)
- MP4 callsite (wipe):
  `GXBegin(GX_QUADS, GX_VTXFMT0, 4)` emits a FIFO header:
  - `GX_WRITE_U8(vtxfmt | type)` -> `0x80`
  - `GX_WRITE_U16(nverts)` -> `4`
  `GXEnd()` is a macro barrier (no observable state change for our deterministic model).
  Evidence: decomp_mario_party_4/src/game/wipe.c (`WipeFrameStill`); decomp_mario_party_4/src/dolphin/gx/GXGeometry.c (`GXBegin`);
  tests/sdk/gx/gx_begin/expected/gx_begin_mp4_wipe_quads_001.bin

### GXSetTexCoordGen (MP4 wipe)
- MP4 callsite (wipe):
  `GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY)` calls `GXSetTexCoordGen2(..., GX_FALSE, GX_PTIDENTITY)`.
  For this callsite, the observable end state is:
  - XF texgen ctrl (0x40 + 0): `0x00000280` (row=5 at bits [7..11])
  - XF postmtx/normalize (0x50 + 0): `0x0000003D` (postmtx=125 => 125-64=61)
  - matIdxA TEX0 field (bits [6..11]) = `GX_IDENTITY` (60), so `matIdxA = 60 << 6 = 0x00000F00`
  - `__GXSetMatrixIndex` writes XF reg 24 = `matIdxA`
  Evidence: decomp_mario_party_4/src/game/wipe.c (`WipeFrameStill`); decomp_mario_party_4/include/dolphin/gx/GXGeometry.h (`GXSetTexCoordGen` inline);
  decomp_mario_party_4/src/dolphin/gx/GXAttr.c (`GXSetTexCoordGen2`);
  tests/sdk/gx/gx_set_tex_coord_gen/expected/gx_set_tex_coord_gen_mp4_wipe_frame_still_001.bin

### GXSetTevColor (MP4 wipe)
- MP4 callsite (wipe):
  `GXSetTevColor(GX_TEVSTAGE1, color)` uses numeric `1`, which matches `GX_TEVREG0` (C0).
  It packs:
  - `regRA` addr `224 + id*2` with 11-bit `r` at bits [0..10] and 11-bit `a` at [12..22]
  - `regBG` addr `225 + id*2` with 11-bit `b` at bits [0..10] and 11-bit `g` at [12..22]
  SDK writes `regRA` once then `regBG` three times (last write wins).
  Evidence: decomp_mario_party_4/src/game/wipe.c (`WipeFrameStill`); decomp_mario_party_4/src/dolphin/gx/GXTev.c (`GXSetTevColor`);
  tests/sdk/gx/gx_set_tev_color/expected/gx_set_tev_color_mp4_wipe_c0_001.bin

### GXInitTexObj (MP4 wipe)
- MP4 callsite (wipe):
  `GXInitTexObj(&tex, wipe->copy_data, wipe->w, wipe->h, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE)`.
  For RGB565 and mipmap=false, the observable object fields include:
  - `image0` packs `(width-1)` and `(height-1)` plus `format&0xF` at bits [20..23]
  - `image3` packs `imageBase = (image_ptr >> 5) & 0x01FFFFFF` into bits [0..20]
  - `loadFmt = 2`, `flags |= 2`, `loadCnt = ((ceil(w/4) * ceil(h/4)) & 0x7FFF)`
  Evidence: decomp_mario_party_4/src/game/wipe.c (`WipeFrameStill`); decomp_mario_party_4/src/dolphin/gx/GXTexture.c (`GXInitTexObj`);
  tests/sdk/gx/gx_init_tex_obj/expected/gx_init_tex_obj_mp4_wipe_rgb565_001.bin

### GXSetCurrentMtx (MP4 Hu3DExec)
- Contract: updates `matIdxA` low 6 bits and writes XF reg 24 to the updated `matIdxA` (via `__GXSetMatrixIndex`).
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXTransform.c (`GXSetCurrentMtx`, `__GXSetMatrixIndex`).
- MP4 callsite test: `GXSetCurrentMtx(0)`.
  Evidence: decomp_mario_party_4/src/game/hsfman.c (`Hu3DExec`); tests/sdk/gx/gx_set_current_mtx/expected/gx_set_current_mtx_mp4_hu3d_exec_001.bin

### GXSetDrawDone / GXWaitDrawDone (MP4 Hu3DExec)
- SDK behavior: `GXSetDrawDone()` writes BP `0x45000002` and resets a completion flag; `GXWaitDrawDone()` blocks until the flag is set by a finish interrupt.
  Evidence: decomp_mario_party_4/src/dolphin/gx/GXMisc.c (`GXSetDrawDone`, `GXWaitDrawDone`).
- Current repo model: deterministic no-block shim for `GXWaitDrawDone` (marks complete immediately) to keep host tests deterministic.
  Evidence: src/sdk_port/gx/GX.c (`GXWaitDrawDone`); tests/sdk/gx/gx_wait_draw_done/expected/gx_wait_draw_done_mp4_hu3d_exec_001.bin

## Known Invariants

## Undocumented Quirks

## Smoke Chains (PPC-vs-Host)

### mp4_init_chain_001 / mp4_init_chain_002
- Fact: Smoke DOLs must compile `sdk_port` modules as separate translation units (`oracle_*.c`) to avoid enum/macro clashes (example: `VI_NTSC` in `VI.c` vs `SI.c`).
  Evidence: `tests/sdk/smoke/mp4_init_chain_001/dol/mp4/mp4_init_chain_001/oracle_*.c`.
- Result: Deterministic compare window passes (marker + snapshot + sdk_state page) for both chain suites.
  Evidence:
  - `tests/sdk/smoke/mp4_init_chain_001/expected/mp4_init_chain_001_mem1.bin` vs `tests/sdk/smoke/mp4_init_chain_001/actual/mp4_init_chain_001_mem1.bin`
  - `tests/sdk/smoke/mp4_init_chain_002/expected/mp4_init_chain_002_mem1.bin` vs `tests/sdk/smoke/mp4_init_chain_002/actual/mp4_init_chain_002_mem1.bin`

### MP4 chain: GXInit tail after GXSetDither
- Fact: Dolphin SDK `GXInit()` continues with additional pixel/copy/POKE setup after `GXSetDither()`.
  Evidence: `decomp_mario_party_4/src/dolphin/gx/GXInit.c:332+` (GXSetDstAlpha, GXSetFieldMask/Mode, GXSetCopyClamp, GXPoke*).
- Result: Added MP4-realistic tests for the missing tail setters/pokes and appended them to `docs/sdk/mp4/MP4_chain_all.csv` so the chain stays contiguous.
  Evidence: `docs/sdk/mp4/MP4_chain_all.csv` PlannedBatch4 (126..147) and `tests/sdk/gx/gx_*` suites.

### Real MP4 RVZ breakpoint sanity check
- Fact: Dolphin GDB stub accepts `Z0/Z1` software breakpoints on this machine.
  Evidence: `tools/ram_dump.py --exec "...Mario Party 4 (USA).rvz" --breakpoint 0x800B723C ...` stops at `T0540:800b723c;...`.
- Fact: RVZ MEM1 dump at PC checkpoint works reliably via polling.
  Evidence: `tools/dump_expected_rvz_mem1_at_pc.sh "...Mario Party 4 (USA).rvz" 0x800B723C tests/oracles/mp4_rvz/mp4_rvz_pc_800B723C_mem1_24m.bin`
  - SHA256: `98316b556ec8c60ad775cba9a1d6048e15347a8cbd9fe1422147091f94a637db`

### DVDConvertPathToEntrynum

- Callsite (MP4): `HuDataInit` validates that a list of `data/*.bin` paths exist, and panics if any returns `-1`.
  Evidence: `decomp_mario_party_4/src/game/data.c`
- Current port status: deterministic test backend only (injectable path table), not a full disc FST model yet.
  Evidence: `src/sdk_port/dvd/DVD.c`
- Testcase:
  - `dvd_convert_path_to_entrynum_mp4_hu_data_init_001`: returns stable indices for known MP4 `data/*.bin` paths; returns `-1` for a missing path.
    Evidence: `tests/sdk/dvd/dvd_convert_path_to_entrynum/expected/dvd_convert_path_to_entrynum_mp4_hu_data_init_001.bin` and `tests/sdk/dvd/dvd_convert_path_to_entrynum/actual/dvd_convert_path_to_entrynum_mp4_hu_data_init_001.bin`
  - `dvd_convert_path_to_entrynum_mp4_datadir_table_001`: validates *all* MP4 `datadir_table.h` paths resolve (count=140); returns `-1` for a missing path.
    Evidence: `tests/sdk/dvd/dvd_convert_path_to_entrynum/expected/dvd_convert_path_to_entrynum_mp4_datadir_table_001.bin` and `tests/sdk/dvd/dvd_convert_path_to_entrynum/actual/dvd_convert_path_to_entrynum_mp4_datadir_table_001.bin`

### DVDFastOpen / DVDReadAsync (MP4 HuDvdDataFastRead path)

- Callsites (MP4):
  - `HuDvdDataFastRead(entrynum)` does `DVDFastOpen(entrynum, &file)` then reads `file.length`.
    Evidence: `decomp_mario_party_4/src/game/dvd.c`
  - `HuDataDVDdirDirectRead(fileInfo, dest, len, offset)` does `DVDReadAsync(...)` then polls `DVDGetCommandBlockStatus(&fileInfo->cb)` until it returns 0.
    Evidence: `decomp_mario_party_4/src/game/data.c`
- Current port status: deterministic “virtual disc” backend only (injectable entrynum->bytes table), not a real disc/FST model yet.
  Evidence: `src/sdk_port/dvd/DVD.c` (`gc_dvd_test_set_file`, `DVDFastOpen`, `DVDReadAsync`)
- Testcases (PPC in Dolphin vs host sdk_port):
  - `dvd_fast_open_mp4_hu_dvd_data_fast_read_001`: `DVDFastOpen(0)` sets `file.length=0x40`.
    Evidence: `tests/sdk/dvd/dvd_fast_open/expected/dvd_fast_open_mp4_hu_dvd_data_fast_read_001.bin` and `tests/sdk/dvd/dvd_fast_open/actual/dvd_fast_open_mp4_hu_dvd_data_fast_read_001.bin`
  - `dvd_read_async_mp4_hu_data_dvd_dir_direct_read_001`: `DVDReadAsync(&fi, dest, 0x20, 0x10, NULL)` copies bytes and `DVDGetCommandBlockStatus(&fi.cb)==0`.
    Evidence: `tests/sdk/dvd/dvd_read_async/expected/dvd_read_async_mp4_hu_data_dvd_dir_direct_read_001.bin` and `tests/sdk/dvd/dvd_read_async/actual/dvd_read_async_mp4_hu_data_dvd_dir_direct_read_001.bin`
  - `dvd_read_async_mp4_hu_dvd_data_read_wait_cb_001`: `DVDReadAsync(&fi, dest, 0x20, 0x00, cb)` calls the callback (cb_called=1) and copies bytes.
    Evidence: `tests/sdk/dvd/dvd_read_async/expected/dvd_read_async_mp4_hu_dvd_data_read_wait_cb_001.bin` and `tests/sdk/dvd/dvd_read_async/actual/dvd_read_async_mp4_hu_dvd_data_read_wait_cb_001.bin`

### HuDataReadNumHeapShortForce (MP4 HuDataDVDdirDirect* path)

- Callsite (MP4): `HuDataReadNumHeapShortForce(data_id, num, heap)` opens the directory file via `DVDFastOpen(DataDirStat[dir].file_id, &fileInfo)`,
  reads a small header with `DVDReadAsync` + `DVDGetCommandBlockStatus` polling, then reads the selected data chunk similarly and closes via `DVDClose(&fileInfo)`.
  It also aligns offsets/lengths using `OSRoundUp32B` and `OSRoundDown32B`.
  Evidence: `decomp_mario_party_4/src/game/data.c` (`HuDataDVDdirDirectOpen`, `HuDataDVDdirDirectRead`, `HuDataReadNumHeapShortForce`)
- Chain tracker update: appended these SDK calls as separate rows in `docs/sdk/mp4/MP4_chain_all.csv` so we can gate future work on evidence-based suites.
  Evidence: `docs/sdk/mp4/MP4_chain_all.csv` sections `HuDataReadNumHeapShortForce` and `HuDvdDataReadWait`.

### DCInvalidateRange (MP4 HuDvdDataReadWait path)

- Callsite (MP4): after allocating the read buffer, MP4 calls `DCInvalidateRange(buf, OSRoundUp32B(len))` before issuing `DVDReadAsync`.
  Evidence: `decomp_mario_party_4/src/game/dvd.c`
- Current port status: record-only; does not model cache effects, only tracks last `(addr,len)` for deterministic testing.
  Evidence: `src/sdk_port/os/OSCache.c`
- Testcase:
  - `dc_invalidate_range_mp4_hu_dvd_data_read_wait_001`: records `addr=0x80400000` and `len=0x40`.
    Evidence: `tests/sdk/os/dc_invalidate_range/expected/dc_invalidate_range_mp4_hu_dvd_data_read_wait_001.bin` and `tests/sdk/os/dc_invalidate_range/actual/dc_invalidate_range_mp4_hu_dvd_data_read_wait_001.bin`

## Test Runs (auto)

- Format: `[PASS|FAIL] <label> expected=<path> actual=<path> (first_mismatch=0x........)`
- [PASS] 2026-02-06T09:54:20Z OSSetArenaLo/min_001 expected=tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_min_001.bin actual=tests/sdk/os/os_set_arena_lo/actual/os_set_arena_lo_min_001.bin
- [PASS] 2026-02-06T10:03:35Z OSSetArenaLo/min_001 expected=tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_min_001.bin actual=tests/sdk/os/os_set_arena_lo/actual/os_set_arena_lo_min_001.bin
- [PASS] 2026-02-06T10:06:08Z OSSetArenaLo/os_set_arena_lo_edge_double_call_001 expected=tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_edge_double_call_001.bin actual=tests/sdk/os/os_set_arena_lo/actual/os_set_arena_lo_edge_double_call_001.bin
- [PASS] 2026-02-06T10:06:08Z OSSetArenaLo/os_set_arena_lo_realistic_tp_fixed_001 expected=tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_realistic_tp_fixed_001.bin actual=tests/sdk/os/os_set_arena_lo/actual/os_set_arena_lo_realistic_tp_fixed_001.bin
- [PASS] 2026-02-06T10:06:08Z OSSetArenaLo/os_set_arena_lo_realistic_mp4_mem_alloc_hi_001 expected=tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_realistic_mp4_mem_alloc_hi_001.bin actual=tests/sdk/os/os_set_arena_lo/actual/os_set_arena_lo_realistic_mp4_mem_alloc_hi_001.bin
- [PASS] 2026-02-06T10:06:08Z OSSetArenaLo/os_set_arena_lo_realistic_ww_rounddown_hi_001 expected=tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_realistic_ww_rounddown_hi_001.bin actual=tests/sdk/os/os_set_arena_lo/actual/os_set_arena_lo_realistic_ww_rounddown_hi_001.bin
- [PASS] 2026-02-06T10:06:08Z OSSetArenaLo/os_set_arena_lo_realistic_ac_stack_round_001 expected=tests/sdk/os/os_set_arena_lo/expected/os_set_arena_lo_realistic_ac_stack_round_001.bin actual=tests/sdk/os/os_set_arena_lo/actual/os_set_arena_lo_realistic_ac_stack_round_001.bin
- [PASS] 2026-02-06T10:18:27Z OSGetArenaLo/os_get_arena_lo_min_001 expected=tests/sdk/os/os_get_arena_lo/expected/os_get_arena_lo_min_001.bin actual=tests/sdk/os/os_get_arena_lo/actual/os_get_arena_lo_min_001.bin
- [PASS] 2026-02-06T10:18:27Z OSGetArenaLo/os_get_arena_lo_edge_default_uninit_001 expected=tests/sdk/os/os_get_arena_lo/expected/os_get_arena_lo_edge_default_uninit_001.bin actual=tests/sdk/os/os_get_arena_lo/actual/os_get_arena_lo_edge_default_uninit_001.bin
- [PASS] 2026-02-06T10:18:27Z OSGetArenaLo/os_get_arena_lo_realistic_mem_alloc_001 expected=tests/sdk/os/os_get_arena_lo/expected/os_get_arena_lo_realistic_mem_alloc_001.bin actual=tests/sdk/os/os_get_arena_lo/actual/os_get_arena_lo_realistic_mem_alloc_001.bin
- [PASS] 2026-02-06T10:18:27Z OSGetArenaLo/os_get_arena_lo_realistic_stack_round_001 expected=tests/sdk/os/os_get_arena_lo/expected/os_get_arena_lo_realistic_stack_round_001.bin actual=tests/sdk/os/os_get_arena_lo/actual/os_get_arena_lo_realistic_stack_round_001.bin
- [PASS] 2026-02-06T11:22:25Z OSInitAlloc/os_init_alloc_ac_maxheaps_001 expected=tests/sdk/os/os_init_alloc/expected/os_init_alloc_ac_maxheaps_001.bin actual=tests/sdk/os/os_init_alloc/actual/os_init_alloc_ac_maxheaps_001.bin
- [PASS] 2026-02-06T11:22:25Z OSInitAlloc/os_init_alloc_edge_unaligned_001 expected=tests/sdk/os/os_init_alloc/expected/os_init_alloc_edge_unaligned_001.bin actual=tests/sdk/os/os_init_alloc/actual/os_init_alloc_edge_unaligned_001.bin
- [PASS] 2026-02-06T11:22:25Z OSInitAlloc/os_init_alloc_min_001 expected=tests/sdk/os/os_init_alloc/expected/os_init_alloc_min_001.bin actual=tests/sdk/os/os_init_alloc/actual/os_init_alloc_min_001.bin
- [PASS] 2026-02-06T11:22:25Z OSInitAlloc/os_init_alloc_mp4_realistic_initmem_001 expected=tests/sdk/os/os_init_alloc/expected/os_init_alloc_mp4_realistic_initmem_001.bin actual=tests/sdk/os/os_init_alloc/actual/os_init_alloc_mp4_realistic_initmem_001.bin
- [PASS] 2026-02-06T11:22:25Z OSInitAlloc/os_init_alloc_tp_maxheaps_001 expected=tests/sdk/os/os_init_alloc/expected/os_init_alloc_tp_maxheaps_001.bin actual=tests/sdk/os/os_init_alloc/actual/os_init_alloc_tp_maxheaps_001.bin
- [PASS] 2026-02-06T11:22:25Z OSInitAlloc/os_init_alloc_ww_maxheaps_001 expected=tests/sdk/os/os_init_alloc/expected/os_init_alloc_ww_maxheaps_001.bin actual=tests/sdk/os/os_init_alloc/actual/os_init_alloc_ww_maxheaps_001.bin
- [PASS] 2026-02-06T11:38:40Z OSCreateHeap/os_create_heap_mp4_realistic_initmem_001 expected=tests/sdk/os/os_create_heap/expected/os_create_heap_mp4_realistic_initmem_001.bin actual=tests/sdk/os/os_create_heap/actual/os_create_heap_mp4_realistic_initmem_001.bin
- [PASS] 2026-02-06T11:38:40Z OSCreateHeap/os_create_heap_ww_jkrstdheap_001 expected=tests/sdk/os/os_create_heap/expected/os_create_heap_ww_jkrstdheap_001.bin actual=tests/sdk/os/os_create_heap/actual/os_create_heap_ww_jkrstdheap_001.bin
- [PASS] 2026-02-06T12:09:45Z OSSetCurrentHeap/generic_min_001 expected=tests/sdk/os/os_set_current_heap/expected/os_set_current_heap_generic_min_001.bin actual=tests/sdk/os/os_set_current_heap/actual/os_set_current_heap_generic_min_001.bin
- [PASS] 2026-02-06T12:09:45Z OSSetCurrentHeap/mp4_realistic_initmem_001 expected=tests/sdk/os/os_set_current_heap/expected/os_set_current_heap_mp4_realistic_initmem_001.bin actual=tests/sdk/os/os_set_current_heap/actual/os_set_current_heap_mp4_realistic_initmem_001.bin
- [PASS] 2026-02-06T12:16:08Z OSAllocFromHeap/mp4_realistic_fifo_001 expected=tests/sdk/os/os_alloc_from_heap/expected/os_alloc_from_heap_mp4_realistic_fifo_001.bin actual=tests/sdk/os/os_alloc_from_heap/actual/os_alloc_from_heap_mp4_realistic_fifo_001.bin
- [PASS] 2026-02-06T12:24:14Z OSSetArenaHi/edge_double_call_001 expected=tests/sdk/os/os_set_arena_hi/expected/os_set_arena_hi_generic_edge_double_call_001.bin actual=tests/sdk/os/os_set_arena_hi/actual/os_set_arena_hi_generic_edge_double_call_001.bin
- [PASS] 2026-02-06T19:13:29Z GX batch3/mp4_init_gx_001/gx_set_chan_ctrl expected=tests/sdk/gx/gx_set_chan_ctrl/expected/gx_set_chan_ctrl_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_chan_ctrl/actual/gx_set_chan_ctrl_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_chan_amb_color expected=tests/sdk/gx/gx_set_chan_amb_color/expected/gx_set_chan_amb_color_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_chan_amb_color/actual/gx_set_chan_amb_color_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_chan_mat_color expected=tests/sdk/gx/gx_set_chan_mat_color/expected/gx_set_chan_mat_color_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_chan_mat_color/actual/gx_set_chan_mat_color_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_num_tev_stages expected=tests/sdk/gx/gx_set_num_tev_stages/expected/gx_set_num_tev_stages_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_num_tev_stages/actual/gx_set_num_tev_stages_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_tev_op expected=tests/sdk/gx/gx_set_tev_op/expected/gx_set_tev_op_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_tev_op/actual/gx_set_tev_op_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_alpha_compare expected=tests/sdk/gx/gx_set_alpha_compare/expected/gx_set_alpha_compare_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_alpha_compare/actual/gx_set_alpha_compare_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_blend_mode expected=tests/sdk/gx/gx_set_blend_mode/expected/gx_set_blend_mode_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_blend_mode/actual/gx_set_blend_mode_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_alpha_update expected=tests/sdk/gx/gx_set_alpha_update/expected/gx_set_alpha_update_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_alpha_update/actual/gx_set_alpha_update_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_z_comp_loc expected=tests/sdk/gx/gx_set_z_comp_loc/expected/gx_set_z_comp_loc_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_z_comp_loc/actual/gx_set_z_comp_loc_mp4_init_gx_001.bin
- [PASS] 2026-02-06T19:13:30Z GX batch3/mp4_init_gx_001/gx_set_dither expected=tests/sdk/gx/gx_set_dither/expected/gx_set_dither_mp4_init_gx_001.bin actual=tests/sdk/gx/gx_set_dither/actual/gx_set_dither_mp4_init_gx_001.bin

## GXLoadTexObj (MP4 wipe)

Evidence:
- MP4 callsite: `<DECOMP_MP4_ROOT>/src/game/wipe.c` (`WipeFrameStill`)
- SDK reference:
  - `<DECOMP_MP4_ROOT>/src/dolphin/gx/GXTexture.c` (`GXLoadTexObjPreLoaded` ID injection + 6 BP writes)
  - `<DECOMP_MP4_ROOT>/src/dolphin/gx/GXInit.c` (`__GXDefaultTexRegionCallback` round-robin + `GXInitTexCacheRegion` init pattern)

Test:
- expected (PPC/Dolphin): `tests/sdk/gx/gx_load_tex_obj/expected/gx_load_tex_obj_mp4_wipe_texmap0_001.bin`
- actual (host/sdk_port): `tests/sdk/gx/gx_load_tex_obj/actual/gx_load_tex_obj_mp4_wipe_texmap0_001.bin`

Facts (confirmed by bit-exact expected vs actual):
- `GXInitTexCacheRegion` packs `image1`/`image2` from TMEM (>>5) and widthExp2 fields, and `GXLoadTexObjPreLoaded` injects the BP register ID in bits 24..31 for `mode0/mode1/image0/image1/image2/image3`.
- Default non-CI region selection uses round-robin `nextTexRgn++ & 7`.

## RVZ oracle: MEM1 dump at GXInit (MP4 USA)

- RVZ: `<MP4_RVZ_PATH>`
- PC checkpoint: `GXInit` entry @ `0x800C7E7C` (from `decomp_mario_party_4/config/GMPE01_00/symbols.txt`)
- Dump: `tests/oracles/mp4_rvz/mem1_at_pc_800C7E7C_gxinit.bin` (MEM1 0x80000000 size 0x01800000)
- SHA256: `1de788bdfe8d7ab0643af186d8000c606263e667e81a5e5fa16d50aea58bfffa`

How:
- `tools/dump_expected_rvz_mem1_at_pc.sh <rvz> 0x800C7E7C <out> 60`

Notes:
- This is a secondary oracle only. It proves our Dolphin+GDB capture pipeline works on a real RVZ at an early-init checkpoint.


## MP4 RVZ oracle: MEM1 dump at HuPadInit
- Game: Mario Party 4 (USA) RVZ (see docs/sdk/mp4/MP4_assets.md)
- Checkpoint: HuPadInit PC=0x80005A5C (from symbols.txt)
- Dump: MEM1 0x80000000 size 0x01800000
- Local path (gitignored): `tests/oracles/mp4_rvz/mem1_at_pc_80005A5C_hupadinit.bin`
- SHA256: `61f7c0d775bee3ef9f23364ad929a7d7d16c0c2ccbe7874f478030956bc6f9ae`
- Notes: This is a secondary oracle snapshot to compare against our sdk_port+tests as we extend coverage around PAD init.

## MP4 RVZ oracle: MEM1 dump at HuPrcInit
- Game: Mario Party 4 (USA) RVZ (see docs/sdk/mp4/MP4_assets.md)
- Checkpoint: HuPrcInit PC=0x8000C4A4 (from symbols.txt)
- Dump: MEM1 0x80000000 size 0x01800000
- Local path (gitignored): `tests/oracles/mp4_rvz/mem1_at_pc_8000C4A4_huprcinit.bin`
- SHA256: `e37e874fee24439a83351d7683913d419ceef27751205097a1e27f4262852ec8`

## MP4 host workload: HuSysInit reachability (host-only)
- Purpose: integration reachability only (not a correctness oracle). Used to discover missing SDK surface while running decompiled MP4 init code against `src/sdk_port`.
- Scenario: `tests/workload/mp4/mp4_husysinit_001_scenario.c`
- Build/run: `tools/run_host_scenario.sh tests/workload/mp4/mp4_husysinit_001_scenario.c`
- Output (marker only): `tests/actual/workload/mp4_husysinit_001.bin` begins with `MP40` + `DEADBEEF`.

## MP4 host workload: HuPadInit reachability (host-only)
- Purpose: integration reachability only. Runs the next MP4 init step (`HuPadInit`) after `HuSysInit` on host against `src/sdk_port`.
- Scenario: `tests/workload/mp4/mp4_hupadinit_001_scenario.c`
- Build/run: `tools/run_host_scenario.sh tests/workload/mp4/mp4_hupadinit_001_scenario.c`
- Output (marker only): `tests/actual/workload/mp4_hupadinit_001.bin` begins with `MP41` + `DEADBEEF`.

## MP4 host workload: HuPrcInit reachability (host-only)
- Purpose: integration reachability only. Compiles MP4 decomp `process.c` on host and runs `HuPrcInit` after `HuSysInit` (aligning with MP4 `main.c` init order).
- Scenario: `tests/workload/mp4/mp4_huprcinit_001_scenario.c`
- Build/run: `tools/run_host_scenario.sh tests/workload/mp4/mp4_huprcinit_001_scenario.c`
- Output (marker only): `tests/actual/workload/mp4_huprcinit_001.bin` begins with `MP42` + `DEADBEEF`.

## MP4 RVZ oracle: MEM1 dump at GWInit
- Game: Mario Party 4 (USA) RVZ (see docs/sdk/mp4/MP4_assets.md)
- Checkpoint: GWInit PC=0x800308B8 (from symbols.txt)
- Dump: MEM1 0x80000000 size 0x01800000
- Local path (gitignored): `tests/oracles/mp4_rvz/mem1_at_pc_800308B8_gwinit.bin`
- SHA256: `9130214e62b4c4abade670307d903bb9e8329145ca631e73057ea4e7eead0c0e`

## MP4 host workload: GWInit reachability (host-only)
- Purpose: integration reachability only. Runs MP4 init chain up to `GWInit` against `src/sdk_port`.
- Scenario: `tests/workload/mp4/mp4_gwinit_001_scenario.c`
- Build/run: `tools/run_host_scenario.sh tests/workload/mp4/mp4_gwinit_001_scenario.c`
- Output (marker only): `tests/actual/workload/mp4_gwinit_001.bin` begins with `MP43` + `DEADBEEF`.

## MP4 RVZ oracle: MEM1 dump at pfInit
- Game: Mario Party 4 (USA) RVZ (see docs/sdk/mp4/MP4_assets.md)
- Checkpoint: pfInit PC=0x8000AEF0 (from symbols.txt)
- Dump: MEM1 0x80000000 size 0x01800000
- Local path (gitignored): `tests/oracles/mp4_rvz/mem1_at_pc_8000AEF0_pfinit.bin`
- SHA256: `13f41479161d3d1e1036d722dd0ac9d24655cc3ef9f6b2568140a931c3e0f73f`

## MP4 host workload: pfInit reachability (host-only)
- Purpose: integration reachability only. Runs MP4 init chain up to `pfInit` against `src/sdk_port`.
- Scenario: `tests/workload/mp4/mp4_pfinit_001_scenario.c`
- Build/run: `tools/run_host_scenario.sh tests/workload/mp4/mp4_pfinit_001_scenario.c`
- Output (marker only): `tests/actual/workload/mp4_pfinit_001.bin` begins with `MP44` + `DEADBEEF`.

## MP4 RVZ oracle: MEM1 dump at HuSprInit
- Game: Mario Party 4 (USA) RVZ (see docs/sdk/mp4/MP4_assets.md)
- Checkpoint: HuSprInit PC=0x8000D348 (from symbols.txt)
- Dump: MEM1 0x80000000 size 0x01800000
- Local path (gitignored): `tests/oracles/mp4_rvz/mem1_at_pc_8000D348_husprinit.bin`
- SHA256: `c2c0d9b5ba41152b1e9e6d0e94b19037880d478cdeb380267e611fb30b994f73`

## MP4 host workload: HuSprInit reachability (host-only)
- Purpose: integration reachability only. Runs MP4 init chain up to `HuSprInit` against `src/sdk_port`.
- Scenario: `tests/workload/mp4/mp4_husprinit_001_scenario.c`
- Build/run: `tools/run_host_scenario.sh tests/workload/mp4/mp4_husprinit_001_scenario.c`
- Output (marker only): `tests/actual/workload/mp4_husprinit_001.bin` begins with `MP45` + `DEADBEEF`.

## MP4 host workload: init to VIWaitForRetrace (stubbed post-HuSprInit)
- Purpose: reachability only. Uses host-only stubs for heavy game modules after `HuSprInit` to reach the first hard SDK checkpoint `VIWaitForRetrace()` without importing full MP4 engine code.
- Scenario: `tests/workload/mp4/mp4_init_to_viwait_001_scenario.c`
- Stubs: `tests/workload/mp4/slices/post_sprinit_stubs.c` (WipeInit/omMasterInit)
- Build/run: `tools/run_host_scenario.sh tests/workload/mp4/mp4_init_to_viwait_001_scenario.c`
- Output (marker only): `tests/actual/workload/mp4_init_to_viwait_001.bin` begins with `MP46` + `DEADBEEF`.

## MP4 host workload: one main loop iteration (stubbed)
- Purpose: reachability only. Executes one iteration of the MP4 `while (1)` loop with non-SDK calls stubbed, to keep surfacing SDK gaps as we extend coverage.
- Scenario: `tests/workload/mp4/mp4_mainloop_one_iter_001_scenario.c`
- Uses real MP4 decomp functions for low-risk calls where available: `HuPadRead()` (from `pad.c`), `pfClsScr()` (from `pfinit_only.c`), `HuSysBeforeRender()` and `HuSysDoneRender()` (from `init.c`), `HuDvdErrorWatch()` (minimal slice `tests/workload/mp4/slices/hudvderrorwatch_only.c`), `HuSoftResetButtonCheck()` (minimal slice `tests/workload/mp4/slices/husoftresetbuttoncheck_only.c`), and a minimal `Hu3DPreProc()` slice (`tests/workload/mp4/slices/hu3d_preproc_only.c`).
- Note: `msmMusFdoutEnd()` is now provided by a minimal slice `tests/workload/mp4/slices/msmmusfdoutend_only.c` (empty in decomp).
- Build/run: `tools/run_host_scenario.sh tests/workload/mp4/mp4_mainloop_one_iter_001_scenario.c`
- Output (marker only): `tests/actual/workload/mp4_mainloop_one_iter_001.bin` begins with `MP47` + `DEADBEEF`.

## MP4 host workload: two main loop iterations (stubbed)
- Purpose: reachability only. Executes two iterations of the MP4 `while (1)` loop with non-SDK calls stubbed, to catch VI/GX state bugs that only show up after multiple frames.
- Scenario: `tests/workload/mp4/mp4_mainloop_two_iter_001_scenario.c`
- Build/run: `tools/run_host_scenario.sh tests/workload/mp4/mp4_mainloop_two_iter_001_scenario.c`
- Output (marker only): `tests/actual/workload/mp4_mainloop_two_iter_001.bin` begins with `MP49` + `DEADBEEF`.

## MP4 host workload: one main loop iteration with VI tick (PadReadVSync driven)
- Purpose: reachability + pad callback proof. Simulates one VI retrace tick so the PostRetraceCallback installed by `HuPadInit` (PadReadVSync) runs.
- Scenario: `tests/workload/mp4/mp4_mainloop_one_iter_tick_001_scenario.c`
- Build/run: `tools/run_host_scenario.sh tests/workload/mp4/mp4_mainloop_one_iter_tick_001_scenario.c`
- Output: `tests/actual/workload/mp4_mainloop_one_iter_tick_001.bin`
  - 0x00: `MP4P` marker
  - 0x08: HuPadErr[0] (expected: 0xFE == PAD_ERR_NOT_READY in our deterministic PADRead stub)

## MP4 host workload: two main loop iterations with VI tick (PadReadVSync driven)
- Purpose: reachability + pad callback proof over multiple frames. Simulates one VI retrace tick per iteration so PadReadVSync runs each frame.
- Scenario: `tests/workload/mp4/mp4_mainloop_two_iter_tick_001_scenario.c`
- Build/run: `tools/run_host_scenario.sh tests/workload/mp4/mp4_mainloop_two_iter_tick_001_scenario.c`
- Output: `tests/actual/workload/mp4_mainloop_two_iter_tick_001.bin`
  - 0x00: `MP4Q` marker
  - 0x08: HuPadErr[0] (expected: 0xFE)
  - 0x0C: GC_SDK_OFF_PAD_READ_CALLS (expected: 8 == 4 pads * 2 ticks)

## MP4 RVZ oracle: MEM1 dump at HuPerfInit
- Game: Mario Party 4 (USA) RVZ (see docs/sdk/mp4/MP4_assets.md)
- Checkpoint: HuPerfInit PC=0x8002E74C (from symbols.txt)
- Dump: MEM1 0x80000000 size 0x01800000
- Local path (gitignored): `tests/oracles/mp4_rvz/mem1_at_pc_8002E74C_huperfinit.bin`
- SHA256: `8cc09cd47ba1eed19c925d747fae9e090a3475d16ec3c4e1639f80f979ea8c21`

## MP4 host workload: HuPerfInit reachability (host-only)
- Purpose: integration reachability only. Adds a minimal decomp slice for `HuPerfInit`/`HuPerfCreate` (perf.c) and runs MP4 init chain up to that point.
- Slice: `tests/workload/mp4/slices/huperfinit_only.c`
- Scenario: `tests/workload/mp4/mp4_huperfinit_001_scenario.c`
- Build/run: `tools/run_host_scenario.sh tests/workload/mp4/mp4_huperfinit_001_scenario.c`
- Output (marker only): `tests/actual/workload/mp4_huperfinit_001.bin` begins with `MP48` + `DEADBEEF`.

## MP4 RVZ oracle: MEM1 dump at HuSysBeforeRender
- Game: Mario Party 4 (USA) RVZ (see docs/sdk/mp4/MP4_assets.md)
- Checkpoint: HuSysBeforeRender PC=0x80009FF8 (from symbols.txt)
- Dump: MEM1 0x80000000 size 0x01800000
- Local path (gitignored): `tests/oracles/mp4_rvz/mem1_at_pc_80009FF8_husysbeforerender.bin`
- SHA256: `d2d51196f4bfd4d740597cfe9ec89477a74871d6b3865511d65b7d94b5fb986f`

## MP4 RVZ oracle: MEM1 dump at HuSysDoneRender
- Game: Mario Party 4 (USA) RVZ (see docs/sdk/mp4/MP4_assets.md)
- Checkpoint: HuSysDoneRender PC=0x8000A0DC (from symbols.txt)
- Dump: MEM1 0x80000000 size 0x01800000
- Local path (gitignored): `tests/oracles/mp4_rvz/mem1_at_pc_8000A0DC_husysdonerender.bin`
- SHA256: `1affc12f7a01dadef66fe81c6c3e5ec14536f07acd32c0a2d8739bfb4451d3da`

## MP4 RVZ oracle: MEM1 dump at Hu3DPreProc
- Game: Mario Party 4 (USA) RVZ (see docs/sdk/mp4/MP4_assets.md)
- Checkpoint: Hu3DPreProc PC=0x8001FBCC (from symbols.txt)
- Dump: MEM1 0x80000000 size 0x01800000
- Local path (gitignored): `tests/oracles/mp4_rvz/mem1_at_pc_8001FBCC_hu3dpreproc.bin`
- SHA256: `7dc513a8280395f8c037ff9ed36740d5b234bc4975a6b3a512e26727b9e55959`

## SDK MTX: C_MTXIdentity
- Evidence: `tests/sdk/mtx/mtx_identity` (DOL expected vs host actual) PASS:
  - `tests/sdk/mtx/mtx_identity/expected/mtx_identity_min_001.bin`
  - `tests/sdk/mtx/mtx_identity/actual/mtx_identity_min_001.bin`
- Confirmed behavior: writes a 3x4 identity matrix (diag = 1.0f, all other entries = 0.0f).

## SDK MTX: C_MTXOrtho
- Evidence: `tests/sdk/mtx/mtx_ortho` (DOL expected vs host actual) PASS:
  - `tests/sdk/mtx/mtx_ortho/expected/mtx_ortho_ortho_0_1_001.bin`
  - `tests/sdk/mtx/mtx_ortho/actual/mtx_ortho_ortho_0_1_001.bin`
- Confirmed behavior: matches `decomp_mario_party_4/src/dolphin/mtx/mtx44.c:C_MTXOrtho`.
- Tested parameters: `t=0, b=1, l=0, r=1, n=0, f=1` (chosen to avoid float rounding drift).

## MP4 host workload: WipeExecAlways minimal decomp slice (blank-mode only)
- Purpose: integration reachability only (not a GX parity oracle).
- Change: mp4_mainloop workloads now link `tests/workload/mp4/slices/wipeexecalways_decomp_blank.c` instead of an inline stub.
- Behavior: executes the real `WipeExecAlways` mode switch for `WIPE_MODE_BLANK`; all GX drawing helpers are NO-OP on host.
- WipeInit:
  - `mp4_init_to_viwait_001_scenario` uses stub `tests/workload/mp4/slices/wipeinit_stub.c`
  - `mp4_mainloop_*` scenarios call `WipeInit(&GXNtsc480IntDf)` from the decomp slice

## MP4 host workload: HuPrcCall remains stubbed
- Attempt: link real `src/game_workload/mp4/vendor/src/game/process.c:HuPrcCall`.
- Result: blocked on missing coroutine support (`gcsetjmp`/`gclongjmp` with PPC register layout in `tests/workload/include/game/jmp.h`) and heap cleanup dependency (`HuMemDirectFree`).
- Decision: keep HuPrcCall as a scenario-level stub until we design a host-safe process scheduler strategy.

## SDK VI: VIWaitForRetrace invokes PostRetraceCallback (modeled)
- Decomp evidence: `decomp_mario_party_4/src/dolphin/vi.c:__VIRetraceHandler` calls `PostCB(retraceCount)` on each retrace.
- Model behavior: `src/sdk_port/vi/VI.c:VIWaitForRetrace` increments `GC_SDK_OFF_VI_RETRACE_COUNT` and invokes the stored PostRetraceCallback once per call.
- Deterministic test PASS (DOL expected vs host actual):
  - `tests/sdk/vi/vi_wait_for_retrace/expected/vi_wait_for_retrace_calls_post_cb_hupadinit_001.bin`
  - `tests/sdk/vi/vi_wait_for_retrace/actual/vi_wait_for_retrace_calls_post_cb_hupadinit_001.bin`
## 2026-02-09: GXInitTexObjLOD (MP4 pfDrawFonts)
- MP4 callsite: `decomp_mario_party_4/src/game/printfunc.c` calls `GXInitTexObjLOD(&font_tex, GX_NEAR, GX_NEAR, 0,0,0, GX_FALSE, GX_FALSE, GX_ANISO_1)`.
- Oracle test: `tests/sdk/gx/gx_init_tex_obj_lod`.
  - expected: Dolphin running DOL `tests/sdk/gx/gx_init_tex_obj_lod/dol/mp4/realistic_pfdrawfonts_001` and dumping 0x80300000.
  - actual: host scenario `tests/sdk/gx/gx_init_tex_obj_lod/host/gx_init_tex_obj_lod_mp4_pfdrawfonts_001_scenario.c` using `src/sdk_port/gx/GX.c`.
  - status: PASS (bit-exact expected.bin == actual.bin).

## 2026-02-09: OSStopwatch Start/Stop/Check (MP4 HuPerf)
- MP4 callsites: `decomp_mario_party_4/src/game/perf.c` uses OSStopwatch for perf bookkeeping.
  - `HuPerfZero`: `OSStopStopwatch; OSResetStopwatch; OSStartStopwatch`
  - `HuPerfBegin/End`: `OSStartStopwatch; OSCheckStopwatch; OSStopStopwatch; OSResetStopwatch`
- Oracle test: `tests/sdk/os/os_stopwatch`
  - expected: Dolphin DOL `tests/sdk/os/os_stopwatch/dol/mp4/realistic_huperf_001`
  - actual: host scenario `tests/sdk/os/os_stopwatch/host/os_stopwatch_mp4_huperf_001_scenario.c` using `src/sdk_port/os/OSStopwatch.c`
  - status: PASS (bit-exact expected.bin == actual.bin)

## 2026-02-09: GX immediate-mode helpers (MP4 pfDrawFonts)
- Context: MP4 `pfDrawFonts` emits immediate-mode vertex/color/texcoord streams. On real hardware these go to the GP FIFO and are not directly observable in MEM1 without a full FIFO model.
- Decision: model these helpers deterministically in `src/sdk_port/gx/GX.c` as "last written" mirrors, and assert them via DOL expected vs host actual dumps at 0x80300000.
- Tests (DOL expected vs host actual) PASS:
  - `tests/sdk/gx/gx_position_3s16` (stores last x/y/z as sign-extended u32)
  - `tests/sdk/gx/gx_color_1x8` (stores last 8-bit value)
  - `tests/sdk/gx/gx_tex_coord_2f32` (stores last s/t as f32 bit patterns)
  - `tests/sdk/gx/gx_position_2f32` (stores last x/y as f32 bit patterns)
  - `tests/sdk/gx/gx_color_3u8` (stores last RGB packed as 0x00RRGGBB)

## 2026-02-09: DVDInit default drive status (MP4 HuDvdErrorWatch)
- MP4 callsite: `decomp_mario_party_4/src/game/dvd.c:HuDvdErrorWatch` polls `DVDGetDriveStatus()` during normal boot.
- Deterministic contract we model for boot: `DVDInit(); DVDGetDriveStatus();` returns `0` (idle).
- Test PASS (DOL expected vs host actual):
  - `tests/sdk/dvd/dvd_get_drive_status/expected/dvd_get_drive_status_realistic_hudvderrorwatch_001.bin`
  - `tests/sdk/dvd/dvd_get_drive_status/actual/dvd_get_drive_status_realistic_hudvderrorwatch_001.bin`

## 2026-02-09: GXTransform FIFO packing (MP4 hsfdraw)

### GXLoadNrmMtxImm
- Decomp reference: `decomp_mario_party_4/src/dolphin/gx/GXTransform.c:GXLoadNrmMtxImm`.
- Confirmed FIFO packing:
  - `addr = id * 3 + 0x400`
  - `reg = addr | 0x80000`
  - payload = top-left 3x3 of the 3x4 input (9 floats).
- Test PASS (DOL expected vs host actual):
  - `tests/sdk/gx/gx_load_nrm_mtx_imm/expected/gx_load_nrm_mtx_imm_mp4_hsfdraw_001.bin`
  - `tests/sdk/gx/gx_load_nrm_mtx_imm/actual/gx_load_nrm_mtx_imm_mp4_hsfdraw_001.bin`

### GXLoadTexMtxImm
- Decomp reference: `decomp_mario_party_4/src/dolphin/gx/GXTransform.c:GXLoadTexMtxImm`.
- Confirmed FIFO packing:
  - if `id >= GX_PTTEXMTX0 (64)`: `addr = (id - 64) * 4 + 0x500`
  - else: `addr = id * 4`
  - `count = (type == GX_MTX2x4) ? 8 : 12`
  - `reg = addr | ((count - 1) << 16)`
  - payload = 2x4 (8 floats) or 3x4 (12 floats), row-major.
- Tests PASS (DOL expected vs host actual):
  - `tests/sdk/gx/gx_load_tex_mtx_imm/expected/gx_load_tex_mtx_imm_mp4_hsfdraw_2x4_001.bin`
  - `tests/sdk/gx/gx_load_tex_mtx_imm/actual/gx_load_tex_mtx_imm_mp4_hsfdraw_2x4_001.bin`
  - `tests/sdk/gx/gx_load_tex_mtx_imm/expected/gx_load_tex_mtx_imm_mp4_hsfdraw_3x4_001.bin`
  - `tests/sdk/gx/gx_load_tex_mtx_imm/actual/gx_load_tex_mtx_imm_mp4_hsfdraw_3x4_001.bin`

## 2026-02-09: MP4 RVZ MEM1 oracle dump at GXLoadTexMtxImm
- RVZ specimen: `<MP4_RVZ_PATH>`
- Checkpoint PC: `0x800CF628` (GXLoadTexMtxImm) from `output_mp4/ppc_func_mapping.cpp`.
- Produced via: `tools/dump_expected_rvz_mem1_at_pc.sh <rvz> 0x800CF628 <out>`
- Output (local-only, gitignored): `tests/oracles/rvz/mp4_mem1_at_gx_load_tex_mtx_imm_0x800CF628.bin`

#### RVZ vs host semantic check (GXLoadTexMtxImm checkpoint)
- Fact: Even at a deep main-loop checkpoint (PC `0x800CF628`), the same semantic invariants still match between RVZ and host workloads.
  Evidence (RVZ): `tests/oracles/mp4_rvz/probes/gxloadtexmtximm_pc_800CF628_v2/values.txt`
  Evidence (host): `tests/oracles/mp4_rvz/probes/host_gxloadtexmtximm_mp4_mainloop_one_iter_tick_001_v2/values.txt` (from `tests/workload/mp4/mp4_mainloop_one_iter_tick_001_scenario.c`)
  Evidence (compare): `python3 tools/helpers/compare_rvz_host_probe_values.py <rvz_values> <host_values> --ignore __PADSpec`
- SHA256: `054827a091f57b2f8941886078625b8d00cf1adf91aaeb64770b110f34be9c93`

## 2026-02-09: Host MEM1 dump determinism (MP4 workload)
- Scenario: `tests/workload/mp4/mp4_mainloop_one_iter_tick_001_scenario.c`
- Host MEM1 dump (local-only, gitignored): `tests/oracles/host/mp4_mainloop_one_iter_tick_001_mem1.bin`
- SHA256 (deterministic after fix below): `fbe781c70eb15e8f7db1d01dfaf891223000fd6e0d590898507884c01cb89fbe`

### Determinism fix: VI post-retrace callback tokenization
- Problem: `VISetPostRetraceCallback` stored raw host function pointers into RAM-backed `sdk_state` at `GC_SDK_OFF_VI_POST_CB_PTR` (0x817FE200).
  - Host pointers are ASLR-dependent, so MEM1 dumps changed run-to-run.
- Fix: in `src/sdk_port/vi/VI.c`, store a stable small token for real host pointers while still retaining the real function pointer for invocation.
  - If the callback already looks like a PPC/MEM1 token (0x80000000..0x81800000), store it as-is (this is what unit tests use).
  - Otherwise store `1..N` in first-seen order (sufficient for MP4 workloads).

### RVZ vs host MEM1 diff: first mismatch is harness marker (not meaningful yet)
- Comparing `tests/oracles/rvz/mp4_mem1_at_gx_load_tex_mtx_imm_0x800CF628.bin` vs `tests/oracles/host/mp4_mainloop_one_iter_tick_001_mem1.bin`:
  - First mismatch at dump offset `0x00300000` (RAM address `0x80300000`).
  - Cause: host workloads write harness markers (e.g. "MP4P" + 0xDEADBEEF) at `0x80300000`, while the retail RVZ run does not.
- Conclusion: a raw MEM1-vs-MEM1 binary diff between RVZ and host is not an oracle yet.
  - Use RVZ dumps as *evidence* (runtime snapshots) and compare only well-defined regions that have matching semantics in both worlds (to be defined).

## 2026-02-10: Smoke chain PASS (mp4_perf_chain_001)
- Goal: Exercise MP4 perf primitives (OSStopwatch + GX draw sync) and prove PPC-vs-host determinism for a small MEM1 window.
- Result: PASS (bit-exact for smoke compare window).
  Evidence (expected): `tests/sdk/smoke/mp4_perf_chain_001/expected/mp4_perf_chain_001_mem1.bin`
  Evidence (actual): `tests/sdk/smoke/mp4_perf_chain_001/actual/mp4_perf_chain_001_mem1.bin`
  Evidence (diff): `tools/diff_bins_smoke.sh <expected> <actual>` (includes marker/snapshot + `sdk_state` page)

## 2026-02-10: Retail RVZ trace replays (MP4 PADClamp + PADReset)

### PADClamp
- Retail entry PC: `0x800C3E0C` (from `external/mp4-decomp/config/GMPE01_00/symbols.txt`)
- Trace harvesting: `tools/trace_pc_entry_exit.py` (entry PC + LR breakpoint) dumping `status:@r3:0x30`
- Host replay harness (committed):
  - Scenario: `tests/sdk/pad/pad_clamp/host/pad_clamp_rvz_trace_replay_001_scenario.c`
  - Replay: `tools/replay_trace_case_pad_clamp.sh <trace_case_dir>`
- Result: 6 unique cases replay PASS (bit-exact vs retail `out_status.bin`).

### PADReset
- Retail entry PC: `0x800C4774` (from `external/mp4-decomp/config/GMPE01_00/symbols.txt`)
- Observed retail behavior for MP4 boot case (mask `0x70000000`):
  - return `r3 = 1`
  - `ResettingBits: 0x00000000 -> 0x30000000`
  - `ResettingChan: 32 -> 1`
- Trace harvesting output is local-only (gitignored) under `tests/trace-harvest/pad_reset/mp4_rvz_v2/`.
- Host replay harness (committed):
  - Scenario: `tests/sdk/pad/pad_reset/host/pad_reset_rvz_trace_replay_001_scenario.c`
  - Replay: `tools/replay_trace_case_pad_reset.sh <trace_case_dir>`
- Implementation note: `src/sdk_port/pad/PAD.c` models the immediate `DoReset()` step (OR bits, `cntlzw`, clear one bit).
  - Seed + observable state stored in `sdk_state` offsets:
    - `GC_SDK_OFF_PAD_RESETTING_BITS`
    - `GC_SDK_OFF_PAD_RESETTING_CHAN`
    - `GC_SDK_OFF_PAD_RECALIBRATE_BITS`
    - `GC_SDK_OFF_PAD_RESET_CB_PTR`

## 2026-02-10: Retail RVZ trace replay (MP4 PADInit)

### PADInit
- Retail entry PC: `0x800C4978` (from `external/mp4-decomp/config/GMPE01_00/symbols.txt`)
- Trace dir (local-only, gitignored): `tests/trace-harvest/pad_init/mp4_rvz_v1/` (2 unique cases)
- Replay harness (committed):
  - Scenario: `tests/sdk/pad/pad_init/host/pad_init_rvz_trace_replay_001_scenario.c`
  - Replay: `tools/replay_trace_case_pad_init.sh <trace_case_dir>`
- Result: replay PASS for both captured cases (bit-exact vs retail `out_sdk_state.bin` + return value).

## 2026-02-10: Retail RVZ trace replay (MP4 PADSetSpec)

### PADSetSpec
- Retail entry PC: `0x800C4FD8` (from `external/mp4-decomp/config/GMPE01_00/symbols.txt`)
- Trace dir (local-only, gitignored): `tests/trace-harvest/pad_set_spec/mp4_rvz_v3/` (1 unique case)
- Replay harness (committed):
  - Scenario: `tests/sdk/pad/pad_set_spec/host/pad_set_spec_rvz_trace_replay_001_scenario.c`
  - Replay: `tools/replay_trace_case_pad_set_spec.sh <trace_case_dir>`
- Confirmed retail side effects (from trace dumps + decomp `external/mp4-decomp/src/dolphin/pad/Pad.c`):
  - `__PADSpec` @ `0x801D450C` is cleared to 0.
  - `Spec` @ `0x801D3924` is set to the input `spec`.
  - `MakeStatus` @ `0x801D3928` points to `SPEC2_MakeStatus` (`0x800C5320`) for `PAD_SPEC_5` (MP4 boot case).
- Result: replay PASS for the captured case (bit-exact vs retail global dumps).


## 2026-02-10: Retail RVZ trace replay (MP4 OSDisableInterrupts)

### OSDisableInterrupts
- Retail entry PC: `0x800B723C` (from `external/mp4-decomp/config/GMPE01_00/symbols.txt`)
- Trace dir (local-only, gitignored): `tests/trace-harvest/os_disable_interrupts/mp4_rvz_v1/` (10 unique cases)
- Replay harness (committed):
  - Scenario: `tests/sdk/os/os_disable_interrupts/host/os_disable_interrupts_rvz_trace_replay_001_scenario.c`
  - Replay: `tools/replay_trace_case_os_disable_interrupts.sh <trace_case_dir>`
  - 1-button: `tools/harvest_and_replay_os_disable_interrupts.sh`
- Observed in these MP4 boot traces: return value = 0 (interrupts already disabled at entry in all collected cases).
- Result: replay PASS for all 10 unique cases (bit-exact).


## 2026-02-10: Retail RVZ trace replay (MP4 VISetPostRetraceCallback)

### VISetPostRetraceCallback
- Retail entry PC: `0x800C0DD8` (from `external/mp4-decomp/config/GMPE01_00/symbols.txt`)
- Retail global: `PostCB` @ `0x801D443C` (from `external/mp4-decomp/config/GMPE01_00/symbols.txt`)
- Trace dir (local-only, gitignored): `tests/trace-harvest/vi_set_post_retrace_callback/mp4_rvz_v2/` (1 unique case)
- Replay harness (committed):
  - Scenario: `tests/sdk/vi/vi_set_post_retrace_callback/host/vi_set_post_retrace_callback_rvz_trace_replay_001_scenario.c`
  - Replay: `tools/replay_trace_case_vi_set_post_retrace_callback.sh <trace_case_dir>`
  - 1-button: `tools/harvest_and_replay_vi_set_post_retrace_callback.sh`
- Observed MP4 boot case: old callback `0x00000000`, new callback `0x80005CB4` (PadReadVSync).
- Result: replay PASS for the captured case (bit-exact).


## 2026-02-10: Retail RVZ trace replay (MP4 SISetSamplingRate)

### SISetSamplingRate
- Retail entry PC: `0x800DA3C8` (from `external/mp4-decomp/config/GMPE01_00/symbols.txt`)
- Trace dir (local-only, gitignored): `tests/trace-harvest/si_set_sampling_rate/mp4_rvz_si_ctrl/` (10 unique cases)
- Replay harness (committed):
  - Scenario: `tests/sdk/si/si_set_sampling_rate/host/si_set_sampling_rate_rvz_trace_replay_001_scenario.c`
  - Replay: `tools/replay_trace_case_si_set_sampling_rate.sh <trace_case_dir>`
  - 1-button: `tools/harvest_and_replay_si_set_sampling_rate.sh` (replay-only; uses existing trace corpus)
- Result: replay PASS for all 10 unique cases (bit-exact).


## 2026-02-10: Retail RVZ trace replay (MP4 PADControlMotor)

### PADControlMotor
- Retail entry PC: `0x800C4F34` (from `external/mp4-decomp/config/GMPE01_00/symbols.txt`)
- Trace dir (local-only, gitignored): `tests/trace-harvest/pad_control_motor/mp4_rvz_v1/` (2 unique cases)
- Replay harness (committed):
  - Scenario: `tests/sdk/pad/pad_control_motor/host/pad_control_motor_rvz_trace_replay_001_scenario.c`
  - Replay: `tools/replay_trace_case_pad_control_motor.sh <trace_case_dir>`
  - 1-button: `tools/harvest_and_replay_pad_control_motor.sh`
- Observed cases: chan=0 cmd=2
- Result: replay PASS for both captured cases (bit-exact).





## 2026-02-10: Retail RVZ trace replay (MP4 SITransfer)

### SITransfer
- Retail entry PC: `0x800D9CC4` (from `external/mp4-decomp/config/GMPE01_00/symbols.txt`)
- Trace dir (local-only, gitignored): `tests/trace-harvest/si_transfer/mp4_rvz_v4/` (20 unique cases)
- Replay harness (committed):
  - Scenario: `tests/sdk/si/si_transfer/host/si_transfer_rvz_trace_replay_001_scenario.c`
  - Replay: `tools/replay_trace_case_si_transfer.sh <trace_case_dir>`
- Result: replay PASS for all 20 unique cases (bit-exact).
- Confirmed behaviors (as observed in traces + decomp `external/mp4-decomp/src/dolphin/si/SIBios.c`):
  - `SITransfer()` returns `TRUE` in all observed MP4 init cases.
  - When an immediate transfer succeeds, `Packet[chan]` and `Alarm[chan]` remain unchanged.
  - When an immediate transfer fails, it queues `Packet[chan]` fields and sets `packet->fire = XferTime[chan] + delay`.
  - When `now < fire`, it also arms `Alarm[chan]` (handler pointer + fire time).

## 2026-02-10: Retail RVZ trace replay (MP4 SIGetResponse)

### SIGetResponse
- Retail entry PC: `0x800D9B74` (from `external/mp4-decomp/config/GMPE01_00/symbols.txt`)
- Decomp reference: `external/mp4-decomp/src/dolphin/si/SIBios.c` (`SIGetResponseRaw` + `SIGetResponse`)
- Trace dir (local-only, gitignored): `tests/trace-harvest/si_get_response/mp4_rvz_v1/`
- Replay harness (committed):
  - Scenario: `tests/sdk/si/si_get_response/host/si_get_response_rvz_trace_replay_001_scenario.c`
  - Replay: `tools/replay_trace_case_si_get_response.sh <trace_case_dir>`
- Result: 10 replay cases PASS (bit-exact).

Observed trace format note:
- `tools/trace_pc_entry_exit.py` writes `out_regs.json` with the post-call register file under `"args"` (not `"rets"`).
  - For this function, the return value is `out_regs.json: args.r3`.

Confirmed behaviors (as observed in traces + decomp):
- `SIGetResponse()` always calls `SIGetResponseRaw(chan)` under interrupts-disabled critical section.
- When the return is `TRUE`, it copies two `u32` words into `data` and clears `InputBufferValid[chan]` back to `FALSE`.
- In the observed MP4 init case `hit_000001_*`, the caller's `data` buffer changes from `00000000 801A6F98` to `00808080 80800000`.

Implementation note (sdk_port model):
- Host trace replay cannot read SI MMIO (`__SIRegs`). For deterministic replay we seed:
  - per-channel status (`SI_ERROR_RDST` bit)
  - per-channel response words
- Seed entrypoints: `gc_si_set_status_seed()` + `gc_si_set_resp_words_seed()` in `src/sdk_port/si/SI.c`.

## 2026-02-10: MP4 decomp scan: unique Nintendo SDK functions used by MP4 (game sources)

Goal: estimate how much of the Nintendo SDK surface MP4 needs by scanning the MP4 decomp *game* sources
for SDK API usage, excluding SDK sources themselves.

Method (reproducible):
1) Build a set of SDK API names from the MP4 decomp Dolphin headers:
   - Source: `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/include/dolphin/**/*.h`
   - Extract prototypes matching SDK prefixes:
     `OS,GX,VI,PAD,DVD,SI,EXI,AI,AX,AR,ARQ,CARD,MTX,PS,DB,DSP,THP,DEMO`
2) Scan MP4 decomp `src/**/*.c` excluding `src/dolphin/**` for callsites to names in that API set.

Result (current snapshot):
- SDK API names extracted from headers: 694
- Unique SDK functions referenced by MP4 game sources (excluding `src/dolphin`): 273
- Per-subsystem unique usage (from the scan):
  - GX: 121
  - OS: 48
  - PS: 28
  - CARD: 17
  - VI: 12
  - DVD: 10
  - AR: 10
  - AI: 8
  - PAD: 7
  - THP: 6
  - ARQ: 2
  - DEMO: 2
  - DB: 1
  - SI: 1

Notes:
- This is distinct from `docs/sdk/mp4/MP4_sdk_calls_inventory.csv` which inventories SDK-like callsites
  in `src/game_workload/mp4/vendor/src/game/*.c` only.
## 2026-02-11: PBT chain expansion (MTX + DVD core + ARQ + CARD-FAT)

- Added suite selector to `tools/run_pbt.sh`: `os_round_32b`, `mtx_core`, `dvd_core`.
- Added `tests/pbt/mtx/mtx_core_pbt.c`:
  - validates `C_MTXIdentity` / `PSMTXIdentity` invariants
  - validates `C_MTXOrtho` formula invariants over randomized ranges
  - PASS at 50,000 iterations (`tools/run_pbt.sh mtx_core 50000 0xC0DEC0DE`)
- Added `tests/pbt/dvd/dvd_core_pbt.c`:
  - randomized checks for `DVDFastOpen`, `DVDOpen`, `DVDConvertPathToEntrynum`
  - randomized checks for `DVDRead`, `DVDReadAsync`, `DVDReadPrio`, `DVDReadAsyncPrio`, `DVDClose`
  - callback and command-block state checks
  - PASS at 20,000 iterations (`tools/run_pbt.sh dvd_core 20000 0xC0DEC0DE`)
- Imported ARQ + CARD-FAT property suites from integration branch and validated on macOS:
  - `tools/run_arq_property_test.sh --num-runs=50 -v` PASS
  - `tools/run_card_fat_property_test.sh --num-runs=50 -v` PASS
- Updated chain matrix baseline with current ARQ/CARD-FAT status:
  - `docs/sdk/PBT_CHAIN_MATRIX.md`

## 2026-02-11: One-button HuPadInit blocker harvest/replay loop
- Added `tools/harvest_and_replay_hupadinit_blockers.sh` to run all four MP4 HuPadInit blockers in one command:
  - `OSDisableInterrupts` / `OSRestoreInterrupts`
  - `VISetPostRetraceCallback`
  - `SISetSamplingRate`
  - `PADControlMotor`
- Verified end-to-end with short caps (`GC_MAX_UNIQUE=1 GC_MAX_HITS=20 GC_TIMEOUT=60`) on this machine.
- Replay outcomes from the run:
  - `OSDisableInterrupts`: replay PASS on harvested hits.
  - `VISetPostRetraceCallback`: replay PASS (`old=0x0 new=0x80005CB4` case).
  - `SISetSamplingRate`: replay PASS for sampled unique case.
  - `PADControlMotor`: replay PASS for harvested hits (`chan=0 cmd=2` examples).
- This loop is the current fastest way to expand retail-trace fixtures for HuPadInit blockers while keeping deterministic host replay validation in the same run.

## 2026-02-11: Dual-run oracle parity check (phase 1)
- Added `tools/run_oracle_dualcheck.sh`.
- Implemented strict-vs-adapted dual check for CARD leaf function `__CARDCheckSum`:
  - strict oracle body: `tests/sdk/card/property/card_checksum_strict_oracle.h` (copied from MP4 decomp)
  - adapted oracle body: `tests/sdk/card/property/card_fat_oracle.h`
  - candidate: `src/sdk_port/card/card_fat.c` via existing property harness
- Validation run:
  - `tools/run_oracle_dualcheck.sh 200`
  - strict checksum dual-check: `200/200 PASS`
  - ARQ adapted suite: `150/150 PASS`
  - CARD FAT adapted suite: `1569/1569 PASS`
- Scope note: this is phase 1 (leaf strict check) and does not yet cover strict dual-run for ARQ/OSAlloc/DVDFS/MTX.

## 2026-02-11: MP4 chain coverage sync (GXInitTexObjLOD)
- Verified suite parity for MP4 callsite test:
  - `tests/sdk/gx/gx_init_tex_obj_lod/expected/gx_init_tex_obj_lod_mp4_pfdrawfonts_001.bin`
  - `tests/sdk/gx/gx_init_tex_obj_lod/actual/gx_init_tex_obj_lod_mp4_pfdrawfonts_001.bin`
  - `tools/ram_compare.py` result: PASS (bit-identical).
- Updated `docs/sdk/mp4/MP4_chain_all.csv` row `pfDrawFonts -> GXInitTexObjLOD` from `0/1` to `1/1` and marked covered (`y`).

## 2026-02-11: HuPadInit blocker retail replay refresh
- Revalidated blocker replays with current trace corpus:
  - `OSDisableInterrupts/OSRestoreInterrupts`: harvested (unique=3 target) and replay PASS over existing hit set under `tests/trace-harvest/os_disable_interrupts/mp4_rvz_v1/`.
  - `VISetPostRetraceCallback`: replay PASS on `tests/trace-harvest/vi_set_post_retrace_callback/mp4_rvz_v2/hit_000001_pc_800C0DD8_lr_80005A98`.
  - `SISetSamplingRate`: replay PASS for 3 unique cases from `tests/trace-harvest/si_set_sampling_rate/mp4_rvz_si_ctrl/`.
  - `PADControlMotor`: replay PASS on `tests/trace-harvest/pad_control_motor/mp4_rvz_v1/hit_000002_pc_800C4F34_lr_80005B00`.
- Tooling note:
  - `harvest_and_replay_vi_set_post_retrace_callback.sh` and `harvest_and_replay_pad_control_motor.sh` can hang after collecting hits (no terminal completion message).
  - Workaround used in this session: stop harvest process after hit files appear, then run replay script directly for collected hit dirs.

## 2026-02-11: Harvest teardown hang fixed (trace_pc_entry_exit)
- Root cause: `tools/trace_pc_entry_exit.py` could block during Dolphin teardown due a second `wait()` without timeout after kill path.
- Fix:
  - Added bounded teardown in `tools/trace_pc_entry_exit.py`:
    - `wait(timeout=5)` after terminate
    - fallback `kill()` + `wait(timeout=5)`
    - final non-blocking SIGKILL fallback (best-effort), no indefinite wait.
- Verification:
  - `GC_MAX_UNIQUE=1 GC_MAX_HITS=10 GC_TIMEOUT=25 tools/harvest_and_replay_vi_set_post_retrace_callback.sh <rvz>`: completes and replays PASS.
  - `GC_MAX_UNIQUE=1 GC_MAX_HITS=10 GC_TIMEOUT=25 tools/harvest_and_replay_pad_control_motor.sh <rvz>`: completes and replays PASS.

## 2026-02-11: PADRead one-button retail harvest/replay
- Added script: `tools/harvest_and_replay_pad_read.sh`
  - Entry PC: `0x800C4B88` (`PADRead`)
  - Dump window: `status:@r3:0x30`
  - Output trace dir: `tests/trace-harvest/pad_read/mp4_rvz_v3/`
- Validation run:
  - `GC_MAX_UNIQUE=1 GC_MAX_HITS=10 GC_TIMEOUT=25 tools/harvest_and_replay_pad_read.sh <rvz>`
  - Harvest completed with 1 unique case.
  - Replay PASS: `hit_000001_pc_800C4B88_lr_8005A7F0` (`ret=0x80000000`).

## 2026-02-11: Post-HuDataInit blocker trace (decomp callflow)
- Traced `HuDataDirRead -> HuDataReadNumHeapShortForce -> HuDvdDataReadWait` in MP4 decomp:
  - `DVDConvertPathToEntrynum`
  - `DVDFastOpen`
  - `DVDReadAsync`
  - `DVDGetCommandBlockStatus`
  - `DCInvalidateRange`
  - `DVDClose`
  - `OSRoundUp32B` / `OSRoundDown32B`
- Conclusion:
  - Immediate post-`HuDataInit` SDK blockers in this path are already covered in current MP4 chain suites.
  - Next higher-risk blockers move to frame-loop and streaming paths (e.g. THP path using `DVDReadPrio` / `DVDReadAsyncPrio`).

## 2026-02-11: OSLink retail trace harvest (omMaster blocker path)
- Captured first real-game `OSLink` entry/exit case from MP4 RVZ:
  - Entry PC: `0x800B7D24`
  - Output dir: `tests/trace-harvest/os_link/mp4_rvz_v1/`
  - Hit dir: `tests/trace-harvest/os_link/mp4_rvz_v1/hit_000001_pc_800B7D24_lr_8003222C/`
- Captured windows:
  - `module_info` (`@r3`, 0x100 bytes)
  - `bss` (`@r4`, 0x80 bytes)
  - `os_module_list` (`0x800030C8`, 0x20 bytes)
  - `os_string_table` (`0x800030D0`, 0x10 bytes)
- Added one-button harvest scripts:
  - `tools/harvest_and_replay_os_link.sh`
  - `tools/harvest_and_replay_os_unlink.sh`
- Added first replay harness:
  - `tools/replay_trace_case_os_link.sh`
  - scenario: `tests/sdk/os/os_link/host/os_link_rvz_trace_replay_001_scenario.c`
- Validation:
  - `GC_ALLOW_DIRTY=1 tools/replay_trace_case_os_link.sh tests/trace-harvest/os_link/mp4_rvz_v1/hit_000001_pc_800B7D24_lr_8003222C`
  - pass marker: `0xDEADBEEF`
  - core semantics aligned for this case (return/head/tail/link nullability flags).
- `OSUnlink` harvest status:
  - Entry PC `0x800B8180` produced `0` hits in boot-window runs (60s and 240s).
  - Current conclusion: `OSUnlink` path is not reached in early boot; needs explicit later-game trigger path for RVZ harvesting.
  - Additional evidence: overlay-kill checkpoint also not reached in boot-window:
    - `tools/dump_expected_rvz_probe_at_pc.sh ... 0x8002F014 ... 180 1`
    - result: breakpoint `omOvlKill` not hit before timeout.
  - Trigger-path memo saved at:
    - `docs/sdk/mp4/OSUnlink_trigger_path.md`

## 2026-02-12: OSUnlink replay harness added (ready for real trace cases)
- Added replay harness script:
  - `tools/replay_trace_case_os_unlink.sh`
- Added host scenario:
  - `tests/sdk/os/os_unlink/host/os_unlink_rvz_trace_replay_001_scenario.c`
- Replay contract mirrors OSLink flow:
  - Reads `in_*`/`out_*` trace blobs (`module_info`, `os_module_list`, regs)
  - Seeds queue/link state from `in_*` nullability and `head/tail == oldModule` flags
  - Calls `OSUnlink`, compares return + post-state nullability/ownership flags
  - Writes marker `0xDEADBEEF` on pass.
- Validation:
  - Synthetic case replay PASS:
    `GC_ALLOW_DIRTY=1 tools/replay_trace_case_os_unlink.sh tests/trace-harvest/os_unlink/synth_case_001`
  - Harvest script smoke run:
    `GC_MAX_UNIQUE=1 GC_MAX_HITS=5 GC_TIMEOUT=30 tools/harvest_and_replay_os_unlink.sh`
    completed with `unique=0` (no real RVZ hits yet), so replay-on-real-case remains blocked on trigger-path capture.

## 2026-02-11: DVDReadPrio MP4 suite added and passing
- Added suite:
  - DOL: `tests/sdk/dvd/dvd_read_prio/dol/mp4/dvd_read_prio_mp4_init_mem_001/`
  - Host: `tests/sdk/dvd/dvd_read_prio/host/dvd_read_prio_mp4_init_mem_001_scenario.c`
- Deterministic fixture:
  - path `/meminfo.bin`, read window `(len=0x20, off=0x10)`, priority `2`.
- Validation:
  - expected: `tests/sdk/dvd/dvd_read_prio/expected/dvd_read_prio_mp4_init_mem_001.bin`
  - actual: `tests/sdk/dvd/dvd_read_prio/actual/dvd_read_prio_mp4_init_mem_001.bin`
  - compare: `PASS` (bit-identical).

## 2026-02-11: PBT backlog execution batch (new suites)
- Added and validated new property suites (all 200k iterations, seed `0xC0DEC0DE`):
  - `tests/pbt/os/os_arena/os_arena_pbt.c`
  - `tests/pbt/os/os_interrupts/os_interrupts_pbt.c`
  - `tests/pbt/os/os_module_queue/os_module_queue_pbt.c`
  - `tests/pbt/si/si_sampling_rate/si_sampling_rate_pbt.c`
  - `tests/pbt/si/si_get_response/si_get_response_pbt.c`
  - `tests/pbt/pad/pad_control_motor/pad_control_motor_pbt.c`
  - `tests/pbt/pad/pad_reset_clamp/pad_reset_clamp_pbt.c`
  - `tests/pbt/vi/vi_post_retrace_callback/vi_post_retrace_callback_pbt.c`
  - `tests/pbt/dvd/dvd_read_prio/dvd_read_prio_pbt.c`
  - `tests/pbt/dvd/dvd_read_async_prio/dvd_read_async_prio_pbt.c`
  - `tests/pbt/dvd/dvd_convert_path/dvd_convert_path_pbt.c`
- Runner extended:
  - `tools/run_pbt.sh` suites added:
    - `os_arena`
    - `os_interrupts`
    - `os_module_queue`
    - `si_sampling_rate`
    - `si_get_response`
    - `pad_control_motor`
    - `pad_reset_clamp`
    - `vi_post_retrace_callback`
    - `dvd_read_prio`
    - `dvd_read_async_prio`
    - `dvd_convert_path`
- New backlog index file:
  - `docs/codex/PBT_BACKLOG.md`
- One failed first attempt (useful fact):
  - Initial `si_get_response` PBT used a host stack pointer as output and failed.
  - Correct behavior requires MEM1 output address (function writes through emulated RAM), after fix the suite passes.
  - Initial `dvd_convert_path` fuzz run failed due to duplicate generated paths mapping to first-match index.
  - Fix: enforce uniqueness in generated path table before checking index mapping.

## 2026-02-11: GX PBT batch unblocked and closed
- Added missing runner wiring in `tools/run_pbt.sh` for:
  - `gx_texcopy_relation`
  - `gx_vtxdesc_packing`
  - `gx_alpha_tev_packing`
- Build fix:
  - Link `src/sdk_port/gc_mem.c` with all GX PBT suites (`GX.c` references `gc_mem_ptr`).
- Validation (all PASS, 200k iterations, seed `0xC0DEC0DE`):
  - `tools/run_pbt.sh gx_texcopy_relation 200000 0xC0DEC0DE`
  - `tools/run_pbt.sh gx_vtxdesc_packing 200000 0xC0DEC0DE`
  - `tools/run_pbt.sh gx_alpha_tev_packing 200000 0xC0DEC0DE`
- Result:
  - BD tasks closed:
    - `gamecube-8b2x`
    - `gamecube-hw36`
    - `gamecube-wqzd`

## 2026-02-11: OSUnlink overlay-chain probing + movie pipeline
- Added chain probe script:
  - `tools/probe_os_unlink_overlay_chain.sh`
  - probes in one command: `omOvlGotoEx` (`0x8002EEC0`) -> `omOvlKill` (`0x8002F014`) -> `omDLLNumEnd` (`0x80031FA0`) -> `OSUnlink` (`0x800B8180`)
- Evidence run (45s per probe):
  - output: `tests/trace-harvest/os_unlink/probes/20260211_160246/`
  - result: no hit for all 4 PCs in boot-window run.
- Added movie-capable harvest plumbing:
  - `tools/ram_dump.py` now supports `--movie <.dtm>` (passes `-m` to Dolphin)
  - `tools/trace_pc_entry_exit.py` now supports `--movie <.dtm>`
  - new one-button script: `tools/harvest_and_replay_os_unlink_movie.sh`
- Practical conclusion:
  - We now have deterministic input automation wiring; remaining blocker is a valid MP4 `.dtm` that reaches overlay unload.
## 2026-02-11: Validation run of merged Claude PBT suites on `main`

- Full gate run passed on this machine:
  - `tools/run_pbt_chain_gate.sh`
  - Included mutation checks and all expected FAIL-under-mutation outcomes for MTX, DVD fast-open, ARQ callback, CARD alloc/free invariants.
- Explicit reruns (host) passed:
  - `bash tools/run_property_test.sh --num-runs=200 --seed=0xC0DEC0DE -v` (OSAlloc)
  - `bash tools/run_mtx_property_test.sh --num-runs=200 --seed=0xC0DEC0DE -v` (MTX)
  - `bash tools/run_osthread_property_test.sh --num-runs=200 --seed=0xC0DEC0DE -v` (OSThread/Mutex/Message)
  - `bash tools/run_stopwatch_property_test.sh --num-runs=200 --seed=0xC0DEC0DE -v` (OSStopwatch)
  - `bash tools/run_property_dvdfs.sh --num-runs=200 --seed=0xC0DEC0DE -v` (DVDFS)
  - `bash tools/run_arq_property_test.sh --num-runs=200 --seed=0xC0DEC0DE` (ARQ)
  - `bash tools/run_card_fat_property_test.sh --num-runs=200 --seed=0xC0DEC0DE` (CARD FAT)
- Practical note from verification:
  - ARQ/CARD property binaries report `Seeds: 1` when a fixed `--seed` is supplied (deterministic single-seed mode), while gate-mode without fixed seed iterates multiple seeds.
## 2026-02-11: Claude PBT leaf-audit (bottom-up completeness check)

- Audited merged Claude PBT suites for "leaf-first then upward levels" structure (OSAlloc-style baseline).
- Result:
  - Not uniform 100% leaf-complete across all suites.
  - Strong layered suites exist (OSAlloc, OSThread, ARQ, CARD FAT, MTX family).
  - Some suites remain scenario-centric vs explicit full leaf ladder (notably DVDFS).
- Evidence report:
  - `docs/codex/PBT_LEAF_AUDIT_CLAUDE_2026-02-11.md`
## 2026-02-11: Micropolis-alignment cleanup for PBT workflow

- Added explicit alignment report:
  - `docs/codex/MICROPOLIS_ALIGNMENT.md`
- Added replay gate runner so replay verification is one-command:
  - `tools/run_replay_gate.sh`
- Added deterministic failure minimizer helper for parity/PBT regressions:
  - `tools/pbt_minimize_failure.sh`
- Updated workflow wording to avoid over-claiming framework-grade PBT semantics:
  - `docs/codex/WORKFLOW.md`
## 2026-02-11: Full PBT/replay consistency sweep

- Fixed runner reference mismatch in status doc:
  - `docs/codex/PROJECT_STATUS.md` now points DVDFS to `tools/run_property_dvdfs.sh`.
- Normalized property runner script permissions so they can be run directly (`./...`):
  - `tools/run_property_test.sh`
  - `tools/run_mtx_property_test.sh`
  - `tools/run_osthread_property_test.sh`
  - `tools/run_ostime_property_test.sh`
  - `tools/run_stopwatch_property_test.sh`
- Fixed stale task doc target for not-yet-existing OSUnlink replay script:
  - `docs/codex/TASKS.md` now states this script still needs to be created.
- Verification run (all PASS, 20 deterministic seeds):
  - OSAlloc, MTX, OSThread, OSTime, OSStopwatch, DVDFS, ARQ, CARD-FAT property/parity runners.
## 2026-02-11: OSAlloc-style PBT standardization sweep (step-up)

- Added standard matrix and checklist baseline:
  - `docs/codex/PBT_OSALLOC_STANDARD_MATRIX.md`
- Added cross-suite leaf-to-level mapping:
  - `docs/codex/leaf_maps/PBT_LEAF_MAPS.md`
- Added focused level targeting where missing:
  - `tests/sdk/os/stopwatch/property/stopwatch_property_test.c` now supports `--op=L0|L1|RANDOM|MULTI`
  - `tests/sdk/dvd/dvdfs/property/dvdfs_property_test.c` now supports `--op=L0|L1|L2|L3|L4|MIX`
- Updated DVDFS property README with per-level run commands:
  - `tests/sdk/dvd/dvdfs/property/README.md`
- Validation:
  - `./tools/run_stopwatch_property_test.sh --seed=0xC0DEC0DE --op=L0 -v` PASS
  - `./tools/run_stopwatch_property_test.sh --seed=0xC0DEC0DE --op=L1 -v` PASS
  - `./tools/run_property_dvdfs.sh --seed=0xC0DEC0DE --op=L0..L4 -v` PASS
## 2026-02-11: Pulled latest Claude PBT batch into current branch

- Merged remote branch `origin/pbt/remaining-sdk-coverage` into `codex/macos-codex53-pbt-audit`.
- New suites added by that batch:
  - `tests/sdk/dvd/property/dvdqueue_property_test.c`
  - `tests/sdk/gx/property/gxtexture_property_test.c`
  - `tests/sdk/os/osalarm/property/osalarm_property_test.c`
  - `tests/sdk/pad/property/padclamp_property_test.c`
- Validation reruns on this branch (all PASS, deterministic seed):
  - `bash tools/run_dvdqueue_property_test.sh --num-runs=50 --seed=0xC0DEC0DE`
  - `bash tools/run_gxtexture_property_test.sh --num-runs=50 --seed=0xC0DEC0DE`
  - `bash tools/run_osalarm_property_test.sh --num-runs=50 --seed=0xC0DEC0DE`
  - `bash tools/run_padclamp_property_test.sh --num-runs=50 --seed=0xC0DEC0DE`
## 2026-02-11: Synced second latest PBT push into current branch

- Pulled and merged updated `origin/pbt/remaining-sdk-coverage` (new commit batch) into `codex/macos-codex53-pbt-audit`.
- New suites added in this sync:
  - `tests/sdk/gx/property/gxproject_property_test.c`
  - `tests/sdk/gx/property/gxz16_property_test.c`
  - `tests/sdk/gx/property/gxyscale_property_test.c`
  - `tests/sdk/thp/property/thpaudio_property_test.c`
- Validation reruns on this branch (PASS):
  - `bash tools/run_gxproject_property_test.sh --num-runs=20 --seed=0xC0DEC0DE`
  - `bash tools/run_gxz16_property_test.sh --num-runs=20 --seed=0xC0DEC0DE`
  - `bash tools/run_gxyscale_property_test.sh --num-runs=20 --seed=0xC0DEC0DE`
  - `bash tools/run_thpaudio_property_test.sh --num-runs=20 --seed=0xC0DEC0DE`
## 2026-02-11: OSAlloc-style `--op` targeting added for new 3 suites

- Upgraded these suites to OSAlloc-style focused level execution:
  - `tests/sdk/dvd/property/dvdqueue_property_test.c`
  - `tests/sdk/gx/property/gxtexture_property_test.c`
  - `tests/sdk/os/osalarm/property/osalarm_property_test.c`
- Added `--op=` parsing + per-level gating in `run_seed`:
  - dvdqueue: `L0..L4` + `PUSH|DEQUEUE|INQUEUE|PROP|FULL|MIX`
  - gxtexture: `L0..L4` + `PARITY|NOMIP|MONO|RGBA8|TILE`
  - osalarm: `L0..L4` + `INSERT|CANCEL|FIRE|PERIODIC|FULL|MIX`
- Validation (PASS):
  - `bash tools/run_dvdqueue_property_test.sh --seed=0xC0DEC0DE --op=L0 -v`
  - `bash tools/run_dvdqueue_property_test.sh --seed=0xC0DEC0DE --op=L4 -v`
  - `bash tools/run_gxtexture_property_test.sh --seed=0xC0DEC0DE --op=L1 -v`
  - `bash tools/run_gxtexture_property_test.sh --seed=0xC0DEC0DE --op=L4 -v`
  - `bash tools/run_osalarm_property_test.sh --seed=0xC0DEC0DE --op=L0 -v`
  - `bash tools/run_osalarm_property_test.sh --seed=0xC0DEC0DE --op=L4 -v`
## 2026-02-11: OSAlloc-style `--op` targeting added for latest 4 suites

- Upgraded these suites to OSAlloc-style focused level execution:
  - `tests/sdk/gx/property/gxproject_property_test.c`
  - `tests/sdk/gx/property/gxz16_property_test.c`
  - `tests/sdk/gx/property/gxyscale_property_test.c`
  - `tests/sdk/thp/property/thpaudio_property_test.c`
- Added `--op=` parsing + per-level gating in `run_seed`:
  - GXProject: `L0..L5` + `PARITY|IDENTITY|ORTHO|DEPTH|CENTER|FULL|MIX`
  - GXZ16: `L0..L5` + `PARITY|ROUNDTRIP|IDEMPOTENCE|RANGE|EXHAUSTIVE|FULL|MIX`
  - GXYScale: `L0..L5` + `NUMXFB|YSCALE|RESULT|BOUNDED|IDENTITY|FULL|MIX`
  - THPAudio: `L0..L4` + `MONO|STEREO|CHANNEL|SATURATION|FULL|MIX`
- Validation (PASS):
  - `bash tools/run_gxproject_property_test.sh --seed=0xC0DEC0DE --op=L0 -v`
  - `bash tools/run_gxproject_property_test.sh --seed=0xC0DEC0DE --op=L5 -v`
  - `bash tools/run_gxz16_property_test.sh --seed=0xC0DEC0DE --op=L1 -v`
  - `bash tools/run_gxz16_property_test.sh --seed=0xC0DEC0DE --op=L5 -v`
  - `bash tools/run_gxyscale_property_test.sh --seed=0xC0DEC0DE --op=L2 -v`
  - `bash tools/run_gxyscale_property_test.sh --seed=0xC0DEC0DE --op=L5 -v`
  - `bash tools/run_thpaudio_property_test.sh --seed=0xC0DEC0DE --op=L0 -v`
  - `bash tools/run_thpaudio_property_test.sh --seed=0xC0DEC0DE --op=L4 -v`
## 2026-02-11: Synced latest candidate-coverage doc push

- Merged latest `origin/pbt/remaining-sdk-coverage` updates into `codex/macos-codex53-pbt-audit`.
- `docs/codex/PROJECT_STATUS.md` now includes the "PBT coverage — complete" status block from that push.
- Kept local OSAlloc-style `--op` upgrades in all new suites during conflict resolution.
- Quick regression checks after merge (PASS):
  - `bash tools/run_dvdqueue_property_test.sh --num-runs=5 --seed=0xC0DEC0DE --op=L0`
  - `bash tools/run_gxproject_property_test.sh --num-runs=5 --seed=0xC0DEC0DE --op=L5`
  - `bash tools/run_thpaudio_property_test.sh --num-runs=5 --seed=0xC0DEC0DE --op=L4`
## 2026-02-11: Branch consolidation into codex/integration-all

- Merged latest remote work from:
  - `origin/codex/macos-codex53-pbt-audit`
  - `origin/pbt/remaining-sdk-coverage`
- Resolved add/add conflicts for new PBT suite files by keeping integration-all copies with OSAlloc-style `--op` targeting.
- Verified no remote branch remains ahead of `codex/integration-all` after consolidation (`ahead_of_integration_all` list empty).
- Post-merge validation (PASS):
  - `tools/run_dvdqueue_property_test.sh --num-runs=20 --seed=0xC0DEC0DE --op=L4`
  - `tools/run_gxtexture_property_test.sh --num-runs=20 --seed=0xC0DEC0DE --op=L4`
  - `tools/run_osalarm_property_test.sh --num-runs=20 --seed=0xC0DEC0DE --op=L4`
  - `tools/run_gxproject_property_test.sh --num-runs=10 --seed=0xC0DEC0DE --op=L5`
  - `tools/run_gxz16_property_test.sh --num-runs=10 --seed=0xC0DEC0DE --op=L5`
  - `tools/run_gxyscale_property_test.sh --num-runs=10 --seed=0xC0DEC0DE --op=L5`
  - `tools/run_thpaudio_property_test.sh --num-runs=10 --seed=0xC0DEC0DE --op=L4`

## 2026-02-12: Small SDK blockers batch (OSGetTick / OSTicksToCalendarTime / OSDumpStopwatch / DVDCancel)

- Added missing sdk_port implementations:
  - `src/sdk_port/os/OSRtc.c`: `OSGetTick` (deterministic +1ms step per call), `OSTicksToCalendarTime` (ported from decomp math/date conversion).
  - `src/sdk_port/os/OSStopwatch.c`: `OSDumpStopwatch` formatting via `OSReport`.
  - `src/sdk_port/dvd/DVD.c`: `DVDCancel` minimal deterministic cancel semantics.
- Added deterministic unit checks:
  - `tests/sdk/os/ostime/property/ostime_sdk_port_unit_test.c`
  - `tests/sdk/dvd/property/dvdcancel_unit_test.c`
  - runners: `tools/run_ostime_sdk_port_unit_test.sh`, `tools/run_dvdcancel_unit_test.sh`
- Validation run (PASS):
  - `bash tools/run_ostime_sdk_port_unit_test.sh`
  - `bash tools/run_dvdcancel_unit_test.sh`
  - `bash tools/run_ostime_property_test.sh --num-runs=20 --seed=0xC0DEC0DE`
  - `bash tools/run_stopwatch_property_test.sh --num-runs=20 --seed=0xC0DEC0DE`

## 2026-02-12: Replay suite consolidation + test-layout migration scaffold

- Added consolidated replay runner:
  - `tools/replay_trace_suite.sh` (runs representative replay cases across OS/VI/SI/PAD + OSLink/OSUnlink synth)
  - updated `tools/run_replay_gate.sh` to call consolidated suite first; harvest scripts optional behind `GC_REPLAY_HARVEST=1`.
- Verified replay suite smoke (1 case per corpus) PASS:
  - `GC_REPLAY_MAX_CASES=1 bash tools/replay_trace_suite.sh`
  - Note: PADSetSpec `mp4_rvz_v1` case lacks `in_pad_spec_shadow.bin`; suite now treats rc=2 as SKIP for non-runnable cases.
- Started directory migration planning for clearer test semantics:
  - plan doc: `docs/codex/TEST_LAYOUT_MIGRATION.md`
  - new scaffold dirs:
    - `tests/trace-harvest/`
    - `tests/sdk/callsite/`
    - `tests/sdk/pbt/`
- Completed phase-2 trace move:
  - moved trace corpora from `tests/traces/*` to `tests/trace-harvest/*`
  - removed legacy `tests/traces` alias after script retarget
  - replay smoke re-run remains PASS:
    - `GC_REPLAY_MAX_CASES=1 bash tools/replay_trace_suite.sh`
- Added compatibility mirrors for next migration phases:
  - `tests/sdk/callsite/<subsystem>` symlinks to existing `tests/sdk/<subsystem>`
  - `tests/sdk/pbt/legacy` symlink to `tests/pbt`
  - `tests/sdk/pbt/<subsystem>/...` symlink entries to existing property suites
- Retargeted replay suite globs to prefer `tests/trace-harvest/...` directly.

## 2026-02-12: Trace-path migration finalized (`tests/traces` removed)

- Finalized migration to `tests/trace-harvest` as canonical retail-trace path.
- Updated replay/harvest tooling and docs to use `tests/trace-harvest/...`.
- Removed legacy `tests/traces` alias path from repository.
- Validation (PASS):
  - `GC_REPLAY_MAX_CASES=1 bash tools/run_replay_gate.sh`

## 2026-02-12: Integration-all migration test branch validation

- Created integration validation branch from `origin/codex/integration-all`:
  - `codex/integration-all-migration-test`
- Merged:
  - `codex/macos-codex5.3-migrate-tests-layout`
- Replay gate validation on integration branch (PASS):
  - `GC_REPLAY_MAX_CASES=1 bash tools/run_replay_gate.sh`
- Observed known non-blocking replay skip retained:
  - `pad_set_spec` v1 case missing `in_pad_spec_shadow.bin` (treated as SKIP by replay suite).

## 2026-02-12: Claude inline-migration branch re-check after latest push

- Re-fetched `origin/claude-win11/pbt-migrate-inline-to-sdk-port`.
- Latest commit now includes merge from `origin/main`:
  - `d67e0c3 Merge remote-tracking branch 'origin/main' into claude-win11/pbt-migrate-inline-to-sdk-port`
- Verified blockers/tests now present on that branch:
  - `src/sdk_port/dvd/DVD.c` (`DVDCancel`)
  - `src/sdk_port/os/OSRtc.c` (`OSGetTick`, `OSTicksToCalendarTime`)
  - `src/sdk_port/os/OSStopwatch.c` (`OSDumpStopwatch`)
  - `tests/sdk/dvd/property/dvdcancel_unit_test.c`
  - `tests/sdk/os/ostime/property/ostime_sdk_port_unit_test.c`
  - `tools/run_dvdcancel_unit_test.sh`, `tools/run_ostime_sdk_port_unit_test.sh`
- Validation in isolated worktree (`.worktrees/claude-inline-check`) PASS:
  - `bash tools/run_dvdcancel_unit_test.sh`
  - `bash tools/run_ostime_sdk_port_unit_test.sh`
  - `bash tools/run_ostime_property_test.sh --num-runs=20 --seed=0xC0DEC0DE`

## 2026-02-12: Merged Claude inline migration into integration-all test branch

- Branch merge completed on `codex/integration-all-migration-test`:
  - merged `origin/claude-win11/pbt-migrate-inline-to-sdk-port`
- Post-merge validation PASS:
  - `GC_REPLAY_MAX_CASES=1 bash tools/run_replay_gate.sh`
  - `bash tools/run_dvdcancel_unit_test.sh`
  - `bash tools/run_ostime_sdk_port_unit_test.sh`
  - `bash tools/run_dvdqueue_property_test.sh --num-runs=50 --seed=0xC0DEC0DE`
  - `bash tools/run_osalarm_property_test.sh --num-runs=50 --seed=0xC0DEC0DE`
  - `bash tools/run_padclamp_property_test.sh --num-runs=50 --seed=0xC0DEC0DE`
  - `bash tools/run_gxproject_property_test.sh --num-runs=50 --seed=0xC0DEC0DE`
  - `bash tools/run_gxz16_property_test.sh --num-runs=50 --seed=0xC0DEC0DE`
  - `bash tools/run_gxyscale_property_test.sh --num-runs=50 --seed=0xC0DEC0DE`
  - `bash tools/run_gxtexture_property_test.sh --num-runs=50 --seed=0xC0DEC0DE`
  - `bash tools/run_ostime_property_test.sh --num-runs=50 --seed=0xC0DEC0DE`
- Known replay status unchanged:
  - `pad_set_spec` v1 replay case remains SKIP due to missing `in_pad_spec_shadow.bin` in trace corpus.

## 2026-02-12: OSUnlink trigger-path progress (RVZ probes + omOvlGotoEx analysis)

- Updated trigger tooling:
  - `tools/probe_os_unlink_overlay_chain.sh` now defaults `DOLPHIN_START_DELAY=6` for RVZ boot stability.
  - `tools/harvest_and_replay_os_unlink_movie.sh` now supports zero-arg defaults:
    - RVZ from `docs/sdk/mp4/MP4_assets.md`
    - DTM from `tests/trace-harvest/os_unlink/inputs/mp4_overlay_trigger_001.dtm`
  - Added `tests/trace-harvest/os_unlink/inputs/README.md`.
- Probe result with delay fix:
  - `bash tools/probe_os_unlink_overlay_chain.sh \"\" 20`
  - `omOvlGotoEx` (`0x8002EEC0`) now hits in boot window.
  - `omOvlKill` (`0x8002F014`), `omDLLNumEnd` (`0x80031FA0`), `OSUnlink` (`0x800B8180`) still not hit in boot window.
  - Evidence: `tests/trace-harvest/os_unlink/probes/20260212_113053/`.
- `omOvlGotoEx` trace capture/analysis:
  - Captured: `tests/trace-harvest/os_unlink/omovlgotoex_boot_v2/` and `.../omovlgotoex_boot_v3/`.
  - Entry regs at first boot hit: `r3=1`, `r4=1`, `r5=0`, `r6=0`.
  - `objmain` globals dump (`0x801D3CC0:0x40`) shows:
    - `omcurovl` (`0x801D3CE0`) = `-1`
    - `omovlhisidx` (`0x801D3CD8`) = `0`
  - Decomp-consistent conclusion: first `omOvlGotoEx` does not call `omOvlKill` because `omcurovl < 0`; we need a later overlay transition (DTM-driven path) to reach `OSUnlink`.
- Additional long-window probe:
  - `DOLPHIN_START_DELAY=6 bash tools/dump_expected_rvz_probe_at_pc.sh "<MP4_RVZ>" 0x8002F014 tests/trace-harvest/os_unlink/probes/omovlkill_long_001 180 1`
  - Result: `omOvlKill` not hit within 180s boot-only run (timeout, hits=0).

## 2026-02-12: DVDReadPrio invariants property suite

- Added new property suite:
  - `tests/sdk/dvd/property/dvdreadprio_property_test.c`
  - `tools/run_dvdreadprio_property_test.sh`
- Invariants covered:
  - `L0/PARITY`: `DVDReadPrio` return parity with strict read-window oracle.
  - `L1/COPY`: copied byte window (and no-write on invalid cases).
  - `L2/STATE`: RAM-backed `GC_SDK_OFF_DVD_READ_CALLS`, `...LAST_READ_LEN`, `...LAST_READ_OFF` side effects.
  - `L3/PRIO`: priority argument does not change return/payload semantics.
  - `FULL`: combined run.
- Validation (PASS):
  - `bash tools/run_dvdreadprio_property_test.sh --num-runs=50 --seed=0xC0DEC0DE --op=L0`
  - `bash tools/run_dvdreadprio_property_test.sh --num-runs=50 --seed=0xC0DEC0DE --op=L1`
  - `bash tools/run_dvdreadprio_property_test.sh --num-runs=50 --seed=0xC0DEC0DE --op=L2`
  - `bash tools/run_dvdreadprio_property_test.sh --num-runs=50 --seed=0xC0DEC0DE --op=L3`
  - `bash tools/run_dvdreadprio_property_test.sh --num-runs=50 --seed=0xC0DEC0DE --op=FULL`

## 2026-02-12: GXInitTexObjLOD chain coverage reconciled

- Revalidated MP4 `pfDrawFonts -> GXInitTexObjLOD` suite:
  - `python3 tools/ram_compare.py tests/sdk/gx/gx_init_tex_obj_lod/expected/gx_init_tex_obj_lod_mp4_pfdrawfonts_001.bin tests/sdk/gx/gx_init_tex_obj_lod/actual/gx_init_tex_obj_lod_mp4_pfdrawfonts_001.bin`
  - Result: `PASS: files are identical`
- Updated chain row in `docs/sdk/mp4/MP4_chain_all.csv`:
  - `tests_exp_act` `0/1 -> 1/1`
  - `actual_bins` `0 -> 1`
  - `pass_bins` `0 -> 1`
  - `covered` now `y`

## 2026-02-12: DVDFS leaf-level property ops revalidated

- Re-ran DVDFS property suite across per-leaf ops + full mix:
  - `bash tools/run_property_dvdfs.sh --op=L0 --seed=0xDEADBEEF --num-runs=200`
  - `bash tools/run_property_dvdfs.sh --op=L1 --seed=0xDEADBEEF --num-runs=200 --steps=200`
  - `bash tools/run_property_dvdfs.sh --op=L2 --seed=0xDEADBEEF --num-runs=200 --steps=200`
  - `bash tools/run_property_dvdfs.sh --op=L3 --seed=0xDEADBEEF --num-runs=200 --steps=200`
  - `bash tools/run_property_dvdfs.sh --op=L4 --seed=0xDEADBEEF --num-runs=200 --steps=200`
  - `bash tools/run_property_dvdfs.sh --op=FULL --seed=0xDEADBEEF --num-runs=200 --steps=200`
- Result: all runs completed successfully (exit 0) with current `src/sdk_port/dvd/DVD.c` DVDFS behavior.

## 2026-02-12: Remote push scan snapshot

- Latest noteworthy remote heads:
  - `origin/claude-win11/ar-pbt-and-coverage-update` @ `3b71775` (CARD directory chain PBT + coverage update)
  - `origin/claude-win11/pbt-migrate-inline-to-sdk-port` @ `d67e0c3` (now includes merge from `origin/main`)
  - `origin/main` and `origin/codex/integration-all` aligned at `04a4724`.

## 2026-02-12: Integrated AR + CARD directory PBT coverage branch

- Merged `origin/claude-win11/ar-pbt-and-coverage-update` into `codex/osunlink-groundtruth-next`.
- Added sdk_port modules:
  - `src/sdk_port/ar/ar.c`, `src/sdk_port/ar/ar.h`
  - `src/sdk_port/card/card_dir.c`, `src/sdk_port/card/card_dir.h`
- Added property suites/runners:
  - `tests/sdk/ar/property/ar_property_test.c`
  - `tests/sdk/card/property/card_dir_property_test.c`
  - `tools/run_ar_property_test.sh`
  - `tools/run_card_dir_property_test.sh`
- Validation (PASS):
  - `bash tools/run_ar_property_test.sh --num-runs=50 --seed=0xC0DEC0DE`
  - `bash tools/run_card_dir_property_test.sh --num-runs=50 --seed=0xC0DEC0DE`

## 2026-02-12: todo snapshot refreshed

- Updated `todo/REMAINING_TEST_STRATEGY.md` and `todo/SDK_PORT_COVERAGE.md` with current remaining test bucket counts:
  - Trace replay: ~80
  - PBT: ~3
  - No test needed: ~10

## 2026-02-12: Promote Claude PBT additions into integration-all

- Pulled in Claude PBT commits (non-Windows):
  - `c5113c8` CARD unlock crypto suite + sdk_port `card_unlock`
  - `dc95dc4` GXLight / MTX44 / GXOverscan PBT suites
- Validation on `codex/integration-all` (all PASS):
  - `bash tools/run_card_unlock_property_test.sh --num-runs=50 --seed=0xC0DEC0DE` -> 6300/6300 PASS
  - `bash tools/run_gxlight_property_test.sh --num-runs=50 --seed=0xC0DEC0DE` -> 1500/1500 PASS
  - `bash tools/run_mtx44_property_test.sh --num-runs=50 --seed=0xC0DEC0DE` -> 500/500 PASS
  - `bash tools/run_gxoverscan_property_test.sh --num-runs=50 --seed=0xC0DEC0DE` -> 400/400 PASS
- Skipped Windows-only commit (`35dc47a`) by request.

## 2026-02-12: GXLoadNrmMtxImm revalidation

- Re-ran host scenario compare for `tests/sdk/gx/gx_load_nrm_mtx_imm`:
  - `bash tools/run_host_scenario.sh tests/sdk/gx/gx_load_nrm_mtx_imm/host/gx_load_nrm_mtx_imm_mp4_hsfdraw_001_scenario.c`
  - `python3 tools/ram_compare.py tests/sdk/gx/gx_load_nrm_mtx_imm/expected/gx_load_nrm_mtx_imm_mp4_hsfdraw_001.bin tests/sdk/gx/gx_load_nrm_mtx_imm/actual/gx_load_nrm_mtx_imm_mp4_hsfdraw_001.bin`
- Result: `PASS: files are identical`.

## 2026-02-12: VIWaitForRetrace callback behavior revalidation

- Re-ran `tests/sdk/vi/vi_wait_for_retrace` host scenarios and compares:
  - `vi_wait_for_retrace_mp4_init_vi_001`
  - `vi_wait_for_retrace_calls_post_cb_hupadinit_001`
- Commands:
  - `bash tools/run_host_scenario.sh tests/sdk/vi/vi_wait_for_retrace/host/vi_wait_for_retrace_mp4_init_vi_001_scenario.c`
  - `python3 tools/ram_compare.py tests/sdk/vi/vi_wait_for_retrace/expected/vi_wait_for_retrace_mp4_init_vi_001.bin tests/sdk/vi/vi_wait_for_retrace/actual/vi_wait_for_retrace_mp4_init_vi_001.bin`
  - `bash tools/run_host_scenario.sh tests/sdk/vi/vi_wait_for_retrace/host/vi_wait_for_retrace_calls_post_cb_hupadinit_001_scenario.c`
  - `python3 tools/ram_compare.py tests/sdk/vi/vi_wait_for_retrace/expected/vi_wait_for_retrace_calls_post_cb_hupadinit_001.bin tests/sdk/vi/vi_wait_for_retrace/actual/vi_wait_for_retrace_calls_post_cb_hupadinit_001.bin`
- Result: both compares PASS (bit-exact).

## 2026-02-12: PSMTX batch suite added (Reorder / ROMultVecArray / MultVecArray)

- Added sdk_port MTX batch functions in `src/sdk_port/mtx/mtx.c`:
  - `PSMTXReorder`
  - `PSMTXROMultVecArray`
  - `PSMTXMultVecArray`
- Added shared ROMtx types in `src/sdk_port/mtx/sdk_port_mtx_types.h`.
- Added oracle parity hooks in `tests/sdk/mtx/property/mtx_oracle.h`.
- Added new PBT suite + runner:
  - `tests/sdk/mtx/property/psmtx_batch_property_test.c`
  - `tools/run_psmtx_batch_property_test.sh`
- Validation commands (all PASS):
  - `bash tools/run_psmtx_batch_property_test.sh --op=L0 --num-runs=200 --seed=0xC0DEC0DE`
  - `bash tools/run_psmtx_batch_property_test.sh --op=L1 --num-runs=200 --steps=64 --seed=0xC0DEC0DE`
  - `bash tools/run_psmtx_batch_property_test.sh --op=L2 --num-runs=200 --steps=64 --seed=0xC0DEC0DE`
  - `bash tools/run_psmtx_batch_property_test.sh --op=FULL --num-runs=200 --steps=64 --seed=0xC0DEC0DE`
- Updated todo snapshots:
  - Remaining PBT bucket moved from `~3` to `~0` in `todo/REMAINING_TEST_STRATEGY.md` and `todo/SDK_PORT_COVERAGE.md`.

## 2026-02-12: MP4 workload reachability revalidation (HuPrcInit -> GWInit)

- Re-ran host workload scenarios:
  - `bash tools/run_host_scenario.sh tests/workload/mp4/mp4_huprcinit_001_scenario.c`
  - `bash tools/run_host_scenario.sh tests/workload/mp4/mp4_gwinit_001_scenario.c`
- Result: both scenarios built and executed successfully (reachability PASS).
- Note: expected host-only pointer-cast/ucontext deprecation warnings remain; no runtime failures.

## 2026-02-12: Workload checkpoint revalidation (mainloop + Hu3DInit slice path)

- Re-ran workload checkpoints:
  - `bash tools/run_host_scenario.sh tests/workload/mp4/mp4_init_to_viwait_001_scenario.c`
  - `bash tools/run_host_scenario.sh tests/workload/mp4/mp4_mainloop_one_iter_001_scenario.c`
  - `bash tools/run_host_scenario.sh tests/workload/mp4/mp4_mainloop_one_iter_tick_001_scenario.c`
  - `bash tools/run_host_scenario.sh tests/workload/mp4/mp4_mainloop_two_iter_tick_001_scenario.c`
- Result: all scenarios build+run successfully; reachability through Hu3DInit slice and gcsetjmp/gclongjmp path remains stable.
- Host warnings remain expected (32-bit pointer casts in decomp code + macOS ucontext deprecation), no runtime regression.

## 2026-02-12: RVZ probe helper revalidation (GWInit semantic compare)

- Re-ran host probe helper on GWInit checkpoint:
  - `bash tools/dump_actual_host_probe_at_scenario.sh tests/workload/mp4/mp4_gwinit_001_scenario.c tests/oracles/mp4_rvz/probes/host_gwinit_mp4_gwinit_001_v3`
- Re-ran semantic compare against RVZ probe values:
  - `python3 tools/helpers/compare_rvz_host_probe_values.py tests/oracles/mp4_rvz/probes/gwinit_pc_800308B8_v2/values.txt tests/oracles/mp4_rvz/probes/host_gwinit_mp4_gwinit_001_v3/values.txt --ignore __PADSpec`
- Result: all checked fields `OK` (arenaLo/arenaHi/__OSCurrHeap/__OSArenaLo/__OSArenaHi/SamplingRate).

## 2026-02-12: Remote branch scan snapshot

- Refreshed remote heads with `git fetch --all --prune` and recorded latest branch activity.
- Current newest notable heads:
  - `origin/codex/tests-next-batch` @ `a1a3918`
  - `origin/main` @ `80da948`
  - `origin/claude-win11/dolphin-windows-tooling` @ `35dc47a` (Windows tooling branch, not yet promoted)
  - `origin/claude-win11/ar-pbt-and-coverage-update` @ `dc95dc4`

## 2026-02-12: MP4 SDK usage vs PBT coverage audit refresh

- Audited property suites under `tests/sdk/**/property/*_property_test.c`.
- Current PBT suite inventory: 24 suites total.
  - ar: 2 (`ar_property_test`, `arq_property_test`)
  - card: 3 (`card_fat`, `card_dir`, `card_unlock`)
  - dvd: 3 (`dvdfs`, `dvdqueue`, `dvdreadprio`)
  - gx: 7 (`gxtexture`, `gxproject`, `gxyscale`, `gxz16`, `gxlight`, `gxoverscan`, `mtx44`)
  - mtx: 2 (`mtx_property`, `psmtx_batch_property`)
  - os: 5 (`osalloc`, `osthread`, `ostime`, `osalarm`, `stopwatch`)
  - pad: 1 (`padclamp`)
  - thp: 1 (`thpaudio`)
- Coverage interpretation stays aligned with `todo/SDK_PORT_COVERAGE.md`:
  - pure-computation gaps previously marked for MTX batch are now covered (PSMTX batch suite added)
  - remaining major gaps are hardware-coupled paths (AI/CARD APIs/THP runtime/parts of GX/DVD/VI) that require trace-replay or integration, not new PBT leaf suites.

## 2026-02-12: Probe connection retry hardening

- Updated `tools/ram_dump.py` startup connection behavior:
  - Added `--connect-retries` (default 20)
  - Added `--connect-retry-delay` (default 1.0s)
  - Startup now retries with configurable delay after Dolphin launch.
- Re-ran chain probe:
  - `bash tools/probe_os_unlink_overlay_chain.sh "" 20`
  - output: `tests/trace-harvest/os_unlink/probes/20260212_144829/`
- Result:
  - later probes now connect reliably enough to set breakpoints (`omOvlKill`, `omDLLNumEnd`, `OSUnlink` logs show connected + timeout-no-hit)
  - `omOvlGotoEx` still has startup connection refusal in this run
  - no overlay unload hits yet; DTM trigger input is still required for OSUnlink path.

## 2026-02-12: VISetPreRetraceCallback added with deterministic PPC-vs-host suite

- Added minimal sdk_port implementation:
  - `src/sdk_port/vi/VI.c`: `VISetPreRetraceCallback`
  - `src/sdk_port/sdk_state.h`: `GC_SDK_OFF_VI_PRE_CB_PTR`, `GC_SDK_OFF_VI_PRE_CB_SET_CALLS`
  - `tests/workload/include/dolphin/vi.h`: pre-callback declaration
- Added test suite:
  - `tests/sdk/vi/vi_set_pre_retrace_callback/`
  - DOL test: `dol/mp4/realistic_hupadinit_001`
  - Host scenario: `host/vi_set_pre_retrace_callback_realistic_hupadinit_001_scenario.c`
- Validation:
  - `python3 tools/ram_dump.py --exec tests/sdk/vi/vi_set_pre_retrace_callback/dol/mp4/realistic_hupadinit_001/vi_set_pre_retrace_callback_realistic_hupadinit_001.dol --addr 0x80300000 --size 0x40 --out tests/sdk/vi/vi_set_pre_retrace_callback/expected/vi_set_pre_retrace_callback_realistic_hupadinit_001.bin --run 0.5 --halt`
  - `bash tools/run_host_scenario.sh tests/sdk/vi/vi_set_pre_retrace_callback/host/vi_set_pre_retrace_callback_realistic_hupadinit_001_scenario.c`
  - `python3 tools/ram_compare.py tests/sdk/vi/vi_set_pre_retrace_callback/expected/vi_set_pre_retrace_callback_realistic_hupadinit_001.bin tests/sdk/vi/vi_set_pre_retrace_callback/actual/vi_set_pre_retrace_callback_realistic_hupadinit_001.bin`
  - Result: PASS (bit-exact).
- Coverage snapshot updated:
  - VI module now 13/13.
  - Remaining trace-replay bucket adjusted from ~80 to ~79.

## 2026-02-12: OSUnlink probe defaults tuned

- `tools/probe_os_unlink_overlay_chain.sh` and `tools/harvest_and_replay_os_unlink_movie.sh` default `DOLPHIN_START_DELAY` raised to `12`.
- Probe evidence:
  - `tests/trace-harvest/os_unlink/probes/20260212_145716/`
  - `omOvlGotoEx` now consistently hits with default delay 12; unload path probes (`omOvlKill`/`omDLLNumEnd`/`OSUnlink`) still miss without DTM trigger input.

## 2026-02-12: GXPixModeSync added + deterministic PPC-vs-host suite

- Added sdk_port function in `src/sdk_port/gx/GX.c`:
  - `GXPixModeSync()` now writes `gc_gx_pe_ctrl` BP register (`gx_write_ras_reg`) and clears `gc_gx_bp_sent_not`.
- Added deterministic suite:
  - `tests/sdk/gx/gx_pix_mode_sync/dol/mp4/gx_pix_mode_sync_mp4_init_gx_001/`
  - `tests/sdk/gx/gx_pix_mode_sync/host/gx_pix_mode_sync_mp4_init_gx_001_scenario.c`
- Scenario shape:
  - Set `peCtrl` via `GXSetZCompLoc(1)` (bit 6 set), force `bp_sent_not = 1`, call `GXPixModeSync`, then dump marker + peCtrl + lastRasReg + bpSentNot.
- Validation commands:
  - `cd tests/sdk/gx/gx_pix_mode_sync/dol/mp4/gx_pix_mode_sync_mp4_init_gx_001 && make clean && make`
  - `python3 tools/ram_dump.py --exec tests/sdk/gx/gx_pix_mode_sync/dol/mp4/gx_pix_mode_sync_mp4_init_gx_001/gx_pix_mode_sync_mp4_init_gx_001.dol --addr 0x80300000 --size 0x40 --out tests/sdk/gx/gx_pix_mode_sync/expected/gx_pix_mode_sync_mp4_init_gx_001.bin --run 0.5 --halt`
  - `tools/run_host_scenario.sh tests/sdk/gx/gx_pix_mode_sync/host/gx_pix_mode_sync_mp4_init_gx_001_scenario.c`
  - `python3 tools/ram_compare.py tests/sdk/gx/gx_pix_mode_sync/expected/gx_pix_mode_sync_mp4_init_gx_001.bin tests/sdk/gx/gx_pix_mode_sync/actual/gx_pix_mode_sync_mp4_init_gx_001.bin`
- Result: PASS (bit-exact).
- Coverage/todo snapshot updates:
  - `todo/REMAINING_TEST_STRATEGY.md`: trace replay `~79 -> ~78`, GX gap count `25 -> 24`.
  - `todo/SDK_PORT_COVERAGE.md`: GX `96 -> 97`, total ported `~225 -> ~226`.

## 2026-02-12: GXGetProjectionv added + deterministic PPC-vs-host suite

- Added sdk_port function in `src/sdk_port/gx/GX.c`:
  - `GXGetProjectionv(f32* ptr)` mirrors decomp `GXTransform.c`: returns `projType` as float in `ptr[0]` and `projMtx[0..5]` in `ptr[1..6]`.
- Added deterministic suite:
  - `tests/sdk/gx/gx_get_projectionv/dol/mp4/gx_get_projectionv_mp4_shadow_001/`
  - `tests/sdk/gx/gx_get_projectionv/host/gx_get_projectionv_mp4_shadow_001_scenario.c`
- Scenario shape:
  - set orthographic projection via `GXSetProjection`, then fetch with `GXGetProjectionv`, dump 7 float words + `bpSentNot`.
- Validation commands:
  - `cd tests/sdk/gx/gx_get_projectionv/dol/mp4/gx_get_projectionv_mp4_shadow_001 && make clean && make`
  - `python3 tools/ram_dump.py --exec tests/sdk/gx/gx_get_projectionv/dol/mp4/gx_get_projectionv_mp4_shadow_001/gx_get_projectionv_mp4_shadow_001.dol --addr 0x80300000 --size 0x40 --out tests/sdk/gx/gx_get_projectionv/expected/gx_get_projectionv_mp4_shadow_001.bin --run 0.5 --halt`
  - `tools/run_host_scenario.sh tests/sdk/gx/gx_get_projectionv/host/gx_get_projectionv_mp4_shadow_001_scenario.c`
  - `python3 tools/ram_compare.py tests/sdk/gx/gx_get_projectionv/expected/gx_get_projectionv_mp4_shadow_001.bin tests/sdk/gx/gx_get_projectionv/actual/gx_get_projectionv_mp4_shadow_001.bin`
- Result: PASS (bit-exact).
- Coverage/todo snapshot updates:
  - `todo/REMAINING_TEST_STRATEGY.md`: trace replay `~78 -> ~77`, GX gap count `24 -> 23`.
  - `todo/SDK_PORT_COVERAGE.md`: GX `97 -> 98`, total ported `~226 -> ~227`.

## 2026-02-12: GXResetWriteGatherPipe host-safe stub + deterministic suite

- Added sdk_port function in `src/sdk_port/gx/GX.c`:
  - `GXResetWriteGatherPipe()` as host-safe no-op (decomp function only mutates PPC WPAR hardware state).
- Added deterministic suite:
  - `tests/sdk/gx/gx_reset_write_gather_pipe/dol/mp4/gx_reset_write_gather_pipe_mp4_hsfdraw_001/`
  - `tests/sdk/gx/gx_reset_write_gather_pipe/host/gx_reset_write_gather_pipe_mp4_hsfdraw_001_scenario.c`
- Scenario shape:
  - initialize `bp_sent_not = 0x11223344`, call `GXResetWriteGatherPipe`, assert sentinel unchanged.
- Validation commands:
  - `cd tests/sdk/gx/gx_reset_write_gather_pipe/dol/mp4/gx_reset_write_gather_pipe_mp4_hsfdraw_001 && make clean && make`
  - `python3 tools/ram_dump.py --exec tests/sdk/gx/gx_reset_write_gather_pipe/dol/mp4/gx_reset_write_gather_pipe_mp4_hsfdraw_001/gx_reset_write_gather_pipe_mp4_hsfdraw_001.dol --addr 0x80300000 --size 0x40 --out tests/sdk/gx/gx_reset_write_gather_pipe/expected/gx_reset_write_gather_pipe_mp4_hsfdraw_001.bin --run 0.5 --halt`
  - `tools/run_host_scenario.sh tests/sdk/gx/gx_reset_write_gather_pipe/host/gx_reset_write_gather_pipe_mp4_hsfdraw_001_scenario.c`
  - `python3 tools/ram_compare.py tests/sdk/gx/gx_reset_write_gather_pipe/expected/gx_reset_write_gather_pipe_mp4_hsfdraw_001.bin tests/sdk/gx/gx_reset_write_gather_pipe/actual/gx_reset_write_gather_pipe_mp4_hsfdraw_001.bin`
- Result: PASS (bit-exact).
- Coverage snapshot update:
  - `todo/SDK_PORT_COVERAGE.md`: GX `98 -> 99`, total ported `~227 -> ~228`.

## 2026-02-12: GXSetNumIndStages added + deterministic PPC-vs-host suite

- Added sdk_port function in `src/sdk_port/gx/GX.c`:
  - `GXSetNumIndStages(u8 nIndStages)` mirrors `GXBump.c`:
    - updates `genMode` bits `16..18`
    - sets `dirtyState |= 6`
- Added deterministic suite:
  - `tests/sdk/gx/gx_set_num_ind_stages/dol/mp4/gx_set_num_ind_stages_mp4_init_gx_001/`
  - `tests/sdk/gx/gx_set_num_ind_stages/host/gx_set_num_ind_stages_mp4_init_gx_001_scenario.c`
- Validation commands:
  - `cd tests/sdk/gx/gx_set_num_ind_stages/dol/mp4/gx_set_num_ind_stages_mp4_init_gx_001 && make clean && make`
  - `python3 tools/ram_dump.py --exec tests/sdk/gx/gx_set_num_ind_stages/dol/mp4/gx_set_num_ind_stages_mp4_init_gx_001/gx_set_num_ind_stages_mp4_init_gx_001.dol --addr 0x80300000 --size 0x40 --out tests/sdk/gx/gx_set_num_ind_stages/expected/gx_set_num_ind_stages_mp4_init_gx_001.bin --run 0.5 --halt`
  - `tools/run_host_scenario.sh tests/sdk/gx/gx_set_num_ind_stages/host/gx_set_num_ind_stages_mp4_init_gx_001_scenario.c`
  - `python3 tools/ram_compare.py tests/sdk/gx/gx_set_num_ind_stages/expected/gx_set_num_ind_stages_mp4_init_gx_001.bin tests/sdk/gx/gx_set_num_ind_stages/actual/gx_set_num_ind_stages_mp4_init_gx_001.bin`
- Result: PASS (bit-exact).
- Coverage/todo snapshot updates:
  - `todo/REMAINING_TEST_STRATEGY.md`: trace replay `~77 -> ~76`, GX gap count `23 -> 22`.
  - `todo/SDK_PORT_COVERAGE.md`: GX `99 -> 100`, total ported `~228 -> ~229`.

## 2026-02-12: GXSetIndTexCoordScale added + deterministic PPC-vs-host suite

- Added sdk_port function in `src/sdk_port/gx/GX.c`:
  - `GXSetIndTexCoordScale(ind_state, scale_s, scale_t)` mirrors `GXBump.c` packing:
    - stage0/1 -> `gc_gx_ind_tex_scale0`, BP id `0x25`
    - stage2/3 -> `gc_gx_ind_tex_scale1`, BP id `0x26`
    - writes BP reg and clears `gc_gx_bp_sent_not`.
- Added deterministic suite:
  - `tests/sdk/gx/gx_set_ind_tex_coord_scale/dol/mp4/gx_set_ind_tex_coord_scale_mp4_init_gx_001/`
  - `tests/sdk/gx/gx_set_ind_tex_coord_scale/host/gx_set_ind_tex_coord_scale_mp4_init_gx_001_scenario.c`
- Validation commands:
  - `cd tests/sdk/gx/gx_set_ind_tex_coord_scale/dol/mp4/gx_set_ind_tex_coord_scale_mp4_init_gx_001 && make clean && make`
  - `python3 tools/ram_dump.py --exec tests/sdk/gx/gx_set_ind_tex_coord_scale/dol/mp4/gx_set_ind_tex_coord_scale_mp4_init_gx_001/gx_set_ind_tex_coord_scale_mp4_init_gx_001.dol --addr 0x80300000 --size 0x40 --out tests/sdk/gx/gx_set_ind_tex_coord_scale/expected/gx_set_ind_tex_coord_scale_mp4_init_gx_001.bin --run 0.5 --halt`
  - `tools/run_host_scenario.sh tests/sdk/gx/gx_set_ind_tex_coord_scale/host/gx_set_ind_tex_coord_scale_mp4_init_gx_001_scenario.c`
  - `python3 tools/ram_compare.py tests/sdk/gx/gx_set_ind_tex_coord_scale/expected/gx_set_ind_tex_coord_scale_mp4_init_gx_001.bin tests/sdk/gx/gx_set_ind_tex_coord_scale/actual/gx_set_ind_tex_coord_scale_mp4_init_gx_001.bin`
- Result: PASS (bit-exact).
- Coverage/todo snapshot updates:
  - `todo/REMAINING_TEST_STRATEGY.md`: trace replay `~76 -> ~75`, GX gap count `22 -> 21`.
  - `todo/SDK_PORT_COVERAGE.md`: GX `100 -> 101`, total ported `~229 -> ~230`.

## 2026-02-12: GXSetIndTexOrder added + deterministic PPC-vs-host suite

- Added sdk_port function in `src/sdk_port/gx/GX.c`:
  - `GXSetIndTexOrder(ind_stage, tex_coord, tex_map)` mirrors `GXBump.c`:
    - updates `gc_gx_iref` per-stage field pairs
    - writes BP reg
    - `gc_gx_dirty_state |= 3`
    - clears `gc_gx_bp_sent_not`.
- Added deterministic suite:
  - `tests/sdk/gx/gx_set_ind_tex_order/dol/mp4/gx_set_ind_tex_order_mp4_init_gx_001/`
  - `tests/sdk/gx/gx_set_ind_tex_order/host/gx_set_ind_tex_order_mp4_init_gx_001_scenario.c`
- Validation commands:
  - `cd tests/sdk/gx/gx_set_ind_tex_order/dol/mp4/gx_set_ind_tex_order_mp4_init_gx_001 && make clean && make`
  - `python3 tools/ram_dump.py --exec tests/sdk/gx/gx_set_ind_tex_order/dol/mp4/gx_set_ind_tex_order_mp4_init_gx_001/gx_set_ind_tex_order_mp4_init_gx_001.dol --addr 0x80300000 --size 0x40 --out tests/sdk/gx/gx_set_ind_tex_order/expected/gx_set_ind_tex_order_mp4_init_gx_001.bin --run 0.5 --halt`
  - `tools/run_host_scenario.sh tests/sdk/gx/gx_set_ind_tex_order/host/gx_set_ind_tex_order_mp4_init_gx_001_scenario.c`
  - `python3 tools/ram_compare.py tests/sdk/gx/gx_set_ind_tex_order/expected/gx_set_ind_tex_order_mp4_init_gx_001.bin tests/sdk/gx/gx_set_ind_tex_order/actual/gx_set_ind_tex_order_mp4_init_gx_001.bin`
- Result: PASS (bit-exact).
- Coverage/todo snapshot updates:
  - `todo/REMAINING_TEST_STRATEGY.md`: trace replay `~75 -> ~74`, GX gap count `21 -> 20`.
  - `todo/SDK_PORT_COVERAGE.md`: GX `101 -> 102`, total ported `~230 -> ~231`.

## 2026-02-12: GXSetTevDirect added + deterministic PPC-vs-host suite

- Added sdk_port function in `src/sdk_port/gx/GX.c`:
  - `GXSetTevDirect(tev_stage)` writes packed TEV indirect BP reg for direct defaults
    (`reg id = tev_stage + 16`) and clears `gc_gx_bp_sent_not`.
- Added deterministic suite:
  - `tests/sdk/gx/gx_set_tev_direct/dol/mp4/gx_set_tev_direct_mp4_init_gx_001/`
  - `tests/sdk/gx/gx_set_tev_direct/host/gx_set_tev_direct_mp4_init_gx_001_scenario.c`
- Validation commands:
  - `cd tests/sdk/gx/gx_set_tev_direct/dol/mp4/gx_set_tev_direct_mp4_init_gx_001 && make clean && make`
  - `python3 tools/ram_dump.py --exec tests/sdk/gx/gx_set_tev_direct/dol/mp4/gx_set_tev_direct_mp4_init_gx_001/gx_set_tev_direct_mp4_init_gx_001.dol --addr 0x80300000 --size 0x40 --out tests/sdk/gx/gx_set_tev_direct/expected/gx_set_tev_direct_mp4_init_gx_001.bin --run 0.5 --halt`
  - `tools/run_host_scenario.sh tests/sdk/gx/gx_set_tev_direct/host/gx_set_tev_direct_mp4_init_gx_001_scenario.c`
  - `python3 tools/ram_compare.py tests/sdk/gx/gx_set_tev_direct/expected/gx_set_tev_direct_mp4_init_gx_001.bin tests/sdk/gx/gx_set_tev_direct/actual/gx_set_tev_direct_mp4_init_gx_001.bin`
- Result: PASS (bit-exact).
- Coverage/todo snapshot updates:
  - `todo/REMAINING_TEST_STRATEGY.md`: trace replay `~74 -> ~73`, GX gap count `20 -> 19`.
  - `todo/SDK_PORT_COVERAGE.md`: GX `102 -> 103`, total ported `~231 -> ~232`.

## 2026-02-12: GXSetIndTexMtx added + deterministic PPC-vs-host suite

- Added sdk_port function in `src/sdk_port/gx/GX.c`:
  - `GXSetIndTexMtx(mtx_id, offset, scale_exp)` mirrors `GXBump.c`:
    - maps `mtx_id` to slot id
    - emits three packed BP regs (`id*3 + 6/7/8`)
    - writes via `gx_write_ras_reg` and clears `gc_gx_bp_sent_not`
  - Added observable mirrors: `gc_gx_ind_mtx_reg0/1/2`.
- Added deterministic suite:
  - `tests/sdk/gx/gx_set_ind_tex_mtx/dol/mp4/gx_set_ind_tex_mtx_mp4_init_gx_001/`
  - `tests/sdk/gx/gx_set_ind_tex_mtx/host/gx_set_ind_tex_mtx_mp4_init_gx_001_scenario.c`
- Validation commands:
  - `cd tests/sdk/gx/gx_set_ind_tex_mtx/dol/mp4/gx_set_ind_tex_mtx_mp4_init_gx_001 && make clean && make`
  - `python3 tools/ram_dump.py --exec tests/sdk/gx/gx_set_ind_tex_mtx/dol/mp4/gx_set_ind_tex_mtx_mp4_init_gx_001/gx_set_ind_tex_mtx_mp4_init_gx_001.dol --addr 0x80300000 --size 0x40 --out tests/sdk/gx/gx_set_ind_tex_mtx/expected/gx_set_ind_tex_mtx_mp4_init_gx_001.bin --run 0.5 --halt`
  - `tools/run_host_scenario.sh tests/sdk/gx/gx_set_ind_tex_mtx/host/gx_set_ind_tex_mtx_mp4_init_gx_001_scenario.c`
  - `python3 tools/ram_compare.py tests/sdk/gx/gx_set_ind_tex_mtx/expected/gx_set_ind_tex_mtx_mp4_init_gx_001.bin tests/sdk/gx/gx_set_ind_tex_mtx/actual/gx_set_ind_tex_mtx_mp4_init_gx_001.bin`
- Result: PASS (bit-exact).
- Coverage/todo snapshot updates:
  - `todo/REMAINING_TEST_STRATEGY.md`: trace replay `~73 -> ~72`, GX gap count `19 -> 18`.
  - `todo/SDK_PORT_COVERAGE.md`: GX `103 -> 104`, total ported `~232 -> ~233`.

## 2026-02-12: GXSetTevIndWarp (+GXSetTevIndirect core) added + deterministic suite

- Added sdk_port functions in `src/sdk_port/gx/GX.c`:
  - `GXSetTevIndirect(...)` packed BP register writer (decomp field layout from `GXBump.c`).
  - `GXSetTevIndWarp(...)` wrapper that maps:
    - `signed_offset` -> `bias_sel` (`NONE`/`STU`)
    - `replace_mode` -> `wrap` (`OFF`/`0`)
    - forwards to `GXSetTevIndirect`.
- Added deterministic suite:
  - `tests/sdk/gx/gx_set_tev_ind_warp/dol/mp4/gx_set_tev_ind_warp_mp4_init_gx_001/`
  - `tests/sdk/gx/gx_set_tev_ind_warp/host/gx_set_tev_ind_warp_mp4_init_gx_001_scenario.c`
- Validation commands:
  - `cd tests/sdk/gx/gx_set_tev_ind_warp/dol/mp4/gx_set_tev_ind_warp_mp4_init_gx_001 && make clean && make`
  - `python3 tools/ram_dump.py --exec tests/sdk/gx/gx_set_tev_ind_warp/dol/mp4/gx_set_tev_ind_warp_mp4_init_gx_001/gx_set_tev_ind_warp_mp4_init_gx_001.dol --addr 0x80300000 --size 0x40 --out tests/sdk/gx/gx_set_tev_ind_warp/expected/gx_set_tev_ind_warp_mp4_init_gx_001.bin --run 0.5 --halt`
  - `tools/run_host_scenario.sh tests/sdk/gx/gx_set_tev_ind_warp/host/gx_set_tev_ind_warp_mp4_init_gx_001_scenario.c`
  - `python3 tools/ram_compare.py tests/sdk/gx/gx_set_tev_ind_warp/expected/gx_set_tev_ind_warp_mp4_init_gx_001.bin tests/sdk/gx/gx_set_tev_ind_warp/actual/gx_set_tev_ind_warp_mp4_init_gx_001.bin`
- Result: PASS (bit-exact).
- Coverage/todo snapshot updates:
  - `todo/REMAINING_TEST_STRATEGY.md`: trace replay `~72 -> ~71`, GX gap count `18 -> 17`.
  - `todo/SDK_PORT_COVERAGE.md`: GX `104 -> 105`, total ported `~233 -> ~234`.

## 2026-02-12: Merge `origin/claude-win11/remaining-trace-tests` into `codex/integration-all`

- Merge required manual conflict resolution in:
  - `src/sdk_port/gx/GX.c`
  - `src/sdk_port/mtx/mtx.c`
  - `src/sdk_port/sdk_state.h`
  - `tests/sdk/vi/vi_set_pre_retrace_callback/README.md`
  - `todo/SDK_PORT_COVERAGE.md`
- Resolutions applied:
  - Removed duplicate `GC_SDK_OFF_VI_PRE_CB_*` enum entries in `sdk_state.h`.
  - Kept a single implementation set for indirect GX APIs (`GXSetNumIndStages`, `GXSetIndTexCoordScale`, `GXSetIndTexOrder`, `GXSetTevDirect`, `GXSetTevIndirect`, `GXSetTevIndWarp`, `GXSetIndTexMtx`) to avoid duplicate symbol/type conflicts.
  - Kept one `GXPixModeSync` definition and one `GXGetProjectionv` implementation.
  - Kept `ROMtx` layout as `4x3` (`src/sdk_port/mtx/sdk_port_mtx_types.h`) and removed conflicting duplicate `PSMTX*` block.
- Validation run after conflict fixes:
  - `tools/run_host_scenario.sh tests/sdk/gx/gx_texcoordgen/host/gx_texcoordgen_generic_001_scenario.c` ✅
  - `tools/run_host_scenario.sh tests/sdk/dvd/dvd_cancel/host/dvd_cancel_generic_001_scenario.c` ✅
  - `tools/run_ar_property_test.sh --num-runs=200 --seed=0x1234` ✅ (`35379/35379 PASS`)
  - `tools/run_card_dir_property_test.sh --num-runs=200 --seed=0x1234` ✅ (`7800/7800 PASS`)

## 2026-02-12: Validation pass for suites merged from `origin/claude-win11/remaining-trace-tests`

Scope: new host/property suites introduced between `f6c4196..b0555c4` on `codex/integration-all`.

Executed (all PASS):
- `tools/run_host_scenario.sh tests/sdk/ai/ai_generic/host/ai_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/ar/ar_hw/host/ar_hw_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/dvd/dvd_cancel/host/dvd_cancel_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/gx/gx_light_init/host/gx_light_init_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/gx/gx_misc/host/gx_misc_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/gx/gx_set_tev_indirect/host/gx_set_tev_indirect_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/gx/gx_set_tev_kcolor/host/gx_set_tev_kcolor_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/gx/gx_set_tev_swap_mode/host/gx_set_tev_swap_mode_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/gx/gx_tex_ci_tlut/host/gx_tex_ci_tlut_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/gx/gx_tex_misc/host/gx_tex_misc_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/gx/gx_texcoordgen/host/gx_texcoordgen_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/gx/gx_vert_format/host/gx_vert_format_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/os/os_stopwatch/host/os_stopwatch_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/os/os_sysinfo/host/os_sysinfo_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/os/os_time/host/os_time_generic_001_scenario.c`
- `tools/run_host_scenario.sh tests/sdk/vi/vi_set_pre_retrace_callback/host/vi_set_pre_retrace_callback_generic_001_scenario.c`
- `bash tools/run_mtx_batch_property_test.sh --num-runs=200 --seed=0x1234` (`ALL PASS`)

Observed gap (FAIL in compare gate):
- `ai_generic_001.bin` expected size `128`, host actual size `64`.
- `os_stopwatch_generic_001.bin` expected size `288`, host actual size `64`.
- `os_sysinfo_generic_001.bin` expected size `96`, host actual size `64`.
- `os_time_generic_001.bin` expected size `384`, host actual size `64`.

Root cause: `tools/run_host_scenario.sh` currently always dumps only `0x40` bytes (`0x80300000..0x80300040`), while these new suites write larger buffers.

## 2026-02-12: Fix host runner truncation for remaining-trace-tests generic suites

- Updated `tools/run_host_scenario.sh` to auto-size `GC_HOST_MAIN_DUMP_SIZE` from expected fixture length when available.
  - Extracts scenario out path via `gc_scenario_out_path()`.
  - Resolves expected fixture path using:
    1) `actual -> expected` sibling mapping
    2) fallback search under suite root (`*/expected/<basename>.bin`).
- Updated harness docs comment in `tests/harness/gc_host_scenario.h` to reflect non-fixed dump sizing.

Validation:
- Re-ran all 16 host scenarios added in `f6c4196..b0555c4` via `tools/run_host_scenario.sh` (all build/run pass).
- Re-checked previous size-mismatch suites with `tools/ram_compare.py`:
  - `tests/sdk/ai/ai_generic/*` expected vs actual: PASS (128 bytes)
  - `tests/sdk/os/os_stopwatch/*` expected vs actual: PASS (288 bytes)
  - `tests/sdk/os/os_sysinfo/*` expected vs actual: PASS (96 bytes)
  - `tests/sdk/os/os_time/*` expected vs actual: PASS (384 bytes)

Outcome: compare-gate blocker caused by fixed 0x40 host dumps is resolved for these suites.

## 2026-02-12: Planned trace-guided constrained-random workflow

- Added explicit plan items to `todo/REMAINING_TEST_STRATEGY.md` for a new trace-guided constrained-random pipeline (DOL-PBT hybrid).
- Added new directory boundary doc: `tests/trace-guided/README.md`.
- Directory separation now documented as:
  - raw captures: `tests/trace-harvest/`
  - derived models/cases: `tests/trace-guided/`
  - executable suites: `tests/sdk/<module>/<function>/...`
- Validation: docs-only update (`git diff --check` clean).

## 2026-02-12: Trace-guided constrained-random pilot implemented (PADControlMotor)

- Added first trace-guided pilot assets:
  - `tests/trace-guided/pad_control_motor/model.json`
  - `tests/trace-guided/pad_control_motor/README.md`
  - `tools/generate_trace_guided_pad_control_motor_cases.py`
  - `tools/replay_trace_guided_pad_control_motor.sh`
- Generator behavior:
  - deterministic by seed
  - trace-biased + boundary-valid + exploratory-valid mix
  - outputs JSONL case batches under `tests/trace-guided/pad_control_motor/cases/`
- Validation run:
  - `tools/replay_trace_guided_pad_control_motor.sh --seed 0xC0DE1234 --count 64`
  - Result: `PASS: PADControlMotor trace-guided batch (64 cases, seed=0xC0DE1234)`

## 2026-02-12: PADControlMotor DOL-side trace-guided batch + Dolphin oracle

- Added shared deterministic generator for trace-guided PADControlMotor batches:
  - `tests/trace-guided/pad_control_motor/pad_control_motor_trace_guided_gen.h`
- Added DOL-side random batch suite (fixed seed/count for byte-identical oracle):
  - `tests/sdk/pad/pad_control_motor/dol/generic/trace_guided_batch_001/pad_control_motor_trace_guided_batch_001.c`
  - `tests/sdk/pad/pad_control_motor/dol/generic/trace_guided_batch_001/Makefile`
- Added matching host batch scenario:
  - `tests/sdk/pad/pad_control_motor/host/pad_control_motor_trace_guided_batch_001_scenario.c`
- Extended runner:
  - `tools/replay_trace_guided_pad_control_motor.sh --oracle synthetic|dolphin`
  - `--oracle dolphin` currently fixed to `seed=0xC0DE1234`, `count=64`.
- Validation:
  - `tools/replay_trace_guided_pad_control_motor.sh --seed 0xC0DE1234 --count 64 --oracle synthetic` → PASS
  - `tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --seed 0xC0DE1234 --count 64` → PASS
- Mutation gate:
  - `tools/run_mutation_check.sh tools/mutations/pad_control_motor_zero_cmd.patch -- tools/replay_trace_guided_pad_control_motor.sh --seed 0xC0DE1234 --count 64 --oracle synthetic`
  - Result: PASS (mutant fails suite as expected; non-trivial assertion path confirmed).

## 2026-02-12: PADControlMotor max-coverage Dolphin golden expansion

- Added two more Dolphin-oracle suites for broader coverage:
  - `tests/sdk/pad/pad_control_motor/dol/generic/exhaustive_matrix_001/`
    - exhaustive matrix over channels `{-2,-1,0,1,2,3,4,5}` and commands `{0,1,2,3,0xffffffff}`.
  - `tests/sdk/pad/pad_control_motor/dol/generic/trace_guided_batch_002/`
    - deterministic trace-guided stress batch, seed `0xC0DE1234`, count `2048`.
- Added host mirrors:
  - `tests/sdk/pad/pad_control_motor/host/pad_control_motor_exhaustive_matrix_001_scenario.c`
  - `tests/sdk/pad/pad_control_motor/host/pad_control_motor_trace_guided_batch_002_scenario.c`
- Runner now supports explicit suite selection and full run:
  - `tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite exhaustive_matrix_001`
  - `tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite trace_guided_batch_002 --seed 0xC0DE1234 --count 2048`
  - `tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite max`
- Validation:
  - `tools/replay_trace_guided_pad_control_motor.sh --seed 0xC0DE1234 --count 128 --oracle synthetic` → PASS
  - `tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite exhaustive_matrix_001` → PASS
  - `tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite trace_guided_batch_002 --seed 0xC0DE1234 --count 2048` → PASS
- Mutation gate:
  - `tools/run_mutation_check.sh tools/mutations/pad_control_motor_zero_cmd.patch -- tools/replay_trace_guided_pad_control_motor.sh --seed 0xC0DE1234 --count 128 --oracle synthetic`
  - Result: PASS (mutant fails suite as expected).

## 2026-02-12: PADControlMotor unified DOL PBT suite (L0-L5)

- Added a single Dolphin-oracle PBT suite with six levels:
  - L0 isolated matrix (`chan -2..5` × `cmd {0,1,2,3,0xffffffff}`)
  - L1 accumulation (two valid calls without reset)
  - L2 overwrite (same channel called twice)
  - L3 random-start state (seeded xorshift32, mixed valid/invalid channels)
  - L4 harvest replay (`(chan=0, cmd=2)` sequence from harvested traces)
  - L5 boundary/no-op invariants
- New files:
  - `tests/sdk/pad/pad_control_motor/dol/pbt/pad_control_motor_pbt_001/Makefile`
  - `tests/sdk/pad/pad_control_motor/dol/pbt/pad_control_motor_pbt_001/pad_control_motor_pbt_001.c`
  - `tests/sdk/pad/pad_control_motor/host/pad_control_motor_pbt_001_scenario.c`
- Runner integration:
  - `tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite pbt_001`
  - `--dolphin-suite max` now includes `pbt_001`.
- Validation:
  - `tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite pbt_001` → PASS
  - `tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite max` → PASS
- Mutation gate:
  - `tools/run_mutation_check.sh tools/mutations/pad_control_motor_zero_cmd.patch -- tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite pbt_001` → expected FAIL, observed SURVIVED (both DOL and host use mutated sdk_port in this mode).
  - `tools/run_mutation_check.sh tools/mutations/pad_control_motor_zero_cmd.patch -- tools/replay_trace_guided_pad_control_motor.sh --seed 0xC0DE1234 --count 128 --oracle synthetic` → PASS (mutant fails as expected).

## 2026-02-12: beads repo-id mismatch repaired

- Fixed `bd` database mismatch for this clone by running:
  - `bd migrate --update-repo-id` (repo ID updated `536ef5b9 -> abcc26e3`)
- Verified normal operation without override env:
  - `bd ready` → lists ready tasks, no mismatch warning
  - `bd sync` → export succeeds, no mismatch warning
- Added permanent recovery procedure to `docs/codex/WORKFLOW.md`.

## 2026-02-12: PADControlMotor pbt_001 oracle decoupled from sdk_port

- Updated DOL PBT suite `pad_control_motor_pbt_001` to use a dedicated oracle implementation:
  - Added `tests/sdk/pad/pad_control_motor/dol/pbt/pad_control_motor_pbt_001/oracle_pad_control_motor.c`
  - Removed direct include dependency on `src/sdk_port/pad/PAD.c` in the DOL test TU.
- Result: Dolphin oracle path is now independent from host `sdk_port` mutation.
- Validation:
  - `tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite pbt_001` → PASS
  - `tools/run_mutation_check.sh tools/mutations/pad_control_motor_zero_cmd.patch -- tools/replay_trace_guided_pad_control_motor.sh --oracle dolphin --dolphin-suite pbt_001` → PASS (mutant fails as expected).

## 2026-02-12: PADSetSpec unified DOL PBT suite (L0-L5)

- Added unified PADSetSpec PBT suite:
  - `tests/sdk/pad/pad_set_spec/dol/pbt/pad_set_spec_pbt_001/*`
  - `tests/sdk/pad/pad_set_spec/host/pad_set_spec_pbt_001_scenario.c`
  - `tools/run_pad_set_spec_pbt.sh`
- Coverage levels in `pad_set_spec_pbt_001`:
  - L0 isolated specs (`0,1,2,3,4,5,0xffffffff`)
  - L1 accumulation (two-call sequences)
  - L2 overwrite/idempotency (same spec twice)
  - L3 random-start state (deterministic PRNG)
  - L4 harvest replay (`spec=5` sequence from RVZ traces)
  - L5 boundary transitions (`0,1,2,0xffffffff`)
- DOL oracle is decoupled from host sdk_port for meaningful mutation checks:
  - `tests/sdk/pad/pad_set_spec/dol/pbt/pad_set_spec_pbt_001/oracle_pad_set_spec.c`
- Validation:
  - `tools/run_pad_set_spec_pbt.sh` → PASS
  - `tools/run_mutation_check.sh tools/mutations/pad_set_spec_spec1_to_spec2.patch -- tools/run_pad_set_spec_pbt.sh` → PASS (mutant fails as expected).

## 2026-02-12: PADReset unified DOL PBT suite (L0-L5)

- Added unified PADReset PBT suite:
  - `tests/sdk/pad/pad_reset/dol/pbt/pad_reset_pbt_001/*`
  - `tests/sdk/pad/pad_reset/host/pad_reset_pbt_001_scenario.c`
  - `tools/run_pad_reset_pbt.sh`
- Coverage levels in `pad_reset_pbt_001`:
  - L0 isolated masks (`0, 0x80000000, 0x40000000, 0x20000000, 0x10000000, 0x70000000, 0xffffffff`)
  - L1 accumulation (two-call mask sequences)
  - L2 idempotency (same mask twice)
  - L3 random-start state (deterministic PRNG)
  - L4 harvest replay (observed retail mask `0x70000000`)
  - L5 boundary from non-empty pre-state
- DOL oracle decoupled from host sdk_port for meaningful mutation checks:
  - `tests/sdk/pad/pad_reset/dol/pbt/pad_reset_pbt_001/oracle_pad_reset.c`
- Validation:
  - `tools/run_pad_reset_pbt.sh` → PASS
  - `tools/run_mutation_check.sh tools/mutations/pad_reset_skip_or_mask.patch -- tools/run_pad_reset_pbt.sh` → PASS (mutant fails as expected).

## 2026-02-12: PADInit unified DOL PBT suite (L0-L5)

- Added unified PADInit PBT suite:
  - `tests/sdk/pad/pad_init/dol/pbt/pad_init_pbt_001/*`
  - `tests/sdk/pad/pad_init/host/pad_init_pbt_001_scenario.c`
  - `tools/run_pad_init_pbt.sh`
- Coverage levels in `pad_init_pbt_001`:
  - L0 isolated matrix (`initialized` x `fix_bits` x `spec`)
  - L1 accumulation (two-stage init sequence with spec transitions)
  - L2 idempotency (second call with initialized already set)
  - L3 random-start state (deterministic PRNG)
  - L4 harvest-style replay (`spec=5`, clean init path)
  - L5 boundary states (`initialized=1`, fix_bits extremes, spec edge values)
- DOL oracle decoupled from host sdk_port:
  - `tests/sdk/pad/pad_init/dol/pbt/pad_init_pbt_001/oracle_pad_init.c`
- Validation:
  - `tools/run_pad_init_pbt.sh` → PASS
  - `tools/run_mutation_check.sh tools/mutations/pad_init_skip_reset_internal.patch -- tools/run_pad_init_pbt.sh` → PASS (mutant fails as expected).

## 2026-02-12: PADRead unified DOL PBT suite (L0-L5)

- Added unified PADRead PBT suite:
  - `tests/sdk/pad/pad_read/dol/pbt/pad_read_pbt_001/*`
  - `tests/sdk/pad/pad_read/host/pad_read_pbt_001_scenario.c`
  - `tools/run_pad_read_pbt.sh`
- Coverage levels in `pad_read_pbt_001`:
  - L0 isolated (prefill variations, non-null status pointer)
  - L1 accumulation (double-read sequencing)
  - L2 overwrite from seeded garbage
  - L3 random-start (deterministic PRNG; null/non-null pointer mix)
  - L4 harvest-style replay (nonnull path)
  - L5 boundary/null-pointer toggles
- DOL oracle decoupled from host sdk_port:
  - `tests/sdk/pad/pad_read/dol/pbt/pad_read_pbt_001/oracle_pad_read.c`
- Validation:
  - `tools/run_pad_read_pbt.sh` → PASS
  - `tools/run_mutation_check.sh tools/mutations/pad_read_return_zero.patch -- tools/run_pad_read_pbt.sh` → PASS (mutant fails as expected).
