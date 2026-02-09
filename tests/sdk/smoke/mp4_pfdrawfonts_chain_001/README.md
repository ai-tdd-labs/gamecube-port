# mp4_pfdrawfonts_chain_001

Checkpoint smoke chain for the MP4 `pfDrawFonts` immediate-mode helper calls
(SDK-only).

This chain intentionally runs only Nintendo SDK surface calls (no MP4 game code)
and focuses on a set of GX helpers that write to the GX FIFO on real hardware.
Since FIFO writes are not directly observable via RAM dumps, our `sdk_port`
implements a deterministic "mirror last written values" contract for these
helpers, and the smoke snapshot records those mirrors.

Chain steps (pfDrawFonts-like order):
- `GXPosition3s16(x,y,z)` + `GXColor1x8(idx)` + `GXTexCoord2f32(s,t)` (repeated)
- `GXPosition2f32(x,y)` + `GXColor3u8(r,g,b)` (WireDraw-like snippet)

Outputs:
- Dolphin expected MEM1 dump: `expected/mp4_pfdrawfonts_chain_001_mem1.bin`
- Host actual MEM1 dump: `actual/mp4_pfdrawfonts_chain_001_mem1.bin`

Assertion:
`tools/diff_bins_smoke.sh` compares only:
- marker + snapshot window around `0x80300000`
- RAM-backed sdk_port state page at the end of MEM1 (`0x817FE000..0x81800000`)

This does **not** prove retail MP4 correctness by itself; it proves PPC-vs-host
determinism for our current `sdk_port` model.

