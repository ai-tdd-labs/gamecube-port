# mp4_hudata_read_chain_001

Checkpoint smoke chain for the MP4 `HuData*` DVD read subset (SDK-only).

This chain intentionally runs only Nintendo SDK surface calls (no MP4 game code):
- `DVDFastOpen()`
- `OSRoundUp32B()`
- `DCInvalidateRange()`
- `DVDReadAsync()`
- `DVDGetCommandBlockStatus()`
- `DVDClose()`
- `OSRoundDown32B()`

Outputs:
- Dolphin expected MEM1 dump: `expected/mp4_hudata_read_chain_001_mem1.bin`
- Host actual MEM1 dump: `actual/mp4_hudata_read_chain_001_mem1.bin`

Assertion:
`tools/diff_bins_smoke.sh` compares only:
- marker + snapshot window around `0x80300000`
- RAM-backed sdk_port state page at the end of MEM1 (`0x817FE000..0x81800000`)

This does **not** prove retail MP4 correctness by itself; it proves PPC-vs-host
determinism for our current sdk_port model.
