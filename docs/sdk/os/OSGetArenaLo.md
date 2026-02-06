# OSGetArenaLo

## Signature
`void *OSGetArenaLo(void)`

## What we know (from decomp)
- Returns the current global arena low pointer (typically `__OSArenaLo`).
- Some SDK variants assert that OSInit ran and that `lo <= hi` (seen in TP).

Evidence:
- MP4: `decomp_mario_party_4/src/dolphin/os/OSArena.c`
- TP (dolphin): `decomp_twilight_princess/src/dolphin/os/OSArena.c`
- TP (revolution): `decomp_twilight_princess/src/revolution/os/OSArena.c`

## Callsites (evidence)

### MP4 (Mario Party 4)
- `decomp_mario_party_4/src/game/init.c`
- `decomp_mario_party_4/src/dolphin/demo/DEMOInit.c`
- `decomp_mario_party_4/src/dolphin/os/OS.c` (memset arena)
- `decomp_mario_party_4/src/Runtime.PPCEABI.H/GCN_Mem_Alloc.c`

### TP (Twilight Princess)
- `decomp_twilight_princess/src/dolphin/os/OS.c` (memset arena)
- `decomp_twilight_princess/src/dolphin/os/OSFatal.c` (font load to arena)
- `decomp_twilight_princess/src/dolphin/os/OSExec.c` (alloc from arena)
- `decomp_twilight_princess/src/m_Do/m_Do_machine.cpp`

### WW (Wind Waker)
- `decomp_wind_waker/src/dolphin/os/OS.c` (memset arena)
- `decomp_wind_waker/src/JSystem/JKernel/JKRHeap.cpp`
- `decomp_wind_waker/src/m_Do/m_Do_machine.cpp`
- `decomp_wind_waker/src/PowerPC_EABI_Support/Runtime/Src/GCN_mem_alloc.c`

### AC (Animal Crossing)
- `decomp_animal_crossing/src/static/dolphin/os/OS.c` (memset arena)
- `decomp_animal_crossing/src/static/JSystem/JKernel/JKRHeap.cpp`
- `decomp_animal_crossing/src/static/boot.c`

## Tests
See: `tests/sdk/os/os_get_arena_lo/`

