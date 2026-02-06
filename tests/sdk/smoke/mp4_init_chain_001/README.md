# mp4_init_chain_001

Checkpoint smoke test: run the first 20 MP4 HuSysInit SDK calls in a fixed order,
then dump full MEM1 (0x80000000..0x817FFFFF) from Dolphin (expected) and from
host sdk_port virtual RAM (actual).

We *assert* bit-exact equality for a small deterministic window inside MEM1:
- marker @ 0x80300000
- snapshot @ 0x80300100

Full MEM1 dumps are still produced (useful for debugging), but are not yet a
meaningful oracle for host-vs-PPC because sdk_port state still lives in globals.

Oracle: Dolphin MEM1 dump.

Dump range:
- addr: 0x80000000
- size: 0x01800000 (24 MiB)

## Run

```bash
cd /Users/chrislamark/projects/gamecube

# expected (Dolphin)
tools/dump_expected_mem1.sh \
  tests/sdk/smoke/mp4_init_chain_001/dol/mp4/mp4_init_chain_001/mp4_init_chain_001.dol \
  tests/sdk/smoke/mp4_init_chain_001/expected/mp4_init_chain_001_mem1.bin \
  0.75

# actual (host)
tools/run_host_smoke.sh \
  tests/sdk/smoke/mp4_init_chain_001/host/mp4_init_chain_001_scenario.c \
  tests/sdk/smoke/mp4_init_chain_001/actual/mp4_init_chain_001_mem1.bin

# compare deterministic window
tools/diff_bins_smoke.sh \
  tests/sdk/smoke/mp4_init_chain_001/expected/mp4_init_chain_001_mem1.bin \
  tests/sdk/smoke/mp4_init_chain_001/actual/mp4_init_chain_001_mem1.bin
```
