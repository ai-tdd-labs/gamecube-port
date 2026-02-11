# ORACLE DELTA — CARD FAT

Oracle file: `tests/sdk/card/property/card_fat_oracle.h`
Decomp sources:
- `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/card/CARDBlock.c`
- `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/card/CARDCheck.c`
Tier: `DECOMP_ADAPTED`

## Exactly matched behavior
- `__CARDCheckSum` algorithm.
- `__CARDAllocBlock` free-list traversal and allocation chain writes.
- `__CARDFreeBlock` chain walk and free-block count updates.

## Intentional deltas from decomp
- `__CARDUpdateFatBlock` strips I/O path (`DCStoreRange`, `__CARDEraseSector`, `__CARDWrite`).
- Callback plumbing (`eraseCallback`, write/erase callback sequence) is removed.
- Oracle uses minimal control struct instead of full `CARDControl` + channel table.

## Coverage impact
- Strong for FAT metadata and chain math correctness.
- Not authoritative for erase/write completion ordering or callback-driven flow.

## Evidence commands
- `tools/run_card_fat_property_test.sh --num-runs=50 -v`
