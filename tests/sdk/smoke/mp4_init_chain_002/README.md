# mp4_init_chain_002

Checkpoint smoke test: run an MP4 init-style SDK call chain (HuSysInit + early
InitGX/InitMem/InitVI-style calls), then dump full MEM1 (0x80000000..0x817FFFFF)
from Dolphin (expected) and from host sdk_port virtual RAM (actual).

We *assert* bit-exact equality for deterministic windows inside MEM1:
- marker @ 0x80300000
- snapshot @ 0x80300100
- sdk_state page @ 0x817FE000..0x817FFFFF (RAM-backed SDK globals)

Oracle: Dolphin MEM1 dump (PPC run of sdk_port) vs host MEM1 dump (host run of sdk_port).

Dump range:
- addr: 0x80000000
- size: 0x01800000 (24 MiB)

## Run

```bash
cd /Users/chrislamark/projects/gamecube

# expected (Dolphin)
tools/dump_expected_mem1.sh \
  tests/sdk/smoke/mp4_init_chain_002/dol/mp4/mp4_init_chain_002/mp4_init_chain_002.dol \
  tests/sdk/smoke/mp4_init_chain_002/expected/mp4_init_chain_002_mem1.bin \
  0.75

# actual (host)
tools/run_host_smoke.sh \
  tests/sdk/smoke/mp4_init_chain_002/host/mp4_init_chain_002_scenario.c \
  tests/sdk/smoke/mp4_init_chain_002/actual/mp4_init_chain_002_mem1.bin

# compare deterministic window
tools/diff_bins_smoke.sh \
  tests/sdk/smoke/mp4_init_chain_002/expected/mp4_init_chain_002_mem1.bin \
  tests/sdk/smoke/mp4_init_chain_002/actual/mp4_init_chain_002_mem1.bin
```
