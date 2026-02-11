# PBT OSAlloc-Style Standard Matrix

This matrix enforces one uniform standard across active suites:

1) explicit levels (`L0..`)  
2) targeted run selector (`--op=...`)  
3) leaf-to-level mapping documented  
4) deterministic rerun (`--seed`)  
5) mutation/replay pairing where relevant

## Checklist

| Rule | Meaning |
|---|---|
| `levels` | Suite has explicit L0/L1/... phases in code/comments. |
| `op-targeting` | Runner supports focused execution (`--op=...`). |
| `leaf-map` | Leaf functions/semantics mapped to levels in docs. |
| `deterministic` | Fixed seed reproduces failures. |
| `quality-gate` | Mutation and/or replay gate exists for the subsystem risk. |

## Active Suite Status

| Suite | levels | op-targeting | leaf-map | deterministic | quality-gate | Notes |
|---|---:|---:|---:|---:|---:|---|
| OSAlloc property | yes | yes | yes | yes | mutation | Baseline/reference style. |
| OSThread+Mutex+Message property | yes | yes | partial | yes | mutation | L0-L11 explicit. |
| OSTime property | yes | yes | partial | yes | mutation | L0-L2 explicit. |
| OSStopwatch property | yes | yes | partial | yes | mutation | Added `--op` targeting (L0/L1). |
| MTX property | yes | yes | partial | yes | mutation | Level families present (VEC/MTX/QUAT). |
| ARQ property | yes | yes | partial | yes | mutation + strict leaf | L0-L2 + strict callback leaf check. |
| CARD FAT property | yes | yes | partial | yes | mutation + strict leaf | L0-L2 + checksum strict leaf. |
| DVDFS property | yes | yes | partial | yes | mutation | Added `--op=L0..L4|MIX`. |
| MTX core PBT | leaf/core | n/a | yes | yes | mutation | Focused strict leaf suite. |
| DVD core PBT | leaf/core | n/a | yes | yes | mutation | Focused strict semantic suite. |
| OS round PBT | leaf/core | n/a | yes | yes | mutation | Strict dualcheck leafs. |

## Next hardening steps

1. For each `partial` leaf-map row, add explicit markdown map under `docs/codex/leaf_maps/`.
2. Add one standardized command block per suite in docs:
   - full run
   - per-level run
   - deterministic repro run
3. Keep replay gate (`tools/run_replay_gate.sh`) paired for hardware-sensitive paths.
