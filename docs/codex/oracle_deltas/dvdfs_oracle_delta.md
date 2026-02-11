# ORACLE DELTA — DVDFS

Oracle file (branch reference): `tests/sdk/dvd/dvdfs/property/dvdfs_oracle.h`
Branch: `remotes/origin/codex/macos-dvdfs-property`
Decomp source: `/Users/chrislamark/projects/recomp/gamecube_static_recomp/decomp_mario_party_4/src/dolphin/dvd/dvdfs.c`
Tier: `DECOMP_ADAPTED`

## Exactly matched behavior
- Core FST traversal shape for path->entry and entry->path conversion.
- Directory stepping (`.`, `..`, absolute/relative handling) and lookup flow.

## Intentional deltas from decomp
- Host buffer mapping replaces `OSPhysicalToCached` environment.
- Boot/FST structures are minimal mirrors for host deterministic harnessing.
- Illegal-path handling is hardened to deterministic host return behavior.

## Coverage impact
- Strong for parser/traversal logic on harvested fixtures.
- Not a full substitute for retail memory map and OS boot-state behavior.

## Evidence commands
- `tools/run_property_dvdfs.sh ...` (on dvdfs property branch)
