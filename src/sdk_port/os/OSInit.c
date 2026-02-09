#include <stdint.h>

// Minimal OSInit used by host scenarios.
// This is not a full OSInit port (yet); it's only enough for deterministic tests.

void OSSetArenaLo(void *addr);
void OSSetArenaHi(void *addr);

#include "../gc_mem.h"

// Unique helper name: this file is sometimes compiled by textual inclusion
// into a larger "oracle" TU for PPC smoke DOLs.
static inline uint32_t osinit_load_u32be(uint32_t addr) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return 0;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

void OSInit(void) {
    // OSInit sets arenas from BootInfo if present, otherwise uses defaults.
    //
    // Reference: `decomp_mario_party_4/src/dolphin/os/OS.c` OSInit().
    //
    // BootInfo lives at OS_BASE_CACHED (0x80000000). Offsets in OSBootInfo:
    // - arenaLo @ 0x30
    // - arenaHi @ 0x34
    //
    // Note: values are stored in big-endian in emulated RAM.
    const uint32_t bootinfo_base = 0x80000000u;
    const uint32_t boot_arena_lo = osinit_load_u32be(bootinfo_base + 0x30u);
    const uint32_t boot_arena_hi = osinit_load_u32be(bootinfo_base + 0x34u);

    // Defaults match our deterministic DOL oracle for OSInit.
    const uint32_t def_lo = 0x80002000u;
    const uint32_t def_hi = 0x81700000u;

    OSSetArenaLo((void *)(uintptr_t)(boot_arena_lo ? boot_arena_lo : def_lo));
    OSSetArenaHi((void *)(uintptr_t)(boot_arena_hi ? boot_arena_hi : def_hi));
}
