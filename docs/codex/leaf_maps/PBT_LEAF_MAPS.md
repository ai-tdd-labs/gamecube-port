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

## Core strict PBT suites
- `tests/pbt/os/os_round_32b`: strict leaves (`OSRoundUp32B`, `OSRoundDown32B`)
- `tests/pbt/mtx/mtx_core_pbt.c`: strict leaves (`C_MTXIdentity`, `C_MTXOrtho`)
- `tests/pbt/dvd/dvd_core_pbt.c`: strict `dvd_read_window` semantics
