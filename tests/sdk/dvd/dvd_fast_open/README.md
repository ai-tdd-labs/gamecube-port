# DVD DVDFastOpen tests

These testcases are deterministic and MP4-callsite-driven.

They run in two environments:
1) DOL in Dolphin (expected.bin)
2) Host scenario against `src/sdk_port` (actual.bin)

We compare dumps for bit-exact equality.

