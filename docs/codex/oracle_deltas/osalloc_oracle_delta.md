# ORACLE DELTA — OSAlloc

Oracle file (branch reference): `tests/sdk/os/os_alloc/property/osalloc_oracle.h`
Branch: `remotes/origin/osalloc/property-tests-v2`
Decomp source: `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/os/OSAlloc.c`
Tier: `DECOMP_ADAPTED`

## Exactly matched behavior
- Heap descriptor/list algorithms (`DL*` helpers).
- `OSInitAlloc`, `OSCreateHeap`, `OSAllocFromHeap`, `OSFreeToHeap` core logic.
- Arena bounds/size arithmetic and alignment flow.

## Intentional deltas from decomp
- Host-facing type/layout adjustments and macro stubs.
- Added `hd`-related compatibility handling described in oracle header comments.
- Build assumptions for host ABI parity are explicit (`-m32` in branch workflow).

## Coverage impact
- Strong for allocator algorithm invariants.
- Needs strict extraction mode to claim byte-identical decomp execution.

## Evidence commands
- `tools/run_property_test.sh osalloc --num-runs=...` (on property branch)
