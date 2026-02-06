# OSSetArenaLo

## Signature
`void OSSetArenaLo(void *addr)`

## What we know (from decomp)
- Writes the new pointer to the global `__OSArenaLo`.
- No alignment checks; accepts any pointer value.

Evidence (MP4 SDK source): `decomp_mario_party_4/src/dolphin/os/OSArena.c`.

## Callsites (evidence)

### MP4 (Mario Party 4)
- `decomp_mario_party_4/src/game/init.c` (multiple)
- `decomp_mario_party_4/src/dolphin/demo/DEMOInit.c` (multiple)
- `decomp_mario_party_4/src/dolphin/os/OS.c:254` (BootInfo->arenaLo / __ArenaLo)
- `decomp_mario_party_4/src/Runtime.PPCEABI.H/GCN_Mem_Alloc.c` (arena alloc init)

### TP (Twilight Princess)
- `decomp_twilight_princess/src/dolphin/os/OSArena.c` (implementation)
- `decomp_twilight_princess/src/dolphin/os/OS.c:268-270` (BootInfo / stack rounding)
- `decomp_twilight_princess/src/dolphin/os/OSExec.c` (paramsWork / fixed 0x81280000)
- `decomp_twilight_princess/src/dolphin/os/OSFatal.c` (fixed 0x81400000)

### WW (Wind Waker)
- `decomp_wind_waker/src/dolphin/os/OSArena.c` (implementation)
- `decomp_wind_waker/src/dolphin/os/OS.c:262-268` (BootInfo / debugArenaLo)
- `decomp_wind_waker/src/PowerPC_EABI_Support/Runtime/Src/GCN_mem_alloc.c`

### AC (Animal Crossing)
- `decomp_animal_crossing/src/static/dolphin/os/OSArena.c` (implementation)
- `decomp_animal_crossing/src/static/dolphin/os/OS.c:244-246` (BootInfo / stack rounding)

## Tests
See: `tests/sdk/os/os_set_arena_lo/`
