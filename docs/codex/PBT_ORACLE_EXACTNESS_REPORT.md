# PBT Oracle Exactness Report

Date: 2026-02-11
Branch checked: `main`

This report answers one question:
Are we PBT-testing exact MP4 decomp code, or host-adapted approximations?

## Scope checked

- `tests/sdk/ar/property/arq_oracle.h`
- `tests/sdk/card/property/card_fat_oracle.h`
- `remotes/origin/osalloc/property-tests-v2:tests/sdk/os/os_alloc/property/osalloc_oracle.h`
- `remotes/origin/codex/macos-dvdfs-property:tests/sdk/dvd/dvdfs/property/dvdfs_oracle.h`
- `remotes/origin/mtx/property-tests-claude-win11:tests/sdk/mtx/property/mtx_oracle.h`

Against decomp sources:

- `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/ar/arq.c`
- `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/card/CARDBlock.c`
- `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/card/CARDCheck.c`
- `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/os/OSAlloc.c`
- `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/dvd/dvdfs.c`
- `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/mtx/*.c`

## Verdict summary

- ARQ: **Adapted oracle** (core logic copied, hardware/interrupt/callback behavior mocked)
- CARD FAT: **Adapted oracle** (alloc/free/checksum logic copied, FAT update I/O path stripped)
- OSAlloc: **Adapted oracle** (layout/macros adjusted, missing decomp field/assignment fixed for host)
- DVDFS: **Adapted oracle** (host memory mapping and behavior guards added)
- MTX: **Adapted oracle** (GEKKO asm paths removed, C fallback/trivial leaf implementations used)

Current status: **functional parity focused**, not strict byte-identical decomp execution.

## Details per oracle

### ARQ (`tests/sdk/ar/property/arq_oracle.h`)

What matches:
- Queue and DMA scheduling flow mirrors decomp: `__ARQPopTaskQueueHi`, `__ARQServiceQueueLo`, ISR flow, `ARQPostRequest` queue linking.

What is adapted:
- Real callbacks replaced by `has_callback`/counter logic.
- `OSDisableInterrupts/OSRestoreInterrupts` behavior removed.
- `ARStartDMA` is a log sink, not hardware call.

Risk:
- Concurrency/interrupt semantics can diverge while core queue math still passes.

### CARD FAT (`tests/sdk/card/property/card_fat_oracle.h`)

What matches:
- `__CARDAllocBlock`, `__CARDFreeBlock`, `__CARDCheckSum` core algorithms.

What is adapted:
- `__CARDUpdateFatBlock` does checksum+counter only; erase/write callback pipeline removed.
- Control block/channel callback interactions removed.

Risk:
- I/O completion and callback timing regressions are not covered.

### OSAlloc (`remotes/origin/osalloc/property-tests-v2`)

What matches:
- Main heap algorithms and list operations are decomp-derived.

What is adapted:
- Extra `hd` in `Cell`, explicit `cell->hd` assignments, assert/report macro stubs.
- Host assumptions documented (`-m32`) to emulate PPC layout.

Risk:
- Host ABI and patched declarations can hide strict decomp/source mismatches.

### DVDFS (`remotes/origin/codex/macos-dvdfs-property`)

What matches:
- Path parsing and FST traversal structure follows decomp.

What is adapted:
- `OSPhysicalToCached` replaced with host buffer mapping.
- Illegal-path handling explicitly converted to deterministic host return behavior.
- Minimal struct mirrors used.

Risk:
- Exact OS/FST memory environment coupling may differ from retail.

### MTX (`remotes/origin/mtx/property-tests-claude-win11`)

What matches:
- Many C matrix routines are decomp-derived.

What is adapted:
- GEKKO/PS asm paths removed.
- Some vector/quaternion leaves implemented as trivial C behavior.

Risk:
- PPC-specific floating-point and paired-single edge behavior not fully represented.

## What this means for confidence

- Good confidence for algorithmic behavior.
- Medium confidence for interrupt/callback/hardware ordering.
- Lower confidence for strict PPC-asm floating-point parity where asm paths were replaced.

## Hardening plan (required for "true decomp-oracle" claim)

1. Introduce strict tier labels in each oracle:
   - `TIER_A_STRICT_DECOMP`
   - `TIER_B_DECOMP_ADAPTED`
   - `TIER_C_MODEL_OR_SYNTHETIC`
2. For each adapted oracle, add a "delta ledger" listing every intentional deviation.
3. Add dual-run mode where possible:
   - strict extracted decomp body (minimal wrapper)
   - adapted oracle body
   - fail if outputs diverge on shared fixture set.
4. Raise coverage for hardware-sensitive functions with retail trace replay fixtures.
5. For MTX/VEC/QUAT, split tests into:
   - C-path parity
   - asm-sensitive parity (retail trace based).

## Branch integration note

`main` and `codex/integration-all` are currently divergent:

- `main`: `7f49ee8cc1c44e7a741998da5f1983187146fe95`
- `codex/integration-all`: `8dbe7b3b52a9c01d05b7df4671383738e6afac14`

So the "all-branches combined" state is **not fully on `main`**.
