# ORACLE DELTA — MTX

Oracle file (branch reference): `tests/sdk/mtx/property/mtx_oracle.h`
Branch: `remotes/origin/mtx/property-tests-claude-win11`
Decomp sources:
- `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/mtx/mtx.c`
- `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/mtx/vec.c`
- `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/mtx/quat.c`
- `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/mtx/mtx44.c`
- `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/mtx/mtxvec.c`
Tier: `DECOMP_ADAPTED`

## Exactly matched behavior
- Many pure C matrix routines and math identities.

## Intentional deltas from decomp
- GEKKO paired-single asm paths removed in host oracle.
- Some leaf VEC/QUAT behavior represented by C fallback/trivial implementations.
- Internal call graph adapted to self-contained host header layout.

## Coverage impact
- Strong for C-path matrix math invariants.
- Not authoritative for PPC asm floating-point edge semantics.

## Evidence commands
- `tools/run_mtx_property_test.sh ...` (on mtx property branch)
