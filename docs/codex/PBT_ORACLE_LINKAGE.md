# PBT Oracle Linkage

This file maps each PBT subsystem to its oracle source and replay command.

## OSAlloc

- Oracle type: decomp-faithful behavior + deterministic PPC/host snapshots.
- Evidence:
  - `tests/sdk/os/os_alloc/`
  - `tests/sdk/os/os_alloc_from_heap/`
  - `tests/sdk/smoke/mp4_init_chain_001/`
- Commands:
  - `tools/run_pbt.sh os_round_32b 20000 0xC0DEC0DE`
  - `tools/diff_bins.sh tests/sdk/os/os_alloc/expected/os_alloc_mp4_default_fifo_001.bin tests/sdk/os/os_alloc/actual/os_alloc_mp4_default_fifo_001.bin`

## DVDFS

- Oracle type: virtual-disc deterministic oracle (`gc_dvd_test_*`) + snapshot suites.
- Evidence:
  - `tests/sdk/dvd/dvd_fast_open/`
  - `tests/sdk/dvd/dvd_read_async/`
  - `tests/sdk/dvd/dvd_convert_path_to_entrynum/`
- Commands:
  - `tools/run_pbt.sh dvd_core 20000 0xC0DEC0DE`

## MTX

- Oracle type: decomp math invariants + snapshot suites.
- Evidence:
  - `tests/sdk/mtx/mtx_identity/`
  - `tests/sdk/mtx/mtx_ortho/`
- Commands:
  - `tools/run_pbt.sh mtx_core 50000 0xC0DEC0DE`

## ARQ

- Oracle type: embedded decomp oracle (`tests/sdk/ar/property/arq_oracle.h`) vs sdk_port.
- Oracle tier: `DECOMP_ADAPTED`
- Evidence:
  - `tests/sdk/ar/property/arq_property_test.c`
- Commands:
  - `tools/run_arq_property_test.sh --num-runs=50 -v`

## CARD-FAT

- Oracle type: embedded decomp oracle (`tests/sdk/card/property/card_fat_oracle.h`) vs sdk_port.
- Oracle tier: `DECOMP_ADAPTED`
- Evidence:
  - `tests/sdk/card/property/card_fat_property_test.c`
- Commands:
  - `tools/run_card_fat_property_test.sh --num-runs=50 -v`
