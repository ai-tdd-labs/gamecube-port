# PBT Leaf Maps (OSAlloc-style)

This file maps leaves to levels for currently active suites.

## OSAlloc property
- `L0` leaves: `DLInsert`, `DLExtract`, `DLAddFront`, `DLAddEnd`, `DLOverlap`, `DLLookup`
- `L1` API: `OSAllocFromHeap`, `OSFreeToHeap`, `OSCheckHeap`
- `L2` integration: randomized mixed alloc/free/check sequences

## OSThread/Mutex/Message property
- `L0-L3` thread lifecycle/scheduling leaves and mixes
- `L4-L6` mutex + prio inheritance + join semantics
- `L7-L9` message queue leaves and threaded queue interactions
- `L10-L11` cond wait/signal + mutex invariants

## OSTime property
- `L0` leaf: random ticks conversion parity (`OSTicksToCalendarTime`)
- `L1` leaf edges: epoch/negative/leap/year-boundary cases
- `L2` integration: monotonic conversion ordering over sequences

## OSStopwatch property
- `L0` leaf/API mix: single stopwatch op stream (`Init/Start/Stop/Check/Reset`)
- `L1` integration: interleaved multi-stopwatch stream with shared clock

## MTX property
- `L0` leaves: VEC helpers (`Add/Sub/Dot/Cross/Normalize/...`)
- `L1` API: MTX/QUAT function parity checks
- `L2` integration: chained math paths (`LookAt`, `RotAxisRad`, `QUATSlerp`, etc.)

## ARQ property
- `L0` leaf/API: single request DMA/callback normalization
- `L1` ordering: Hi-vs-Lo priority sequencing
- `L2` integration: randomized queue/chunking runs
- strict leaf dualcheck: callback normalization helper

## CARD FAT property
- `L0` leaf: checksum behavior
- `L1` API leaves: alloc/free block transitions
- `L2` integration: randomized alloc+free sequences
- strict leaf dualcheck: checksum strict oracle

## DVDFS property
- `L0` canonical sweep: entry↔path invariants
- `L1` leaf/API: path→entry resolution
- `L2` leaf/API: entry→path/chdir behavior
- `L3` leaf/API: chdir path handling
- `L4` leaf/API: illegal 8.3 path rejection parity

## dvdqueue property
- `L0` push/pop parity
- `L1` dequeue parity
- `L2` membership parity (`IsBlockInWaitingQueue`)
- `L3` queue ordering properties (priority + FIFO within priority)
- `L4` randomized integration mix

## GXTexture property
- `L0` oracle/port parity for `GXGetTexBufferSize`
- `L1` non-mipmap tile math consistency
- `L2` mipmap monotonicity
- `L3` RGBA8 double-tile-size behavior
- `L4` `GetImageTileCount` parity + cross-check

## OSAlarm property
- `L0` insert parity + sorted order
- `L1` cancel parity + cancel idempotence
- `L2` fire-head parity
- `L3` periodic alarm reinsertion behavior
- `L4` randomized integration mix

## PADClamp property
- `L0` ClampStick parity
- `L1` ClampTrigger parity
- `L2` ClampStick properties (idempotence/bounds/symmetry/dead zone)
- `L3` full `PADClamp` parity

## GXProject property
- `L0` projection parity (oracle vs port)
- `L1` identity modelview mapping
- `L2` orthographic linearity
- `L3` perspective depth behavior
- `L4` viewport center mapping
- `L5` randomized integration mix

## GXCompressZ16 / GXDecompressZ16 property
- `L0` compress/decompress parity
- `L1` linear round-trip invariant
- `L2` idempotence invariant
- `L3` bit-range invariants
- `L4` exhaustive linear-space check
- `L5` randomized integration mix

## GXGetYScaleFactor property
- `L0` `__GXGetNumXfbLines` parity
- `L1` `GXGetYScaleFactor` parity
- `L2` result-correctness invariant
- `L3` boundedness invariant
- `L4` identity-scale invariant
- `L5` randomized integration mix

## THPAudioDecode property
- `L0` mono/interleaved parity
- `L1` stereo/separate parity
- `L2` mono channels-equal invariant
- `L3` saturation/range invariant
- `L4` randomized integration mix

## Core strict PBT suites
- `tests/pbt/os/os_round_32b`: strict leaves (`OSRoundUp32B`, `OSRoundDown32B`)
- `tests/pbt/mtx/mtx_core_pbt.c`: strict leaves (`C_MTXIdentity`, `C_MTXOrtho`)
- `tests/pbt/dvd/dvd_core_pbt.c`: strict `dvd_read_window` semantics
