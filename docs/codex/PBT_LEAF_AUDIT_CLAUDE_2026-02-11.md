# Claude PBT Leaf Audit (2026-02-11)

Scope: verify whether recently merged Claude PBT suites are structured bottom-up (leaf first, then higher levels), like the OSAlloc approach.

## Verdict (short)

- **Not uniformly complete bottom-up across all suites.**
- Some suites are clearly leaf-first and layered.
- Other suites are layered but not fully leaf-complete for their full subsystem tree.

## Per-suite audit

| Suite | Evidence | Bottom-up shape | Leaf-complete for that tree? | Notes |
|---|---|---|---|---|
| `tests/sdk/os/os_alloc/property/osalloc_property_test.c` | Explicit comments and sections for L0/L1/L2; `--op` targeting; DLL leaf checks | **Yes** | **Yes (for covered OSAlloc subtree)** | Strongest Micropolis-style structure in repo. |
| `tests/sdk/os/osthread/property/osthread_property_test.c` | Explicit L0..L9 levels with targeted checks and `--op` routing | **Yes** | **No (full OS thread/sync tree not complete)** | Large layered suite, but project status still lists remaining sync candidates outside this exact set. |
| `tests/sdk/mtx/property/mtx_property_test.c` | Declares L0/L1/L2 (VEC/MTX/QUAT/integration) and `--op` filter families | **Mostly yes** | **No (full MTX/GX math tree not fully enumerated here)** | Good layered coverage, not an exhaustive leaf tree inventory. |
| `tests/sdk/os/stopwatch/property/stopwatch_property_test.c` | L0/L1 sections only | **Yes (small tree)** | **Partial** | Good for stopwatch subtree; only two levels. |
| `tests/sdk/ar/property/arq_property_test.c` | Explicit L0/L1/L2 + strict leaf dualcheck (`strict_ARQNormalizeCallback`) | **Yes** | **Partial** | Strong layering, but only one strict leaf singled out explicitly. |
| `tests/sdk/card/property/card_fat_property_test.c` | Explicit L0/L1/L2 + strict checksum leaf oracle | **Yes** | **Mostly yes (for FAT allocator/checksum subtree)** | Clear leaf-to-integration flow for checksum/alloc/free behavior. |
| `tests/sdk/dvd/dvdfs/property/dvdfs_property_test.c` | Random/scenario invariants, no explicit L0/L1/L2 sections | **Partly** | **No** | Good parity checks, but not structured as explicit full leaf ladder. |
| `tests/pbt/os/os_round_32b/os_round_32b_pbt.c` | Strict dualcheck for 2 leaf funcs | **Leaf-only** | **Yes (for those 2 funcs)** | Focused strict leaf suite. |
| `tests/pbt/mtx/mtx_core_pbt.c` | Strict dualcheck leafs (`C_MTXIdentity`, `C_MTXOrtho`) | **Leaf-only** | **Yes (for those 2 funcs)** | Focused strict leaf suite. |
| `tests/pbt/dvd/dvd_core_pbt.c` | Strict oracle read-window semantics | **Leaf/core semantic** | **Yes (for modeled core semantics)** | Covers modeled DVD core arithmetic semantics, not all DVD internals. |

## Conclusion

If the bar is: "every Claude PBT suite fully starts from all leaves under its full subsystem tree and climbs up without gaps", then the answer is **no**.

If the bar is: "Claude added meaningful layered PBT with leaf checks in key subsystems", then the answer is **yes**.

## Concrete gaps to close for strict uniformity

1. Add explicit leaf inventory tables per suite (function-by-function leaf list).
2. For each suite, map every implemented leaf to at least one targeted `--op` run.
3. Promote suites like DVDFS to explicit L0/L1/L2 sections (not only scenario parity loops).
4. Keep mutation checks per suite to prove non-triviality at each level.

