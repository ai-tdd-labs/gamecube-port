# dvdfs property parity (DVD path/FST)

Host-only semantic parity tests for the DVD filesystem path logic in `dvdfs.c`.

Compares:
- **oracle**: derived from MP4 decomp `external/mp4-decomp/src/dolphin/dvd/dvdfs.c` (adapted to avoid `OSPanic`)
- **port**: independent reimplementation of `DVDConvertPathToEntrynum`, `DVDChangeDir`, and directory `entrynum -> path`

This locks down tricky string + FST traversal logic quickly. It does not replace the
Dolphin expected.bin workflow.

## Run

```bash
tools/run_property_dvdfs.sh

# more runs
tools/run_property_dvdfs.sh --num-runs=2000 --steps=200 -v

# reproduce
tools/run_property_dvdfs.sh --seed=0xDEADBEEF --steps=500 -v

# Focused level/operation runs (OSAlloc-style targeting)
tools/run_property_dvdfs.sh --op=L0 --seed=0xDEADBEEF -v          # canonical sweep
tools/run_property_dvdfs.sh --op=L1 --seed=0xDEADBEEF --steps=300 # path->entry
tools/run_property_dvdfs.sh --op=L2 --seed=0xDEADBEEF --steps=300 # entry->path
tools/run_property_dvdfs.sh --op=L3 --seed=0xDEADBEEF --steps=300 # chdir
tools/run_property_dvdfs.sh --op=L4 --seed=0xDEADBEEF --steps=300 # illegal 8.3
```

## What it tests

- Canonical absolute paths resolve to the correct FST entry.
- `DVDChangeDir()` parity for directory targets.
- Illegal 8.3 segments fail when `__DVDLongFileNameFlag == 0` (oracle returns `-1` in this harness).
- Directory `entry -> path` matches between oracle and port (including trailing `/` for dirs).

## Notes

- This harness builds a synthetic, valid FST tree in a memory buffer and points `OSBootInfo.FSTLocation`
  at it.
- We treat `OSPhysicalToCached(0)` as the start of that buffer.
- This is a *logic* parity harness; it does not try to model disc byte endianness.
