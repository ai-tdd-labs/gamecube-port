# OSInit (MP4 chain step 1)

Status: BLOCKED for now.

Reason:
- `decomp_mario_party_4/src/dolphin/os/OS.c` and several OS init dependencies use Metrowerks-specific extensions (`__declspec`, inline PPC asm, linker address pragmas like `extern ... : 0x...`, `@ha/@l`).
- That code does not compile cleanly with the devkitPPC GCC toolchain we use for DOL tests.

Legacy reference (not an oracle):
- `docs/_legacy/tests/dols/os_init/generic/os_init_001/`

Next step when unblocking:
- Either add a compatible build path for MW-flavored SDK sources, or replace `OSInit` with a black-box runtime capture approach (Dolphin execution of a real DOL/ELF that calls OSInit) that does not require recompiling `OS.c`.
