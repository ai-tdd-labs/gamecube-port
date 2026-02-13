# VISetBlack

Unified L0-L5 DOL-PBT suite:
- `dol/pbt/vi_set_black_pbt_001/`
- `host/vi_set_black_pbt_001_scenario.c`

Levels:
- L0 isolated inputs (`0,1,2,0xffffffff`)
- L1 accumulation transitions (2-call matrix)
- L2 idempotency (same value twice)
- L3 random-start seeded states
- L4 callsite-derived value replay set from MP4 decomp (`0,1,1,0`)
- L5 boundary behavior (`set_black_calls` near wrap + bool normalization)
